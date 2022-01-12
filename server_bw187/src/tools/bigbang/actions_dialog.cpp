/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "bigbang2.h"
#include "actions_dialog.hpp"
#include "entitydef/data_description.hpp"
#include "entitydef/entity_description.hpp"
#include "entitydef/data_types.hpp"
#include "pyscript/script.hpp"
#include "worldeditor/big_bang.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// ActionsDialog dialog

IMPLEMENT_DYNAMIC(ActionsDialog, CDialog)
ActionsDialog::ActionsDialog(EditorChunkEntity * entity, CWnd* pParent /*=NULL*/)
	: CDialog(ActionsDialog::IDD, pParent)
	, edEntity_( entity )
{
}

ActionsDialog::~ActionsDialog()
{
}

BOOL ActionsDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	const EntityDescription * pType = edEntity_->getTypeDesc();
	for (uint i=0; i < pType->propertyCount(); i++)
	{
		DataDescription * pDD = pType->property(i);
		if (pDD->typeAlias() == "ACTIONS")
		{
			actionIndexes_.push_back( i );
		}
	}

	if (actionIndexes_.empty())
		return TRUE;

	events_.ResetContent();
	for (std::vector<uint>::iterator it = actionIndexes_.begin();
		it != actionIndexes_.end();
		it++)
	{
		uint index = *it;

		DataDescription * pDD = pType->property(index);
		events_.AddString( pDD->name().c_str() );
	}

	// load the possible actions
	actions_.ResetContent();
	const std::vector<std::string> & actionNames = EditorEntityType::instance().getActionNames();
	for (std::vector<std::string>::const_iterator it = actionNames.begin();
		it != actionNames.end();
		it++)
	{
		actions_.AddString( (*it).c_str() );
	}

	loadCurrentActions();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void ActionsDialog::loadCurrentActions()
{
	currentActions_.clear();
	listIndexMap_.clear();

	// read in the events from the entities properties section
	DataSectionPtr entitySection = edEntity_->pOwnSect();
	DataSectionPtr propertiesSection = entitySection->openSection( "properties", false );
	if (!propertiesSection)
		return;

	const EntityDescription * pType = edEntity_->getTypeDesc();

	if (actionIndexes_.empty())
		return;

	for (std::vector<uint>::iterator it = actionIndexes_.begin();
		it != actionIndexes_.end();
		it++)
	{
		uint index = *it;

		std::string eventName = pType->property(index)->name();

//		currentActions_.AddPropItem( new GroupPropertyItem( eventName, 0 ) );

		// read in the information from the section
		DataSectionPtr eventSection = propertiesSection->openSection( eventName );
		if (!eventSection)
			continue;

		for (DataSectionIterator eventIt = eventSection->begin();
			eventIt != eventSection->end();
			eventIt++)
		{
			DataSectionPtr actSection = *eventIt;
			std::string actName = actSection->sectionName();

			std::string argStr = "";
			for (DataSectionIterator argIt = actSection->begin();
				argIt != actSection->end();
				argIt++)
			{
				if (argStr.empty())
					argStr += " [";
				else
					argStr += ", ";

				DataSectionPtr argSection = *argIt;

				std::string argName = argSection->sectionName();
				std::string argVal = argSection->asString();
				argStr += argName + ":" + argVal;
			}
			if (!argStr.empty())
				argStr += "]";

			
			LabelPropertyItem * newItem = new LabelPropertyItem( (actName + argStr).c_str() );
			newItem->setGroup( eventName );
			int index = currentActions_.AddPropItem( newItem );
			
			listIndexMap_[index] = EventActionSections( eventSection, actSection );
		}
	}
}

void ActionsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DIALOG_ACTION_CURRENT, currentActions_);
	DDX_Control(pDX, IDC_DIALOG_ACTION_EVENT, events_);
	DDX_Control(pDX, IDC_DIALOG_ACTION_ACTION, actions_);
}


