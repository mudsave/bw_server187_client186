/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SCALE_GIZMO_HPP
#define SCALE_GIZMO_HPP

#include "gizmo_manager.hpp"
#include "solid_shape_mesh.hpp"
#include "input/input.hpp"
#include "coord_mode_provider.hpp"

typedef SmartPointer<class MatrixProxy> MatrixProxyPtr;


class ScaleShapePart;

/**
 *	This class implements a gizmo that allows interactive
 *	editing of the scale part of a matrix.
 *
 *	The gizmo represents itself as a ring that by default
 *	appears only when the ALT key is held down, although this
 *	can be overridden by the creator of the object.
 */
class ScaleGizmo : public Gizmo, public Aligned
{
	class ScaleFloatProxy : public FloatProxy
	{
		float data_;
	public:
		virtual float EDCALL get() const
		{
			return data_;
		}
		virtual void EDCALL set( float f, bool transient )
		{
			data_ = f;
		}
		ScaleFloatProxy()
		{
			incRef();incRef();incRef();incRef();
		}
	}
	ScaleFloatProxy_;
	class ScaleMatrixProxy : public MatrixProxy
	{
		Matrix data_;
	public:
		virtual void EDCALL getMatrix( Matrix & m, bool world = true )
		{
			m = data_;
		}
		virtual void EDCALL getMatrixContext( Matrix & m )
		{
			m = data_;
		}
		virtual void EDCALL getMatrixContextInverse( Matrix & m )
		{
			m.invert( data_ );
		}
		virtual bool EDCALL setMatrix( const Matrix & m )
		{
			data_ = m;
			return true;
		}
		virtual void EDCALL recordState(){}
		virtual bool EDCALL commitState( bool revertToRecord = false, bool addUndoBarrier = true )
		{
			return true;
		}

		virtual bool EDCALL hasChanged()
		{
			return true;
		}
		ScaleMatrixProxy()
		{
			incRef();incRef();incRef();incRef();
		}
	}
	ScaleMatrixProxy_;
public:
	ScaleGizmo( MatrixProxyPtr pMatrix, int enablerModifier = MODIFIER_ALT, float scaleSpeedFactor = 1.f );
	~ScaleGizmo();

	bool draw( bool force );
	bool intersects( const Vector3& origin, const Vector3& direction, float& t );
	void click( const Vector3& origin, const Vector3& direction );
	void rollOver( const Vector3& origin, const Vector3& direction );
	Matrix getCoordModifier() const;

protected:
	Matrix	objectTransform() const;	
	Matrix	gizmoTransform();

	bool					active_;
	SolidShapeMesh			selectionMesh_;
	Moo::VisualPtr			drawMesh_;
	ScaleShapePart *		currentPart_;
	MatrixProxyPtr			pMatrix_;
	Moo::Colour				lightColour_;
	int						enablerModifier_;
	float					scaleSpeedFactor_;

	ScaleGizmo( const ScaleGizmo& );
	ScaleGizmo& operator=( const ScaleGizmo& );
};

#endif // SCALE_GIZMO_HPP
