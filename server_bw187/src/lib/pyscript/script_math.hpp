/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 * 	@file
 *
 * 	This file defines the PyVector class.
 *
 * 	@ingroup script
 */

#ifndef SCRIPT_MATH_HPP
#define SCRIPT_MATH_HPP


#include "pyobject_plus.hpp"
#include "script.hpp"
#include "math/matrix.hpp"


// Reference this variable to link in the Math classes.
extern int Math_token;


/**
 *	This helper class has virtual functions to provide a matrix. This
 *	assists many classes in getting out matrices from a variety of
 *	different objects.
 */
class MatrixProvider : public PyObjectPlus
{
	Py_Header( MatrixProvider, PyObjectPlus )

public:
	MatrixProvider( PyTypePlus * pType );
	virtual ~MatrixProvider() {};

	virtual PyObject * pyGetAttribute( const char * attr )
		{ return PyObjectPlus::pyGetAttribute( attr ); }
	virtual int pySetAttribute( const char * attr, PyObject * value )
		{ return PyObjectPlus::pySetAttribute( attr, value ); }
	virtual int pyDelAttribute( const char * attr )
		{ return PyObjectPlus::pyDelAttribute( attr ); }

	virtual void tick( float /*dTime*/ ) { }
	virtual void matrix( Matrix & m ) const = 0;

	virtual void getYawPitch( float& yaw, float& pitch) const
	{
		Matrix m;
		this->matrix(m);
		yaw = m.yaw();
		pitch = m.pitch();
	}
};

typedef SmartPointer<MatrixProvider> MatrixProviderPtr;

PY_SCRIPT_CONVERTERS_DECLARE( MatrixProvider )

/**
 *	This class allows scripts to create and manipulate their own matrices
 */
class PyMatrix : public MatrixProvider, public Matrix
{
	Py_Header( PyMatrix, MatrixProvider )


public:
	PyMatrix( PyTypePlus * pType = &s_type_ ) :
		MatrixProvider( pType ), Matrix( Matrix::identity )	{ }
	void set( const Matrix & m )			{ *static_cast<Matrix*>(this) = m; }
	virtual void matrix( Matrix & m ) const	{ m = *this; }

	PyObject *	pyGetAttribute( const char * attr );
	int			pySetAttribute( const char * attr, PyObject * value );

