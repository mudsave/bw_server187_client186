/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FRAME_WRITER_HPP
#define FRAME_WRITER_HPP


#include <stdio.h>
#include <string>
#include <vector>

#include <d3d9.h>

#include "cstdmf/stdmf.hpp"

#ifndef NO_FRAME_WRITER

/**
 *	This class maintains a thread and a queue of buffers, used to write
 *	out multiple rendered frames in the background, while running the game.
 */
class FrameWriter
{
public:
	FrameWriter();
	~FrameWriter();

	bool start(  const std::string & fileName, const RECT * pRect,
		uint nBuffers = 16 );
	void stop();

	bool started()						{ return thread_ != NULL; }

	void save( const D3DLOCKED_RECT * pLockedRect, float dTime );

private:
	static void __cdecl s_start( void * arg );
	void run();

	struct FrameHeader
	{
		int		width;
		int		height;
		float	dTime;
		int		offset;	// of data in block to fit in 4K
	};

	struct Buffer
	{
		Buffer( char * pD, long l ) : pData( pD ), len( l ) {}
		char	* pData;
		long	len;
	};

	void queueBuffer( Buffer pBuffer );
	void writeBuffer( Buffer pBuffer );

	HANDLE	thread_;

	HANDLE	freeSemaphore_;
	HANDLE	freeMutex_;
	HANDLE	saveSemaphore_;
	HANDLE	saveMutex_;
	

	RECT	area_;
	//FILE	* file_;
	class PhilGraph		*pPhilGraph_;

	struct FileHeader
	{
		int		width;
		int		height;
		int		nFrames;
		float	totalTime;
		int		version;
	} fhead_;

	typedef std::vector<Buffer>	BufferList;
	BufferList	freeList_;
	BufferList	saveList_;
};

#else // NO_FRAME_WRITER

class FrameWriter
{
public:
	FrameWriter() : thread_( false )	{ }
	~FrameWriter()						{ }

	bool start(  const std::string & fileName, const RECT * pRect,
		uint nBuffers = 16 )			{ thread_ = true; return true; }
	void stop()							{ thread_ = false; }

	bool started()						{ return thread_ != NULL; }

	void save( const D3DLOCKED_RECT * pLockedRect, float dTime )	{ }

private:
	bool thread_;
};

#endif // NO_FRAME_WRITER

#endif // FRAME_WRITER_HPP