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

#include "romp/super_model.hpp"

#include "resmgr/xml_section.hpp"

#include "undo_redo.hpp"

#include "romp/geometrics.hpp"

#include "utilities.hpp"

#include "mutant.hpp"

DECLARE_DEBUG_COMPONENT2( "Mutant_Action", 0 )

ActionInfo::ActionInfo() {}

ActionInfo::ActionInfo(
	DataSectionPtr cData,
	DataSectionPtr cModel,
	SuperModelActionPtr cAction
):
	data (cData),
	model (cModel),
	action (cAction)
{}

bool Mutant::hasActs( const std::string& modelPath )
{
	if (models_.find( modelPath ) == models_.end()) return false;

	DataSectionPtr act = models_[ modelPath ]->openSection("action");

	return !!act;
}

StringPair Mutant::createAct( const StringPair& actID, const std::string& animName, const StringPair& afterAct )
{
	StringPair new_act = actID;
	
	//Lets get a unique name
	char buf[256];
	int id = 1;
	while (actions_.find(new_act) != actions_.end())
	{
		id++;
		sprintf(buf, "%s %d", actID.first.c_str(), id);
		new_act.first = std::string(buf);
	}

	UndoRedo::instance().add( new UndoRedoOp( 0, models_[actID.second] ));
	UndoRedo::instance().barrier( "Adding a new action", false );

	int index = models_[actID.second]->getIndex( actions_[afterAct].data );

	index++; // Since we want to insert after the selection.
	
	DataSectionPtr newAct = models_[actID.second]->insertSection( "action", index );
	newAct->writeString( "name", new_act.first );
	newAct->writeString( "animation", animName );

	dirty( models_[actID.second] );

	reloadAllLists();

	return new_act;
}

void Mutant::removeAct( const StringPair& actID )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].model ));
	UndoRedo::instance().barrier( "Removing an action", false );

	actions_[actID].model->delChild( actions_[actID].data );

	dirty( actions_[actID].model );

	reloadAllLists();
}

void Mutant::swapActions( const std::string& what, const StringPair& actID, const StringPair& act2ID, bool reload /* = true*/ )
{
	//First make sure the first action exists
	if (actions_.find(actID) == actions_.end())
		return;
	
	//First make sure the second action exists
	if (actions_.find(act2ID) == actions_.end())
		return;

	DataSectionPtr data = actions_[actID].data;
	DataSectionPtr data2 = actions_[act2ID].data;
	
	UndoRedo::instance().add( new UndoRedoOp( 0, data ));
	UndoRedo::instance().add( new UndoRedoOp( 0, data2 ));
	UndoRedo::instance().barrier( what + " an action", false );

	DataSectionPtr temp = BWResource::openSection( "temp.xml", true );

	temp->copy( data );
	data->copy( data2 );
	data2->copy( temp );

	dirty( actions_[actID].model );

	if (reload)
	{
		reloadAllLists();
	}
}

void Mutant::setAct( const StringPair& actID )
{
	playing_ = false;
	animMode_ = false;
	clearAnim_ = true;
	currAct_ = actID;
	recreateFashions();
}

std::string Mutant::actName( const StringPair& actID )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return "";

	return actions_[actID].data->readString( "name", "" );
}

bool Mutant::actName( const StringPair& actID, const std::string& actName )
{
	StringPair new_act = actID;
	new_act.first = actName;
	
	if (actID.first == actName)
		return true;

	//First make sure the actation exists
	if (actions_.find(actID) == actions_.end())
		return false;

	//If the new actation name is already used
	if (actions_.find(new_act) != actions_.end())
		return false;
		
	//Rename the actation std::map element
	actions_[new_act] = actions_[actID];
    actions_.erase(actID);

	//Now we must adjust the tree list
	for (unsigned i=0; i<actList_.size(); i++)
	{
		if (actList_[i].first.second == actID.second)
		{
			for (unsigned j=0; j<actList_[i].second.size(); j++)
			{
				if (actList_[i].second[j] == actID.first)
				{
					actList_[i].second[j] = actName;
				}
			}
		}
	}

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[new_act].data ));
	UndoRedo::instance().barrier( "Changing the name of an action", false );

	actions_[new_act].data->writeString( "name", actName );

	//Make sure that we update the current actation name if needed
	if (currAct_ == actID)
		setAct( new_act );	

	dirty( actions_[new_act].model );

	return true;

}

std::string Mutant::actAnim( const StringPair& actID )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return "";

	return actions_[actID].data->readString( "animation", "" );
}

void Mutant::actAnim( const StringPair& actID, const std::string& animName )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));
	UndoRedo::instance().barrier( "Changing the animation of an action", false );

	actions_[actID].data->writeString( "animation", animName );

	reloadAllLists();

	dirty( actions_[actID].model );
}

float Mutant::actBlendTime( const StringPair& actID, const std::string& fieldName )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return 0.3f;

	return actions_[actID].data->readFloat( fieldName, 0.3f );
}

