/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MESH_GUI_ADAPTOR_HPP
#define MESH_GUI_ADAPTOR_HPP

#pragma warning( disable:4786 )

#include "simple_gui_component.hpp"
#include "duplo/py_attachment.hpp"

namespace Moo
{
	class LightContainer;
	typedef SmartPointer<LightContainer>	LightContainerPtr;
};

	/*~ class GUI.MeshGUIAdaptor
	 *
	 *	This class is a basic adaptor between SimpleGUIComponent and PyAttachment.
	 *
	 *	If you wish to display pyAttachments in the GUI scene
	 *	then use this class as a container for your attachment, and attach the desired
	 *	object onto it.
	 *
	 *		eg,	guiSomeImage = GUI.Simple(imageFile)
	 *			guiSomeAttachment = BigWorld.Model(modelFile)
	 *			guiMesh = GUI.MeshAdaptor()
	 *			guiMesh.attachment = guiSomeAttachment
	 *			guiMesh.transform = someMatrixProvider
	 *			guiSomeImage.addChild(guiMesh)
	 *			GUI.addRoot(guiSomeImage)
	 *
	 *		Model will now be displayed in GUI.  someMatrixProvider can be updated to change orientation of Model in 
	 *		Menu, plus Model can have animations performed on it like any other...
	 */
/**
 *	This class is a basic adaptor between SimpleGUIComponent and PyAttachment.
 *
 *	If you wish to display pyAttachments in the GUI scene
 *	then use this class as a container for your attachment, and attach the desired
 *	object onto it.
 */
class MeshGUIAdaptor : public SimpleGUIComponent, MatrixLiaison
{
	Py_Header( MeshGUIAdaptor, SimpleGUIComponent )
	
public:
	MeshGUIAdaptor( PyTypePlus * pType = &s_type_ );
	virtual ~MeshGUIAdaptor();

	//These are SimpleGUIComponent methods
	virtual void update( float dTime );
	virtual void draw( bool overlay = true );
	virtual void addAsSortedDrawItem();

	SmartPointer<PyAttachment>	attachment() const	{ return attachment_; }
	void attachment( SmartPointer<PyAttachment> );

	//These are MatrixLiason Methods
	virtual const Matrix & getMatrix() const;
	virtual bool setMatrix( const Matrix & m )	{ return false; }

	//This is the python interface
	PyObject *	pyGetAttribute( const char * attr );
	int			pySetAttribute( const char * attr, PyObject * value );

	PY_FACTORY_DECLARE()

	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SmartPointer<PyAttachment>, attachment, attachment )
	PY_RW_ATTRIBUTE_DECLARE( transform_, transform )

protected:
	SmartPointer<PyAttachment>	attachment_;
	MatrixProviderPtr			transform_;
	static Moo::LightContainerPtr	s_lighting_;

	COMPONENT_FACTORY_DECLARE( MeshGUIAdaptor() )
};

#ifdef CODE_INLINE
#include "mesh_gui_adaptor.ipp"
#endif

#endif