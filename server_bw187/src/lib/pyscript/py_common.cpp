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

// BW Tech Hearders
#include "pyscript/script.hpp"
#include "pyscript/py_data_section.hpp"
#include "moo/graphics_settings.hpp"
#include "moo/material_kinds.hpp"

int PyCommon_token = 0;

/*~ function BigWorld.graphicsSettings
 *
 *  Returns list of registered graphics settings
 *	@return	list of 3-tuples in the form (label : string, index to active
 *			option in options list: int, options : list). Each option entry is a
 *			2-tuple in the form (option label : string, support flag : boolean.
 */
static PyObject * graphicsSettings()
{
	typedef Moo::GraphicsSetting::GraphicsSettingVector GraphicsSettingVector;
	const GraphicsSettingVector & settings = Moo::GraphicsSetting::settings();
	PyObject * settingsList = PyList_New(settings.size());

	GraphicsSettingVector::const_iterator setIt  = settings.begin();
	GraphicsSettingVector::const_iterator setEnd = settings.end();
	while (setIt != setEnd)
	{
		typedef Moo::GraphicsSetting::StringBoolPairVector StringBoolPairVector;
		const StringBoolPairVector & options = (*setIt)->options();
		PyObject * optionsList = PyList_New(options.size());
		StringBoolPairVector::const_iterator optIt  = options.begin();
		StringBoolPairVector::const_iterator optEnd = options.end();
		while (optIt != optEnd)
		{
			PyObject * optionItem = PyTuple_New(2);
			PyTuple_SetItem(optionItem, 0, Script::getData(optIt->first));
			PyTuple_SetItem(optionItem, 1, Script::getData(optIt->second));

			int optionIndex = std::distance(options.begin(), optIt);
			PyList_SetItem(optionsList, optionIndex, optionItem);
			++optIt;
		}
		PyObject * settingItem = PyTuple_New(3);
		PyTuple_SetItem(settingItem, 0, Script::getData((*setIt)->label()));

		// is setting is pending, use value stored in pending
		// list. Otherwise, use active option in setting
		int activeOption = 0;
		if (!Moo::GraphicsSetting::isPending(*setIt, activeOption))
		{
			activeOption = (*setIt)->activeOption();
		}
		PyTuple_SetItem(settingItem, 1, Script::getData(activeOption));
		PyTuple_SetItem(settingItem, 2, optionsList);

		int settingIndex = std::distance(settings.begin(), setIt);
		PyList_SetItem(settingsList, settingIndex, settingItem);
		++setIt;
	}

	return settingsList;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, graphicsSettings, END, BigWorld )


/*~ function BigWorld.setGraphicsSetting
 *
 *	Sets graphics setting option.
 *
 *	Raises a ValueError if the given label does not name a graphics setting, if
 *	the option index is out of range, or if the option is not supported.
 *
 *	@param	label		    string - label of setting to be adjusted.
 *	@param	optionIndex		int - index of option to set.
 */
static bool setGraphicsSetting( const std::string label, int optionIndex )
{

	bool result = false;
	typedef Moo::GraphicsSetting::GraphicsSettingVector GraphicsSettingVector;
	GraphicsSettingVector settings = Moo::GraphicsSetting::settings();
	GraphicsSettingVector::const_iterator setIt  = settings.begin();
	GraphicsSettingVector::const_iterator setEnd = settings.end();
	while (setIt != setEnd)
	{
		if ((*setIt)->label() == label)
		{
			if (optionIndex < int( (*setIt)->options().size() ))
			{
				if ((*setIt)->options()[optionIndex].second)
				{
					(*setIt)->selectOption( optionIndex );
					result = true;
				}
				else
				{
					PyErr_SetString( PyExc_ValueError,
						"Option is not supported." );
				}
			}
			else
			{
				PyErr_SetString( PyExc_ValueError,
					"Option index out of range." );
			}
			break;
		}
		++setIt;
	}
	if (setIt == setEnd)
	{
		PyErr_SetString( PyExc_ValueError,
			"No setting found with given label." );
	}

	return result;
}
PY_AUTO_MODULE_FUNCTION( RETOK, setGraphicsSetting,
	ARG( std::string, ARG( int, END ) ), BigWorld )


/*~ function BigWorld.commitPendingGraphicsSettings
 *
 *	This function commits any pending graphics settings. Some graphics
 *	settings, because they may block the game for up to a few minutes when
 *	coming into effect, are not committed immediately. Instead, they are
 *	flagged as pending and require commitPendingGraphicsSettings to be called
 *	to actually apply them.
 */
static void commitPendingGraphicsSettings()
{
	Moo::GraphicsSetting::commitPending();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, commitPendingGraphicsSettings, END, BigWorld )


/*~ function BigWorld.hasPendingGraphicsSettings
 *
 *  This function returns true if there are any pending graphics settings.
 *  Some graphics settings, because they may block the game for up to a few
 *  minutes when coming into effect, are not committed immediately. Instead,
 *  they are flagged as pending and require commitPendingGraphicsSettings to be
 *  called to actually apply them.
 */
static bool hasPendingGraphicsSettings()
{
	return Moo::GraphicsSetting::hasPending();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, hasPendingGraphicsSettings, END, BigWorld )


/*~ function BigWorld.graphicsSettingsNeedRestart
 *
 *  This function returns true if any recent graphics setting change
 *	requires the client to be restarted to take effect. If that's the
 *	case, restartGame can be used to restart the client. The need restart
 *	flag is reset when this method is called.
 */
static bool graphicsSettingsNeedRestart()
{
	return Moo::GraphicsSetting::needsRestart();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, graphicsSettingsNeedRestart, END, BigWorld )


/*~ function BigWorld.roolbackPendingGraphicsSettings
 *
 *  This function rolls back any pending graphics settings.
 */
static void roolbackPendingGraphicsSettings()
{
	return Moo::GraphicsSetting::rollbackPending();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, roolbackPendingGraphicsSettings, END, BigWorld )

/*~ function BigWorld.getMaterialKinds
 *	@components{ client, cell, base }
 *
 *	This method returns a list of id and section tuples of the
 *	material kind list, in the order in which they appear in the XML file
 *
 *	@return		The list of (id, DataSection) tuples of material kinds
 */
/**
 *	This method returns a list of id and section tuples of material kinds.
 */
static PyObject* getMaterialKinds()
{
	const MaterialKinds::Map& kinds = MaterialKinds::instance().materialKinds();

	int size = kinds.size();
	PyObject* pList = PyList_New( size );

	MaterialKinds::Map::const_iterator iter = kinds.begin();
	int i = 0;

	while (iter != kinds.end())
	{
		PyObject * pTuple = PyTuple_New( 2 );

		PyTuple_SetItem( pTuple, 0, PyInt_FromLong( iter->first ) );
		PyTuple_SetItem( pTuple, 1, new PyDataSection( iter->second ) );

		PyList_SetItem( pList, i, pTuple );

		i++;
		++iter;
	}

	return pList;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, getMaterialKinds, END, BigWorld )

// py_common.cpp
