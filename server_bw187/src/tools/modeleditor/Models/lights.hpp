/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LIGHTS_HPP
#define LIGHTS_HPP

#include "gizmo/general_properties.hpp"

typedef SmartPointer< class GeneralEditor > GeneralEditorPtr;

class GeneralLight
{
public:
	void elect();
	void expel();
	void enabled( bool v );
	bool enabled();

	virtual void setMatrix( const Matrix& matrix ) {}

protected:
	bool enabled_;
	GeneralEditorPtr editor_;
};

class AmbientLight: public GeneralLight, public ReferenceCount
{
public:

	AmbientLight();
	
	Moo::Colour colour();
	void colour( Moo::Colour v );

	Moo::Colour light();

private:

	Moo::Colour light_;
};

template <class CL> class EditorLight: public GeneralLight
{
public:

	EditorLight()
	{
		enabled_ = false;
		light_ = new CL();
		light_->colour( Moo::Colour( 1.f, 1.f, 1.f, 1.f ));
		editor_ = new GeneralEditor();
	}

	SmartPointer<CL> light() { return light_; }
	
	void position( Vector3& pos )
	{
		matrix_[3] = pos;
		matrixProxy_->setMatrix( matrix_ );
	}

	virtual void direction( Vector3& dir )
	{
		dir.normalise();

		Vector3 up( 0.f, 1.f, 0.f );
		if (fabsf( up.dotProduct( dir ) ) > 0.9f)
			up = Vector3( 0.f, 0.f, 1.f );

		Vector3 xaxis = up.crossProduct( dir );
		xaxis.normalise();

		matrix_[1] = xaxis;
		matrix_[0] = xaxis.crossProduct( dir );
		matrix_[0].normalise();
		matrix_[2] = dir;

		matrixProxy_->setMatrix( matrix_ );
	}

	virtual void setMatrix( const Matrix& matrix )
	{
		matrix_ = matrix;
		matrixProxy_->setMatrix( matrix_ );
	}

protected:

	SmartPointer<CL> light_;
	MatrixProxyPtr matrixProxy_;
	Matrix matrix_;
};

class OmniLight: public EditorLight<Moo::OmniLight>
{
public:
	OmniLight();

	void setMatrix( const Matrix& matrix )
	{
		Matrix temp = matrix;
		position( temp[3] );
	}
};

class DirLight: public EditorLight<Moo::DirectionalLight>
{
public:
	DirLight();

	void setMatrix( const Matrix& matrix )
	{
		Matrix temp = matrix;
		direction( temp[2] );
	}
};

class SpotLight: public EditorLight<Moo::SpotLight>
{
public:
	SpotLight();

	virtual void direction( Vector3& dir )
	{
		dir.normalise();

		Vector3 up( 0.f, 0.f, 1.f );
		if (fabsf( up.dotProduct( dir ) ) > 0.9f)
			up = Vector3( 1.f, 0.f, 0.f );

		Vector3 zaxis = up.crossProduct( dir );
		zaxis.normalise();

		matrix_[2] = zaxis;
		matrix_[0] = dir.crossProduct( zaxis );
		matrix_[0].normalise();
		matrix_[1] = dir;

		matrixProxy_->setMatrix( matrix_ );
	}

	void setMatrix( const Matrix& matrix )
	{
		matrix_ = matrix;
		Matrix temp = matrix;
		direction( -temp[2] );
	}
};

class Lights
{
public:

	static Lights* s_instance_;
	static Lights& instance()
	{
		return *s_instance_;
	}
	
	Lights();
	~Lights();
	
	bool newSetup();
	bool open( const std::string& name, DataSectionPtr file = NULL );
	bool save( const std::string& name = "" );
	
	void regenerateLightContainer();
	
	int numOmni() { return omni_.size(); }
	int numDir() { return dir_.size(); }
	int numSpot() { return spot_.size(); }
	
	AmbientLight*	ambient() { return &ambient_; }
	OmniLight*		omni( int i ) { return omni_[i]; }
	DirLight*		dir( int i ) { return dir_[i]; }
	SpotLight*		spot( int i ) { return spot_[i]; }

	Moo::LightContainerPtr lightContainer() { return lc_; }

	void dirty( bool val ) { dirty_ = val; }
	bool dirty() { return dirty_; }

	std::string& name() { return currName_; }

private:

	AmbientLight ambient_;
	std::vector<OmniLight*> omni_;
	std::vector<DirLight*> dir_;
	std::vector<SpotLight*> spot_;

	Moo::LightContainerPtr lc_;

	std::string currName_;
	DataSectionPtr currFile_;
		
	bool dirty_;

	void disableAllLights();
	
};

#endif // LIGHTS_HPP