void Mutant::actBlendTime( const StringPair& actID, const std::string& fieldName, float val )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	//Make sure there was a change
	if (val == actBlendTime( actID, fieldName ) )
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));

	actions_[actID].data->writeFloat( fieldName, val );

	UndoRedo::instance().barrier( "Changing an action blend value", false );

	dirty( actions_[actID].model );
}

bool Mutant::actFlag( const StringPair& actID, const std::string& flagName )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return false;

	return actions_[actID].data->readBool( flagName, false );
}

void Mutant::actFlag( const StringPair& actID, const std::string& flagName, bool flagVal )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	//Make sure there was a change
	if (flagVal == actFlag( actID, flagName ) )
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));
	
	actions_[actID].data->writeBool( flagName, flagVal );

	UndoRedo::instance().barrier( "Changing an action flag", false );

	dirty( actions_[actID].model );
}

int Mutant::actTrack( const StringPair& actID )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return -1;

	int track = actions_[actID].data->readBool( "blended", false ) ? -1 : 0;
	return actions_[actID].data->readInt( "track", track );
}

void Mutant::actTrack( const StringPair& actID, int track )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));
	UndoRedo::instance().barrier( "Changing the track of an action", false );

	// Since "blended" is now deprecated we can remove it
	actions_[actID].data->delChild( "blended" ); 

	actions_[actID].data->writeInt( "track", track );

	dirty( actions_[actID].model );
}

float Mutant::actMatchFloat( const StringPair& actID, const std::string& typeName, const std::string& flagName, bool& valSet )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
	{
		valSet = false;
		return 0.f;
	}

	if (actions_[actID].data->readString( "match/" + typeName + "/" + flagName, "" ) == "")
	{
		valSet = false;
		return 0.f;
	}

	valSet = true;
	return actions_[actID].data->readFloat( "match/" + typeName + "/" + flagName, 0.f );
}

void Mutant::actMatchVal( const StringPair& actID, const std::string& typeName, const std::string& flagName, bool empty, float val, bool &setUndo )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	//Make sure there was a change
	if (empty && (actions_[actID].data->readString( "match/" + typeName + "/" + flagName, "") == ""))
		return;

	//Make sure there was a change
	if (val == actions_[actID].data->readFloat( "match/" + typeName + "/" + flagName, val + 1) )
		return;

	if ( setUndo )
	{
		UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));
		UndoRedo::instance().barrier( "Changing an action matcher value", false );
		setUndo = false;
	}

	if (!empty)
	{
		DataSectionPtr match = actions_[actID].data->openSection( "match" );
		if (!match)
			match = actions_[actID].data->newSection( "match" );

		DataSectionPtr trigger = match->openSection( typeName );
		if (!trigger)
			trigger = match->newSection( typeName );
	
		trigger->writeFloat( flagName, val );
	}
	else
	{
		DataSectionPtr match = actions_[actID].data->openSection( "match" );
		if (match)
		{
			DataSectionPtr trigger = match->openSection( typeName );
			if (trigger)
			{
				trigger->delChild( flagName ); 

				// If there are no triggers left
				if (trigger->countChildren() == 0) 
				{
					match->delChild( typeName ); // Remove the section
				}

				// If there are no matches left
				if (match->countChildren() == 0)
				{
					actions_[actID].data->delChild( "match" ); // Remove the section
				}
			}
		}
	}
	dirty( actions_[actID].model );
}


std::string Mutant::actMatchCaps( const StringPair& actID, const std::string& typeName, const std::string& capsType )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return "";

	return actions_[actID].data->readString( "match/" + typeName + "/" + capsType, "" );
}

void Mutant::actMatchCaps( const StringPair& actID, const std::string& typeName, const std::string& capsType, const std::string& newCaps )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	std::string oldCaps = actions_[actID].data->readString( "match/" + typeName + "/" + capsType, "" );

	if (newCaps == oldCaps) return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));
	UndoRedo::instance().barrier( "Changing an action matcher capabilties list", false );

	actions_[actID].data->writeString( "match/" + typeName + "/" + capsType, newCaps );

	reloadAllLists();

	dirty( actions_[actID].model );
}

void Mutant::actMatchFlag( const StringPair& actID, const std::string& flagName, bool flagVal )
{
	//First make sure the action exists
	if (actions_.find(actID) == actions_.end())
		return;

	//Make sure there was a change
	if (flagVal == actions_[actID].data->readBool( "match/" + flagName, false) )
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, actions_[actID].data ));
	UndoRedo::instance().barrier( "Changing an action matcher flag", false );

	DataSectionPtr match = actions_[actID].data->openSection( "match" );
	if (!match)
			match = actions_[actID].data->newSection( "match" );

	if (flagVal)
	{
		match->writeBool( flagName, flagVal );
	}
	else
	{
		match->delChild( flagName );

		// If there are no matches left
		if (match->countChildren() == 0)
		{
			actions_[actID].data->delChild( "match" ); // Remove the section
		}
	}

	dirty( actions_[actID].model );
}
