/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// Module Interface
#include "pch.hpp"
#include "graphics_settings.hpp"

// BW Tech Hearders
#include "resmgr/dataresource.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 );

namespace Moo
{

// -----------------------------------------------------------------------------
// Section: GraphicsSetting
// -----------------------------------------------------------------------------

/**
 *	Constructor. When this setting is added to the registry, the active option 
 *	will be either reset to the first supported option or to the one restored 
 *	from the options file, if the provided activeOption is -1. If, instead, the
 *	provided activeOption is a non negative value, it will either be reset to 
 *	the one restored from the options file (if this happens to be different 
 *	from the value passed as parameter) or not be reset at all.
 * 
 *	@param	label			label for this setting.
 *	@param	activeOption	initial active option (see above).
 *	@param	delayed			delayed flag.
 *	@param	needsRestart	needsRestart flag.
 */
GraphicsSetting::GraphicsSetting(
	const std::string & label, 
	              int   activeOption, 
	             bool   delayed,
	             bool   needsRestart) :
	label_(label),
	options_(),
	activeOption_(activeOption),
	delayed_(delayed),
	needsRestart_(needsRestart)
{}

/**
 *	Destructor.
 */
GraphicsSetting::~GraphicsSetting()
{}

/**
 *	Returns the label for this setting. Applications must translate 
 *	this to the local human readable string representation.
 */
const std::string & GraphicsSetting::label() const
{
	return this->label_;
}

/**
 *	Adds new option for this setting.
 *
 *	@param	label		label for this option.
 *	@param	isSupported	true if this option is supported be the system.
 */
int GraphicsSetting::addOption(const std::string & label, bool isSupported)
{
	this->options_.push_back(std::make_pair(label, isSupported));
	return this->options_.size() - 1;
}

/**
 *	Returns list of options for this setting.
 */
const GraphicsSetting::StringBoolPairVector & GraphicsSetting::options() const
{
	return this->options_;
}

/**
 *	Selects new option. Will trigger onOptionSelected. 
 *	If the chosen options is not supported, the call will be ignored.
 *
 *	@param	optionIndex	index to option being selected.
 */
void GraphicsSetting::selectOption(int optionIndex)
{
	if (this->selectOptionPriv(optionIndex) && this->needsRestart_)
	{
		GraphicsSetting::s_needsRestart = true;
	}
}

/**
 *	Returns currently active option.
 */
int GraphicsSetting::activeOption() const
{
	MF_ASSERT(this->activeOption_>=0);
	return this->activeOption_;
}

/**
 *	Registers another GraphicsSetting object as an slave to this one. Slave 
 *	settings are not listed with GraphicsSetting::settings() and they have
 *	their activeOption property updated and onOptionSelected method triggered 
 *	whenever their master's active option changes.
 *
 *	@param	slave	GraphicsSetting object to be added as slave.
 */
void GraphicsSetting::addSlave(GraphicsSettingPtr slave)
{
	// effects must have the exact same options
	MF_ASSERT(slave->options() == this->options())

	GraphicsSettingVector & slaves = this->slaves_;
	MF_ASSERT(std::find(
		slaves.begin(), slaves.end(), slave) == 
			slaves.end());

	slaves.push_back(slave);	
}

/**
 *	Reads graphics options from DataSection provided.
 *
 *	@param	section DataSection where to load the options from.
 */
void GraphicsSetting::read(DataSectionPtr section)
{
	DataSectionIterator sectIt  = section->begin();
	DataSectionIterator sectEnd = section->end();
	while (sectIt != sectEnd)
	{
		std::string label = (*sectIt)->readString("label");
		int activeOption   = (*sectIt)->readInt("activeOption");
		GraphicsSettingPtr setting = GraphicsSetting::getFromLabel(label);
		if (setting.exists())
		{	
			setting->selectOption(activeOption);			
		}
		else
		{
			GraphicsSetting::s_latentSettings[label] = activeOption;
		}
		++sectIt;
	}
	GraphicsSetting::commitPending();
}

/**
 *	Writes graphics options into DataSection provided.
 *
 *	@param	section DataSection where to save the options into.
 */
void GraphicsSetting::write(DataSectionPtr section)
{
	section->delChildren();
	GraphicsSettingVector::const_iterator setIt  = GraphicsSetting::s_settings.begin();
	GraphicsSettingVector::const_iterator setEnd = GraphicsSetting::s_settings.end();
	while (setIt != setEnd)
	{
		int optionInSetting = -1;
		if (!GraphicsSetting::isPending((*setIt), optionInSetting))
		{
			optionInSetting = (*setIt)->activeOption();
		}

		DataSectionPtr settingsEntry = section->newSection( "entry" );
		settingsEntry->writeString("label", (*setIt)->label());
		settingsEntry->writeInt("activeOption", optionInSetting);
		++setIt;
	} 
}

/**
 *	Registers new GraphicsSetting into global registry. 
 *  
 *	More than one setting can be registered under the same entry. For this to 
 *	happen, they must share the same description, have the same number of 
 *	options, each with the same descripition text and appearing in the same 
 *	order. If two or more effects shared the same description, but the above 
 *	rules are not respected, an assertion will fail. 
 *
 *	@param	setting	The new GraphicsSetting to register.
 */
void GraphicsSetting::add(GraphicsSettingPtr setting)
{
	int optionInSetting = -1;
	if (!GraphicsSetting::isPending(setting, optionInSetting))
	{
		optionInSetting = setting->activeOption_;
	}

	// look for option 
	// loaded from file
	const std::string & label = setting->label();
	const StringIntMap & latentSettings = GraphicsSetting::s_latentSettings;
	StringIntMap::const_iterator setIt = latentSettings.find(label);
	if (setIt != latentSettings.end())
	{
		const int & optionInFile = setIt->second;

		// if option read from file is different
		// from the one currently set, reset it.
		if (optionInFile != optionInSetting)
		{
			// don't update needRestart
			setting->selectOptionPriv(optionInFile);
		}
	}
	// if the one currently set is negative,
	// reset it to the first supported value
	else if (optionInSetting < 0)
	{
		// don't update needRestart
		setting->selectOptionPriv(0);
	}
	// otherwise, assume engine is already running with the 
	// current activeOption setting. Don't call selectOption

	GraphicsSettingPtr master = GraphicsSetting::getFromLabel(label);
	if (master.exists())
	{
		master->addSlave(setting);
	}
	else 
	{
		GraphicsSetting::s_settings.push_back(setting);
	}
}

/**
 *	Returns list of registered settings.
 */
const GraphicsSetting::GraphicsSettingVector & GraphicsSetting::settings()
{
	return GraphicsSetting::s_settings;
}

/**
 *	Returns true if there are any pending settings that require commit. 
 *	Settings flagged with &lt;delayed> do not have their activeOption property 
 *	and onOptionSelected method triggered immediatly. Instead, they are added 
 *	to a pending list. Use this method to query whether there are any settings 
 *	in the pending list.
 */
bool GraphicsSetting::hasPending()
{
	return !GraphicsSetting::s_pending.empty();
}


/**
 *	Returns true if the given setting object is currently pending.
 *
 *	@param	setting				setting to check for pending status.
 *	@param	o_pendingOption		(out) value of active option pending.
 *	@return						True if setting is pending.
 */
bool GraphicsSetting::isPending(GraphicsSettingPtr setting, int & o_pendingOption)
{
	bool result = false;
	SettingIndexVector::iterator pendingIt  = GraphicsSetting::s_pending.begin();
	SettingIndexVector::iterator pendingEnd = GraphicsSetting::s_pending.end();
	while (pendingIt != pendingEnd)
	{
		if (pendingIt->first == setting)
		{
			o_pendingOption = pendingIt->second;
			result = true;
			break;
		}
		++pendingIt;
	}
	return result;
}

/**
 *	Commits all pending settings. Causes the activeOption property on pending 
 *	settings to be updated and their onOptionSelected method to be triggered.
 */
void GraphicsSetting::commitPending()
{
	SettingIndexVector::iterator pendingIt  = GraphicsSetting::s_pending.begin();
	while (pendingIt != GraphicsSetting::s_pending.end())
	{
		int index = pendingIt->second;
		pendingIt->first->activeOption_ = index;
		pendingIt->first->onOptionSelected(index);

		// notify all slaves
		GraphicsSettingVector::iterator slvIt  = pendingIt->first->slaves_.begin();
		GraphicsSettingVector::iterator slvEnd = pendingIt->first->slaves_.end();
		while (slvIt != slvEnd)
		{
			(*slvIt)->activeOption_ = index;
			(*slvIt)->onOptionSelected(index);
			++slvIt;
		}
		pendingIt = GraphicsSetting::s_pending.erase(pendingIt);
	}
}

/**
 *	Rolls back all pending settings. This will cause the engine 
 *	to ignore all options changes that are currently pending and 
 *	clear the pending serrings list.
 */
void GraphicsSetting::rollbackPending()
{
	 GraphicsSetting::s_pending.clear();
}

/**
 *	Returns true if, since the last time this method was called, 
 *	there has been any settings change that requires the application 
 *	to be restarted to come into effect.
 */
bool GraphicsSetting::needsRestart()
{
	bool result = GraphicsSetting::s_needsRestart;
	GraphicsSetting::s_needsRestart = false;
	return result;
}

/**
 *	Does the internal work related to setting the current active option.
 */
bool GraphicsSetting::selectOptionPriv(int optionIndex)
{
	// keep option in the range [0, size-1]
	int index = std::max(0, std::min(optionIndex, int(this->options_.size()-1)));

	// make sure it's supported. If it's not,
	// look for next supported option in list.
	for (; index<int(this->options_.size()); ++index)
	{
		if (this->options_[index].second) // supported?
		{
			break;
		}
	}
	if (index == this->options_.size())
	{
		CRITICAL_MSG("GraphicsSetting::selectOptionPriv - no supported option found for %s graphic setting", label_.c_str());
	}

	bool applied = false;
	if (!this->delayed_)
	{
		if (index != this->activeOption_)
		{
			this->activeOption_ = index;

			// notify derivied objects
			this->onOptionSelected(index);

			// notify all slaves
			GraphicsSettingVector::iterator slaveIt  = this->slaves_.begin();
			GraphicsSettingVector::iterator slaveEnd = this->slaves_.end();
			while (slaveIt != slaveEnd)
			{
				(*slaveIt)->activeOption_ = index;
				(*slaveIt)->onOptionSelected(index);
				++slaveIt;
			}
			applied = true;
		}
	}
	else
	{
		applied = GraphicsSetting::addPending(this, index);
	}
	return applied;
}

/**
 *	Adds a setting to the pending list (private).
 */
bool GraphicsSetting::addPending(GraphicsSettingPtr setting, int optionIndex)
{
	// if setting is already pending,
	// just reset the optionIndex
	bool applied = false;
	bool found = false;
	SettingIndexVector::iterator pendingIt  = GraphicsSetting::s_pending.begin();
	SettingIndexVector::iterator pendingEnd = GraphicsSetting::s_pending.end();
	while (pendingIt != pendingEnd)
	{
		if (pendingIt->first == setting)
		{
			if (optionIndex == setting->activeOption_)
			{
				// new setting is redundant (the same as the one 
				// currently in use). Remove setting from pending list
				GraphicsSetting::s_pending.erase(pendingIt);
			}
			else
			{
				pendingIt->second = optionIndex;
				applied = true;
			}
			found = true;
			break;
		}
		++pendingIt;
	}

	// otherwise, add new entry, but only if new option is 
	// different from the one currently set on settings 
	// (selectOption does not filter for delayed settings)
	if (!found && optionIndex != setting->activeOption_)
	{
		applied = false;
		GraphicsSetting::s_pending.push_back(
			std::make_pair(setting.getObject(), optionIndex));
	}
	return applied;
}

/**
 *	Returns registered GraphicsSetting object from given label. If
 *	a suitable GraphicsSetting object is not found, returns empty pointer.
 *
 *	@param	label	label of setting to retrieve.
 */
GraphicsSetting::GraphicsSettingPtr GraphicsSetting::getFromLabel(
	const std::string & label)
{
	GraphicsSettingVector::const_iterator setIt  = GraphicsSetting::s_settings.begin();
	GraphicsSettingVector::const_iterator setEnd = GraphicsSetting::s_settings.end();
	while (setIt != setEnd)
	{
		if ((*setIt)->label() == label)
		{
			break;
		}
		++setIt;
	}
	return setIt != setEnd ? *setIt : 0;
}

GraphicsSetting::SettingIndexVector GraphicsSetting::s_pending;
GraphicsSetting::GraphicsSettingVector GraphicsSetting::s_settings;
GraphicsSetting::StringIntMap GraphicsSetting::s_latentSettings;
bool GraphicsSetting::s_needsRestart = false;
std::string GraphicsSetting::s_filename = "";

} // namespace moo

// settings_registry.cpp
