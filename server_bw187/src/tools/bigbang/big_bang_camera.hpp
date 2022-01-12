/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BIG_BANG_CAMERA_HPP
#define BIG_BANG_CAMERA_HPP

class ChunkItem;
typedef SmartPointer<ChunkItem> ChunkItemPtr;

/**
 *	Container of cameras
 *	todo:  pythonise
 */
#include "common/base_camera.hpp"

 class BigBangCamera : public ReferenceCount
{
public:
	BigBangCamera();
	~BigBangCamera();
	
	static BigBangCamera& instance() { return *s_instance; }

	enum CameraType
	{
		CT_MouseLook = 0,
		CT_Orthographic = 1
	};

	BaseCamera & currentCamera() { return *cameras_[currentCameraType_]; }
	BaseCamera & camera( CameraType type ) { return *cameras_[type]; }

	void changeToCamera( CameraType type );

	void zoomExtents( ChunkItemPtr item, float sizeFactor = 1.2 );

	void respace( const Matrix& view );

private:
	typedef SmartPointer<BaseCamera> BaseCameraPtr;
	std::vector<BaseCameraPtr> cameras_;
	CameraType currentCameraType_;
	static SmartPointer<BigBangCamera> s_instance;

	Vector3 getCameraLocation();
};


#endif // BIG_BANG_CAMERA_HPP