BEGIN_MESSAGE_MAP(ActionsDialog, CDialog)
	ON_BN_CLICKED(IDC_DIALOG_ACTION_DELETE, OnBnClickedActionDelete)
	ON_BN_CLICKED(IDC_DIALOG_ACTION_ADD, OnBnClickedActionAdd)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDC_DIALOG_ACTION_CHANGE, OnBnClickedActionChangeEvent)
END_MESSAGE_MAP()


// ActionsDialog message handlers

void ActionsDialog::OnBnClickedActionDelete()
{
	int index = currentActions_.GetCurSel();
	if (index == LB_ERR)
		return;

	EventActionSections sectPair = listIndexMap_[ index ];
	DataSectionPtr eventSection = sectPair.first;
	DataSectionPtr actSection = sectPair.second;
	eventSection->delChild( actSection );

	// update the list
	loadCurrentActions();
}

void ActionsDialog::OnBnClickedActionAdd()
{
	// tricky - add to the section directly, then on ok, reselect the item!
	int eventSel = events_.GetCurSel();
	if (eventSel == CB_ERR)
		return;
	CString eventName;
	events_.GetLBText( eventSel, eventName );

	int actionSel = actions_.GetCurSel();
	if (actionSel == CB_ERR)
		return;
	CString actionName;
	actions_.GetLBText( actionSel, actionName ); 

	DataSectionPtr section = edEntity_->pOwnSect();
	DataSectionPtr propertiesSection = section->openSection( "properties", true );
	DataSectionPtr eventSection = propertiesSection->openSection( eventName.GetBuffer(), true );
	DataSectionPtr actionSection = eventSection->newSection( "item" );
	actionSection->setString( actionName.GetBuffer() );

	// update the current action list
	loadCurrentActions();

	BigBang::instance().changedChunk( edEntity_->chunk() );
}

void ActionsDialog::OnBnClickedActionChangeEvent()
{
	int index = currentActions_.GetCurSel();
	if (index == LB_ERR)
		return;

	int eventSel = events_.GetCurSel();
	if (eventSel == CB_ERR)
		return;
	CString eventName;
	events_.GetLBText( eventSel, eventName );

	if (listIndexMap_.find( index ) == listIndexMap_.end())
		return;

	// remove from old
	EventActionSections sectPair = listIndexMap_[ index ];
	DataSectionPtr eventSection = sectPair.first;
	DataSectionPtr actSection = sectPair.second;
	eventSection->delChild( actSection );

	// add to new
	DataSectionPtr entitySection = edEntity_->pOwnSect();
	DataSectionPtr propertiesSection = entitySection->openSection( "properties" );
	MF_ASSERT(propertiesSection);

	DataSectionPtr newEventSection = propertiesSection->openSection( eventName.GetBuffer(), true );
	DataSectionPtr newActSection = newEventSection->newSection( actSection->sectionName() );
	MF_ASSERT(actSection);
	for (DataSectionIterator argIt = actSection->begin();
		argIt != actSection->end();
		argIt++)
	{
		DataSectionPtr argSection = *argIt;
		DataSectionPtr newArgSection = newActSection->newSection( argSection->sectionName() );
		newArgSection->setString( argSection->asString() );
	}

	// update the list
	loadCurrentActions();
}

void ActionsDialog::OnBnClickedOk()
{
	// reselect the edEntity_ to update the changes
	// reset the property pane
	edEntity_->edLoad( edEntity_->typeGet(), edEntity_->pOwnSect() );
	PyObject* pModule = PyImport_ImportModule( "BorlandDirector" );
	if (pModule != NULL)
	{
		PyObject* pScriptObject = PyObject_GetAttr( pModule, Py_BuildValue( "s", "bd" ) );

		if (pScriptObject != NULL)
		{
			Script::call(
				PyObject_GetAttrString( pScriptObject, "resetSelUpdate" ),
				PyTuple_New(0),
				"ActionsDialog::OnBnClickedOk");
				
			Py_DECREF( pScriptObject );
		}
		Py_DECREF( pModule );
	}

	OnOK();
}
