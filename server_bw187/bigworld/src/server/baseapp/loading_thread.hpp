/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASEAPP_LOADING_THREAD_HPP
#define BASEAPP_LOADING_THREAD_HPP

#include "worker_thread.hpp"

#include <list>

class TickedWorkerJob : public WorkerJob
{
public:
	static void tickJobs();

protected:
	TickedWorkerJob();
	virtual ~TickedWorkerJob() {}

	virtual bool tick() = 0;

private:
	typedef std::list< TickedWorkerJob * > Jobs;
	static Jobs jobs_;
};

#endif // BASEAPP_LOADING_THREAD_HPP