	void set( MatrixProviderPtr mpp )
		{ Matrix m; mpp->matrix( m ); this->set( m ); }
	PY_AUTO_METHOD_DECLARE( RETVOID, set, NZARG( MatrixProviderPtr, END ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, setZero, END )
	PY_AUTO_METHOD_DECLARE( RETVOID, setIdentity, END )

	PY_AUTO_METHOD_DECLARE( RETVOID, setScale, ARG( Vector3, END ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, setTranslate, ARG( Vector3, END ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, setRotateX, ARG( float, END ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, setRotateY, ARG( float, END ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, setRotateZ, ARG( float, END ) )
	void setRotateYPR( const Vector3 & ypr )
		{ this->setRotate( ypr[0], ypr[1], ypr[2] ); }
	PY_AUTO_METHOD_DECLARE( RETVOID, setRotateYPR, ARG( Vector3, END ) )

	void preMultiply( MatrixProviderPtr mpp )
		{ Matrix m; mpp->matrix( m ); this->Matrix::preMultiply( m ); }
	PY_AUTO_METHOD_DECLARE( RETVOID, preMultiply,
		NZARG( MatrixProviderPtr, END ) )
	void postMultiply( MatrixProviderPtr mpp )
		{ Matrix m; mpp->matrix( m ); this->Matrix::postMultiply( m ); }
	PY_AUTO_METHOD_DECLARE( RETVOID, postMultiply,
		NZARG( MatrixProviderPtr, END ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, invert, END )
	PY_AUTO_METHOD_DECLARE( RETVOID, lookAt, ARG( Vector3,
		ARG( Vector3, ARG( Vector3, END ) ) ) )

	void setElement( int col, int row, float value) { (*this)[col&3][row&3] = value; }
	PY_AUTO_METHOD_DECLARE( RETVOID, setElement, ARG( uint, ARG( uint, ARG( float, END ) ) ) )

	float get( int col, int row ) const { return (*this)[col&3][row&3]; }
	PY_AUTO_METHOD_DECLARE( RETDATA, get, ARG( uint, ARG( uint, END ) ) )
	PY_RO_ATTRIBUTE_DECLARE( getDeterminant(), determinant )

	PY_AUTO_METHOD_DECLARE( RETDATA, applyPoint, ARG( Vector3, END ) )
	Vector4 applyV4Point( const Vector4 & p )
		{ Vector4 ret; this->applyPoint( ret, p ); return ret; }
	PY_AUTO_METHOD_DECLARE( RETDATA, applyV4Point, ARG( Vector4, END ) )
	PY_AUTO_METHOD_DECLARE( RETDATA, applyVector, ARG( Vector3, END ) )

	const Vector3 & applyToAxis( uint axis )
		{ return this->applyToUnitAxisVector( axis & 3 ); }
	PY_AUTO_METHOD_DECLARE( RETDATA, applyToAxis, ARG( uint, END ) )
	PY_AUTO_METHOD_DECLARE( RETDATA, applyToOrigin, END )

	PY_RO_ATTRIBUTE_DECLARE( isMirrored(), isMirrored )

	PY_AUTO_METHOD_DECLARE( RETVOID, orthogonalProjection,
		ARG( float, ARG( float, ARG( float, ARG( float, END ) ) ) ) )
	PY_AUTO_METHOD_DECLARE( RETVOID, perspectiveProjection,
		ARG( float, ARG( float, ARG( float, ARG( float, END ) ) ) ) )

	const Vector3 & translation() const
		{ return this->Matrix::applyToOrigin(); }
	void translation( const Vector3 & v )
		{ this->Matrix::translation( v ); }
	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( Vector3, translation, translation )

	PY_RO_ATTRIBUTE_DECLARE( yaw(), yaw )
	PY_RO_ATTRIBUTE_DECLARE( pitch(), pitch )
	PY_RO_ATTRIBUTE_DECLARE( roll(), roll )

	static PyObject * _pyCall( PyObject * self, PyObject * args, PyObject * kw)
		{ return _py_get( self, args, kw ); }

	PY_METHOD_DECLARE( py___getstate__ )
	PY_METHOD_DECLARE( py___setstate__ )

	PY_FACTORY_DECLARE()
};


/**
 *	This class implements a Vector class for use in Python
 *
 * 	@ingroup script
 */
template <class V> class PyVector : public PyObjectPlus
{
	Py_Header( PyVector< V >, PyObjectPlus )

public:
	typedef V Member;

	PyVector( bool isReadOnly, PyTypePlus * pType ) :
		PyObjectPlus( pType ), isReadOnly_( isReadOnly ) {}
	virtual ~PyVector() { };

	virtual V getVector() const = 0;
	virtual bool setVector( const V & v ) = 0;

	bool isReadOnly() const		{ return isReadOnly_; }
	virtual bool isReference() const	{ return false; }

	PyObject * pyGet_x();
	int pySet_x( PyObject * value );

	PyObject * pyGet_y();
	int pySet_y( PyObject * value );

	PyObject * pyGet_z();
	int pySet_z( PyObject * value );

	PyObject * pyGet_w();
	int pySet_w( PyObject * value );

// 	PY_FACTORY_DECLARE()
	PyObject * pyNew( PyObject * args );

	// PyObjectPlus overrides
	PyObject *			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );

	PY_UNARY_FUNC_METHOD( pyStr )

	// Used for as_number
	// PY_BINARY_FUNC_METHOD( py_add )
	// PY_BINARY_FUNC_METHOD( py_subtract )
	// PY_BINARY_FUNC_METHOD( py_multiply )
	static PyObject * _py_add( PyObject * a, PyObject * b );
	static PyObject * _py_subtract( PyObject * a, PyObject * b );
	static PyObject * _py_multiply( PyObject * a, PyObject * b );
	static PyObject * _py_divide( PyObject * a, PyObject * b );

	PY_UNARY_FUNC_METHOD( py_negative )
	PY_UNARY_FUNC_METHOD( py_positive )

	PY_INQUIRY_METHOD( py_nonzero )

	PY_BINARY_FUNC_METHOD( py_inplace_add )
	PY_BINARY_FUNC_METHOD( py_inplace_subtract )
	PY_BINARY_FUNC_METHOD( py_inplace_multiply )
	PY_BINARY_FUNC_METHOD( py_inplace_divide )

	// Used for as_sequence
	PY_INQUIRY_METHOD( py_sq_length )
	PY_INTARG_FUNC_METHOD( py_sq_item )
	PY_INTINTARG_FUNC_METHOD( py_sq_slice )
	PY_INTOBJARG_PROC_METHOD( py_sq_ass_item )

	// Script methods
	PY_METHOD_DECLARE( py_set )
	PY_METHOD_DECLARE( py_scale )
	PY_METHOD_DECLARE( py_dot )
	PY_METHOD_DECLARE( py_normalise )

	PY_METHOD_DECLARE( py_tuple )
	PY_METHOD_DECLARE( py_list )

	PY_METHOD_DECLARE( py_cross2D )

	PY_METHOD_DECLARE( py_distTo )
	PY_METHOD_DECLARE( py_distSqrTo )

	PY_METHOD_DECLARE( py_flatDistTo )
	PY_METHOD_DECLARE( py_flatDistSqrTo )

	// PY_METHOD_DECLARE( py___reduce__ )
	PY_METHOD_DECLARE( py___getstate__ )
	PY_METHOD_DECLARE( py___setstate__ )

	PY_RO_ATTRIBUTE_DECLARE( this->getVector().length(), length )
	PY_RO_ATTRIBUTE_DECLARE( this->getVector().lengthSquared(), lengthSquared )

	PY_RO_ATTRIBUTE_DECLARE( this->isReadOnly(), isReadOnly )
	PY_RO_ATTRIBUTE_DECLARE( this->isReference(), isReference )

	PyObject * pyGet_yaw();
	PyObject * pyGet_pitch();
	PY_RO_ATTRIBUTE_SET( yaw )
	PY_RO_ATTRIBUTE_SET( pitch )

	static PyObject * _tp_new( PyTypeObject * pType,
			PyObject * args, PyObject * kwargs );

private:
	PyVector( const PyVector & toCopy );
	PyVector & operator=( const PyVector & toCopy );

	bool safeSetVector( const V & v );

	bool isReadOnly_;

	static const int NUMELTS;
};


/**
 *	This class implements a PyVector that has the vector attribute as a member.
 */
template <class V>
class PyVectorCopy : public PyVector< V >
{
public:
	PyVectorCopy( bool isReadOnly = false,
			PyTypePlus * pType = &PyVector<V>::s_type_ ) :
		PyVector<V>( isReadOnly, pType ), v_( V::zero() ) {}

	PyVectorCopy( const V & v, bool isReadOnly = false,
			PyTypePlus * pType = &PyVector<V>::s_type_ ) :
		PyVector<V>( isReadOnly, pType ), v_( v ) {}

	virtual V getVector() const
	{
		return v_;
	}

	virtual bool setVector( const V & v )
	{
		v_ = v;
		return true;
	}

private:
	V v_;
};


/**
 *	This class implements a PyVector that takes its value from a member of
 *	another Python object. This is useful for exposing a Vector2, Vector3 or
 *	Vector4 member C++ class and allowing the underlying member to be modified
 *	when the Python Vector is modified.
 */
template <class V>
class PyVectorRef : public PyVector< V >
{
public:
	PyVectorRef( PyObject * pOwner, V * pV, bool isReadOnly = false,
			PyTypePlus * pType = &PyVector<V>::s_type_ ) :
		PyVector<V>( isReadOnly, pType ), pOwner_( pOwner ), pV_( pV ) {}

	virtual V getVector() const
	{
		return *pV_;
	}

	virtual bool setVector( const V & v )
	{
		*pV_ = v;

		return true;
	}

	virtual bool isReference() const	{ return true; }

private:
	PyObjectPtr pOwner_;
	V * pV_;
};


/**
 *	This class extends PyVector4 is add colour attributes that are aliases for
 *	the PyVector4 attributes.
 */
class PyColour : public PyVector< Vector4 >
{
	Py_Header( PyColour, PyVector< Vector4 > )

	virtual PyObject * pyGetAttribute( const char * attr );
	virtual int pySetAttribute( const char * attr, PyObject * value );

public:
	PyColour( bool isReadOnly = false, PyTypePlus * pType = &s_type_ ) :
		PyVector< Vector4 >( isReadOnly, pType ) {}
};


//-----------------------------------------------------------------------------
// Section: Vector4Provider
//-----------------------------------------------------------------------------

/**
 *	This abstract class provides a Vector4
 */
class Vector4Provider : public PyObjectPlus
{
	Py_Header( Vector4Provider, PyObjectPlus )
public:
	Vector4Provider( PyTypePlus * pType = &s_type_ );
	virtual ~Vector4Provider() {};

	virtual PyObject * pyGetAttribute( const char * attr );
	virtual int pySetAttribute( const char * attr, PyObject * value );

	PyObject * pyGet_value();
	PY_RO_ATTRIBUTE_SET( value );

	virtual void tick( float /*dTime*/ ) { }
	virtual void output( Vector4 & val ) = 0;
};

typedef SmartPointer<Vector4Provider> Vector4ProviderPtr;
typedef std::pair<float,Vector4ProviderPtr> Vector4Animation_Keyframe;
namespace Script
{
	int setData( PyObject * pObject, Vector4Animation_Keyframe & rpVal,
		const char * varName = "" );
	PyObject * getData( const Vector4Animation_Keyframe & pVal );
}

PY_SCRIPT_CONVERTERS_DECLARE( Vector4Provider )


/**
 *	This is the basic concrete Vector4 provider.
 */
class Vector4Basic : public Vector4Provider
{
	Py_Header( Vector4Basic, Vector4Provider )
public:
	Vector4Basic( const Vector4 & val = Vector4(0,0,0,0), PyTypePlus * pType = &s_type_ );

	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PY_RW_ATTRIBUTE_DECLARE( value_, value )

	virtual void output( Vector4 & val ) { val = value_; }
	void set( const Vector4 & val )		{ value_ = val; }
	Vector4* pValue()					{ return &value_; }
private:
	Vector4 value_;
};





#endif // SCRIPT_MATH_HPP
