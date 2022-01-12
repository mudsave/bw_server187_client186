/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WORKER_THREAD_HPP
#define WORKER_THREAD_HPP

#include <vector>

#include "cstdmf/concurrency.hpp"
#include "network/interfaces.hpp"
#include "network/nub.hpp"

class WorkerThreadMgr;	// forward declaration


/**
 *	This function sleeps for the specified amount of time.
 */
inline void threadSleep( int usecs )
{
#ifdef _WIN32
		DWORD msecs = usecs/1000;
		if (msecs == 0)
			msecs = 1;
		Sleep(msecs);
#else
		// __kyl__ (2/6/2005) Read in some article that sleep() may have a
		// problem in multi-threaded environment. Using select() instead.
		timeval	timeout;
		timeout.tv_sec = usecs/1000000;
		timeout.tv_usec = usecs%1000000;
		select( 0, NULL, NULL, NULL, &timeout );
#endif
}


/**
 *	This utility class destroy the objects pointed to by pointers stored
 *	in a std container.
 */
template <class CONTAINER>
struct auto_container
{
	CONTAINER	container;

	auto_container()	{}
	~auto_container()	{	clearContainer( container );	}

	static void clearContainer( CONTAINER& c )
	{
		for ( typename CONTAINER::iterator i = c.begin();
			i < c.end(); ++i )
			delete *i;
		c.clear();
	}

private:
	// Not copy-able.
	auto_container( const auto_container& );
	auto_container& operator=( auto_container& );
};



/**
 *	This class creates a separate thread that waits for work to be handed to
 *	it.
 */
class WorkerThread
{
public:
	/**
	 *	WorkerThread accepts work in the form of an ITask derived class.
	 */
	struct ITask
	{
		// This method will be run in a separate thread.
		virtual void run() = 0;
		// This method will be called from the parent thread (not the worker
		// thread) when the run() method has completed.
		virtual void onRunComplete() = 0;
	};

private:
	struct ThreadData
	{
		SimpleSemaphore	workSema;	// Is "pushed" by parent thread when
									// there is work available.
		SimpleSemaphore	readySema;	// Is "pushed" by child thread when
									// it is ready to do work.
		WorkerThreadMgr&	mgr;	// Manager coordinates activity with main
									// thread
		ITask*			pTask;		// The work to do. NULL to terminate worker
									// thread.

		ThreadData(WorkerThreadMgr& threadMgr)
			: workSema(), readySema(), mgr(threadMgr), pTask(NULL)
		{
			readySema.push();	// child thread starts in ready state.
		}
	};
	ThreadData		threadData_;
	SimpleThread	thread_;

public:
	WorkerThread(WorkerThreadMgr& mgr);
	~WorkerThread();

	bool doTask( ITask& task )	{	return doTaskImpl(&task);	}

private:
	// Not copy-able
	WorkerThread(const WorkerThread&);
	WorkerThread& operator=(const WorkerThread&);

	bool doTaskImpl( ITask* task );
	static void threadMainLoop( void* arg );
};



/**
 *	This class is used for the coordination of worker threads and the parent
 *	thread which spawned them.
 */
class WorkerThreadMgr : public Mercury::Nub::IOpportunisticPoller,
                        public Mercury::TimerExpiryHandler
{
	typedef std::vector< WorkerThread::ITask* > CompletedTasks;

	Mercury::Nub&	nub_;
	int				timerID_;
	CompletedTasks	completedTasks_;
	SimpleMutex		completedTasksLock_;

public:
	WorkerThreadMgr( Mercury::Nub& nub );
	virtual ~WorkerThreadMgr();

	int processCompletedTasks();

	bool waitForTaskCompletion( int numTasks, int timeoutMicroSecs = -1 );

	// Called by worker threads.
	void onTaskComplete( WorkerThread::ITask& task );

	// Nub::IOpportunisticPoller overrides
	virtual void poll();

	// Mercury::TimerExpiryHandler overrides
	virtual int handleTimeout( int id, void * arg );

private:
	// Not copy-able.
	WorkerThreadMgr( const WorkerThreadMgr& );
	WorkerThreadMgr& operator=( const WorkerThreadMgr& );

#ifdef WORKERTHREAD_SELFTEST
		void selfTest();
#endif
};



/**
 *	This class implements a pool of worker threads.
 */
class WorkerThreadPool
{
	// An item in our pool.
	class PoolItem : public WorkerThread::ITask
	{
		WorkerThreadPool&		pool_;		// owner
		WorkerThread			thread;		// thread that will do work
		WorkerThread::ITask*	pOrigTask_;	// The task that the thread is running.

	public:
		PoolItem( WorkerThreadMgr& mgr, WorkerThreadPool& pool )
			: pool_(pool), thread(mgr)
		{}
		virtual ~PoolItem() {}

		bool doTask( WorkerThread::ITask& task )
		{
			pOrigTask_ = &task;
			// Ask thread to run us so that we can notify our pool when we
			// finish running.
			return thread.doTask(*this);
		}

		// WorkerThread::ITask overrides
		virtual void run();
		virtual void onRunComplete();
	};

	typedef std::vector< PoolItem* > PoolItems;

	WorkerThreadMgr&			mgr_;
	auto_container< PoolItems >	threads_;
	PoolItems					freeThreads_;

public:
	WorkerThreadPool( WorkerThreadMgr& mgr, int numThreads );

	bool doTask( WorkerThread::ITask& task );
	static void doTaskInCurrentThread( WorkerThread::ITask& task );

	int getNumFreeThreads() const	{	return int(freeThreads_.size());	}
	int getNumBusyThreads() const	{	return int(threads_.container.size()) - getNumFreeThreads();	}

	bool waitForOneTask( int timeoutMicroSecs = -1 );
	bool waitForAllTasks( int timeoutMicroSecs = -1 );

	// Called by PoolItem.
	void onTaskComplete( PoolItem& poolItem );
};

#endif // WORKER_THREAD_HPP
