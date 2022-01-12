#include "cellapp/cellapp.hpp"

static class CellAppIDClass
{
public:
	CellAppIDClass()
	{
		MF_WATCH( "cellAppID", CellApp::instance(), &CellApp::id );
	}
} s_cellAppID;


// add_cellapp_id.cpp
