/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// NewSpace.cpp : implementation file
//
#include "pch.hpp"
#include "new_space_dlg.h"

#include "appmgr/options.hpp"
#include "resmgr/datasection.hpp"
#include "resmgr/multi_file_system.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"
#include "cstdmf/debug.hpp"
#include "chunk/chunk_format.hpp"
#include "controls/dir_dialog.hpp"
#include "big_bang.hpp"
#include "chunk/base_chunk_space.hpp"
#include <set>

static AutoConfigString s_skyXML( "environment/skyXML" );
static AutoConfigString s_blankCDataFname( "dummy/cData" );

DECLARE_DEBUG_COMPONENT2( "BigBang", 0 )

static const int NEWSPACEDLG_MAX_CHUNKS = 1000;
static const int NEWSPACEDLG_MAX_DIM_CHARS = 4;
static char* NEWSPACEDLG_KM_FORMAT = "(%.1f km)";
static char* NEWSPACEDLG_M_FORMAT = "(%d m)";


// NewSpace dialog

IMPLEMENT_DYNAMIC(NewSpaceDlg, CDialog)
NewSpaceDlg::NewSpaceDlg(CWnd* pParent /*=NULL*/)
	: CDialog(NewSpaceDlg::IDD, pParent)
	, defaultSpacePath_( "spaces" )
	, mChangeSpace(0)
{
}

NewSpaceDlg::~NewSpaceDlg()
{
}

void NewSpaceDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SPACE, space_);
	DDX_Control(pDX, IDC_WIDTH, width_);
	DDX_Control(pDX, IDC_HEIGHT, height_);
	DDX_Control(pDX, IDC_PROGRESS1, progress_ );
	DDX_Check(pDX, IDC_CHANGESPACE, mChangeSpace );
	DDX_Control(pDX, IDC_NEWSPACE_WIDTH_KM, widthKms_);
	DDX_Control(pDX, IDC_NEWSPACE_HEIGHT_KM, heightKms_);
	DDX_Control(pDX, IDC_SPACE_PATH, spacePath_);
	DDX_Control(pDX, IDCANCEL, buttonCancel_);
	DDX_Control(pDX, IDOK, buttonCreate_);
}


BEGIN_MESSAGE_MAP(NewSpaceDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_EN_CHANGE(IDC_SPACE, OnEnChangeSpace)
	ON_BN_CLICKED(IDC_NEWSPACE_BROWSE, OnBnClickedNewspaceBrowse)
	ON_EN_CHANGE(IDC_WIDTH, OnEnChangeWidth)
	ON_EN_CHANGE(IDC_HEIGHT, OnEnChangeHeight)
	ON_EN_CHANGE(IDC_SPACE_PATH, OnEnChangeSpacePath)
END_MESSAGE_MAP()


// NewSpace message handlers

BOOL NewSpaceDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	INIT_AUTO_TOOLTIP();
	
	width_.SetWindowText( "10" );
	height_.SetWindowText( "10" );

	space_.SetWindowText( defaultSpace_ );
	spacePath_.SetWindowText( BWResource::resolveFilename( defaultSpacePath_ ).c_str() );

	mChangeSpace = 1;
	UpdateData( FALSE );

	return TRUE;  // return TRUE unless you set the focus to a control
}

class SpaceMaker
{
	std::string spaceName_;
	std::string errorMessage_;
	int minX_, maxX_, minZ_, maxZ_, minY_, maxY_;
	CProgressCtrl& progress_;
	LPVOID terrainCache_;
	DWORD terrainSize_;
	char* memoryCache_;
	std::set<std::string> subDir_;
public:
	SpaceMaker( const std::string& spaceName, int width, int height, int minHeight, int maxHeight, CProgressCtrl& progress )
		: spaceName_( spaceName ),
		  minY_( minHeight ),
		  maxY_( maxHeight ),
		  progress_( progress ),
		  terrainSize_( 0 ),
		  terrainCache_( NULL )
	{
		calcBounds( width, height );
		//Copy the cData
		std::string templateTname = BWResource::resolveFilename( s_blankCDataFname );
		HANDLE file = CreateFile( templateTname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL );
		if( file != INVALID_HANDLE_VALUE )
		{
			DWORD size = GetFileSize( file, NULL );
			terrainSize_ = size;
			char diskRoot[] = { templateTname[0], ':', '\0' };
			DWORD dummy, bytesPerSector;
			GetDiskFreeSpace( diskRoot, &dummy, &bytesPerSector, &dummy, &dummy );
			size = ( size + bytesPerSector - 1 ) / bytesPerSector * bytesPerSector;
			terrainCache_ = VirtualAlloc( NULL, size, MEM_COMMIT, PAGE_READWRITE );
			ReadFile( file, terrainCache_, size, &dummy, NULL );
			CloseHandle( file );
		}
		memoryCache_ = (char*)VirtualAlloc( NULL, 128 * 1024 /*big enough*/, MEM_COMMIT, PAGE_READWRITE );
	}

	bool create()
	{
		if( !terrainCache_ )
		{
			errorMessage_ = "cannot find default cdata";
			return false;
		}
		if( BigBang::instance().connection() )
			if( !BigBang::instance().connection()->changeSpace(
					BWResource::dissolveFilename( spaceName_ ) ) ||
				!BigBang::instance().connection()->lock( GridRect( GridCoord( minX_ - 1, minZ_ - 1 ),
					GridCoord( maxX_ + 1, maxZ_ + 1 ) ), "create space" ) )
			{
				errorMessage_ = "Unable to lock space";
				return false;
			}
		if (!createSpaceDirectory())
		{
			errorMessage_ = "Unable to create space directory";
			return false;
		}

		if (!createSpaceSettings())
		{
			errorMessage_ = "Unable to create space.settings";
			return false;
		}

		DEBUG_MSG( "Purgeless\n");
		DEBUG_MSG( "---------\n");
		
		if (!createChunks( false ))
		{
			errorMessage_ = "Unable to create chunks";
			return false;
		}

/*		DEBUG_MSG( "Purgeful\n");
		DEBUG_MSG( "--------\n");
		
		if (!createChunks( true ))
		{
			errorMessage_ = "Unable to create chunks";
			return false;
		}*/

		return true;
	}

	std::string errorMessage() { return errorMessage_; }

private:
	bool createSpaceDirectory()
	{
		BWResource::ensurePathExists( spaceName_ + "/" );

		return BWResource::openSection( BWResource::dissolveFilename( spaceName_ ) );
	}

	void calcBounds( int width, int height )
	{
		/*
		// Standard bounds calc
		*/
		int w = width / 2;
		int h = height / 2;

		minX_ = -w;
		maxX_ = w - 1;
		if (w * 2 != width)
			++maxX_;

		minZ_ = -h;
		maxZ_ = h - 1;
		if (h * 2 != height)
			++maxZ_;
		/*
		// Corner bounds calc
		minX_ = 0;
		maxX_ = width_;
		minZ_ = -height_;
		maxZ_ = -1;
		*/
	}


	bool createSpaceSettings()
	{
		DataSectionPtr settings = BWResource::openSection(
			BWResource::dissolveFilename( spaceName_ ) )->newSection( "space.settings" );

		if (!settings)
			return false;		
		
		std::string skyXML = s_skyXML.value();
		if (skyXML.empty())
			skyXML = "system/data/sky.xml";
		settings->writeString( "timeOfDay", skyXML);
		settings->writeString( "skyGradientDome", skyXML );

		settings->writeInt( "bounds/minX", minX_ );
		settings->writeInt( "bounds/maxX", maxX_ );
		settings->writeInt( "bounds/minY", minZ_ );
		settings->writeInt( "bounds/maxY", maxZ_ );

		settings->save();

		return true;
	}

	std::string chunkName( int x, int z ) const
	{
		return ChunkFormat::outsideChunkIdentifier( x,z );
	}

	void createPortal( const std::string& toChunk,
		const Vector3& uAxis, const Vector3& pt1, const Vector3& pt2,
		const Vector3& pt3, const Vector3& pt4, char*& buffer )
	{
		char portalStart[] = "\t\t<portal>\r\n";
		char portalEnd[] = "\t\t</portal>\r\n";

		strcpy( buffer, portalStart );
		buffer += sizeof( portalStart ) - 1;

			buffer += sprintf( buffer, "\t\t\t<chunk>\t%s\t</chunk>\r\n", toChunk.c_str() );
			buffer += sprintf( buffer, "\t\t\t<uAxis>\t%f %f %f\t</uAxis>\r\n", uAxis.x, uAxis.y, uAxis.z );
			buffer += sprintf( buffer, "\t\t\t<point>\t%f %f %f\t</point>\r\n", pt1.x, pt1.y, pt1.z );
			buffer += sprintf( buffer, "\t\t\t<point>\t%f %f %f\t</point>\r\n", pt2.x, pt2.y, pt2.z );
			buffer += sprintf( buffer, "\t\t\t<point>\t%f %f %f\t</point>\r\n", pt3.x, pt3.y, pt3.z );
			buffer += sprintf( buffer, "\t\t\t<point>\t%f %f %f\t</point>\r\n", pt4.x, pt4.y, pt4.z );

		strcpy( buffer, portalEnd );
		buffer += sizeof( portalEnd ) - 1;
	}

	void createBoundary( const Vector3& normal, float d, char*& buffer )
	{
		char normalStart[] = "\t\t<normal>\t";
		char normalEnd[] = "\t</normal>\r\n";
		char dStart[] = "\t\t<d>\t";
		char dEnd[] = "\t</d>\r\n";

		strcpy( buffer, normalStart );
		buffer += sizeof( normalStart ) - 1;
		buffer += sprintf( buffer, "%f %f %f", normal.x, normal.y, normal.z );
		strcpy( buffer, normalEnd );
		buffer += sizeof( normalEnd ) - 1;

		strcpy( buffer, dStart );
		buffer += sizeof( dStart ) - 1;
		buffer += sprintf( buffer, "%f", d );
		strcpy( buffer, dEnd );
		buffer += sizeof( dEnd ) - 1;
	}

	void createBoundary( int x, int z, int i, char*& buffer )
	{
		float minYf = float(minY_);
		float maxYf = float(maxY_);
		float minZf = float(minZ_);
		float maxZf = float(maxZ_);

		char boundaryStart[] = "\t<boundary>\r\n";
		char boundaryEnd[] = "\t</boundary>\r\n";

		strcpy( buffer, boundaryStart );
		buffer += sizeof( boundaryStart ) - 1;

		if (i == 0)
		{
			// right
			createBoundary( Vector3( 1.f, 0.f, 0.f ), 0.f, buffer );
			if (x != minX_)
				createPortal( chunkName( x - 1, z ), Vector3( 0.f, 1.f, 0.f ),
					Vector3( minYf, 0.f, 0.f ),
					Vector3( maxYf, 0.f, 0.f ),
					Vector3( maxYf, GRID_RESOLUTION, 0.f ),
					Vector3( minYf, GRID_RESOLUTION, 0.f ), buffer );
		}
		else if (i == 1)
		{
			// left
			createBoundary( Vector3( -1.f, 0.f, 0.f ), -GRID_RESOLUTION, buffer );
			if (x != maxX_)
				createPortal( chunkName( x + 1, z ), Vector3( 0.f, 0.f, 1.f ),
					Vector3( 0.f, minYf, 0.f ),
					Vector3( GRID_RESOLUTION, minYf, 0.f ),
					Vector3( GRID_RESOLUTION, maxYf, 0.f ),
					Vector3( 0.f, maxYf, 0.f ), buffer );

		}
		else if (i == 2)
		{
			// bottom
			createBoundary( Vector3( 0.f, 1.f, 0.f ), minYf, buffer );
			createPortal( "earth", Vector3( 0.f, 0.f, 1.f ),
				Vector3( 0.f, 0.f, 0.f ),
				Vector3( GRID_RESOLUTION, 0.f, 0.f ),
				Vector3( GRID_RESOLUTION, GRID_RESOLUTION, 0.f ),
				Vector3( 0.f, GRID_RESOLUTION, 0.f ), buffer );
		}
		else if (i == 3)
		{
			// top
			createBoundary( Vector3( 0.f, -1.f, 0.f ), -maxYf, buffer );
			createPortal( "heaven", Vector3( 1.f, 0.f, 0.f ),
				Vector3( 0.f, 0.f, 0.f ),
				Vector3( GRID_RESOLUTION, 0.f, 0.f ),
				Vector3( GRID_RESOLUTION, GRID_RESOLUTION, 0.f ),
				Vector3( 0.f, GRID_RESOLUTION, 0.f ), buffer );
		}
		else if (i == 4)
		{
			// back
			createBoundary( Vector3( 0.f, 0.f, 1.f ), 0.f, buffer );
			if (z != minZ_)
				createPortal( chunkName( x, z - 1 ), Vector3( 1.f, 0.f, 0.f ),
					Vector3( 0.f, minYf, 0.f ),
					Vector3( GRID_RESOLUTION, minYf, 0.f ),
					Vector3( GRID_RESOLUTION, maxYf, 0.f ),
					Vector3( 0.f, maxYf, 0.f ), buffer );
		}
		else if (i == 5)
		{
			// front
			createBoundary( Vector3( 0.f, 0.f, -1.f ), -GRID_RESOLUTION, buffer );
			if (z != maxZ_)
				createPortal( chunkName( x, z + 1 ), Vector3( 0.f, 1.f, 0.f ),
					Vector3( minYf, 0.f, 0.f ),
					Vector3( maxYf, 0.f, 0.f ),
					Vector3( maxYf, GRID_RESOLUTION, 0.f ),
					Vector3( minYf, GRID_RESOLUTION, 0.f ), buffer );
		}

		strcpy( buffer, boundaryEnd );
		buffer += sizeof( boundaryEnd ) - 1;
	}

	void createTerrain( int x, int z, char*& buffer )
	{
		std::string name = chunkName( x, z );
		std::string cDataName = name.substr( 0, name.size() - 1 )
			+ "o.cdata";
		std::string terrainName = cDataName + "/terrain";

		buffer += sprintf( buffer, "\t<terrain>\r\n\t\t<resource>\t%s\t</resource>\r\n\t</terrain>",
			terrainName.c_str() );

		MF_ASSERT(!s_blankCDataFname.value().empty());

		//Copy the cData
		std::string fname = spaceName_ + "/" + cDataName;
		HANDLE file = CreateFile( fname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL );
		if( file != INVALID_HANDLE_VALUE )
		{
			DWORD dummy;
			static DWORD bytesPerSector = 0;
			if( bytesPerSector == 0 )
			{
				char diskRoot[] = { fname[0], ':', '\0' };
				GetDiskFreeSpace( diskRoot, &dummy, &bytesPerSector, &dummy, &dummy );
			}
			DWORD size = ( terrainSize_ + bytesPerSector - 1 ) / bytesPerSector * bytesPerSector;
			WriteFile( file, terrainCache_, size, &dummy, NULL );
			CloseHandle( file );
			file = CreateFile( fname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
				OPEN_EXISTING, 0, NULL );
			SetFilePointer( file, terrainSize_, NULL, FILE_BEGIN );
			SetEndOfFile( file );
			CloseHandle( file );
		}
	}

	void createBoundsAndTransform( int x, int z, char*& buffer )
	{
		float xf = float(x) * GRID_RESOLUTION;
		float zf = float(z) * GRID_RESOLUTION;

		buffer += sprintf( buffer,
			"\r\n\t<transform>\r\n"
			"\t\t<row0>\t%f %f %f\t</row0>\r\n"
			"\t\t<row1>\t%f %f %f\t</row1>\r\n"
			"\t\t<row2>\t%f %f %f\t</row2>\r\n"
			"\t\t<row3>\t%f %f %f\t</row3>\r\n"
			"\t</transform>\r\n",
			1.f, 0.f, 0.f,
			0.f, 1.f, 0.f,
			0.f, 0.f, 1.f,
			xf, 0.f, zf );
		buffer += sprintf( buffer,
			"\t<boundingBox>\r\n"
			"\t\t<min>\t%f %f %f\t</min>\r\n"
			"\t\t<max>\t%f %f %f\t</max>\r\n"
			"\t</boundingBox>\r\n",
			xf, float(minY_), zf,
			xf + GRID_RESOLUTION, float(maxY_), zf + GRID_RESOLUTION );
	}

	bool createChunk( int x, int z )
	{
		std::string name = chunkName( x, z ) + ".chunk";

		if( name.rfind( '/' ) != name.npos  )
		{
			std::string path = name.substr( 0, name.rfind( '/' ) + 1 );
			if( subDir_.find( path ) == subDir_.end() )
			{
				BWResource::ensurePathExists( spaceName_ + "/" + path );
				subDir_.insert( path );
			}
		}

		char* buffer = memoryCache_;
		buffer += sprintf( buffer, "<root>\r\n", name.c_str() );

		for (int i = 0; i < 6; ++i)
			createBoundary( x, z, i, buffer );

		createTerrain( x, z, buffer );
		createBoundsAndTransform( x, z, buffer );

		buffer += sprintf( buffer, "</root>\r\n", name.c_str() );

		std::string fname = spaceName_ + "/" + name;
		HANDLE file = CreateFile( fname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL );
		if( file != INVALID_HANDLE_VALUE )
		{
			DWORD dummy;
			static DWORD bytesPerSector = 0;
			if( bytesPerSector == 0 )
			{
				char diskRoot[] = { fname[0], ':', '\0' };
				GetDiskFreeSpace( diskRoot, &dummy, &bytesPerSector, &dummy, &dummy );
			}
			DWORD size = ( buffer - memoryCache_ + bytesPerSector - 1 ) / bytesPerSector * bytesPerSector;
			WriteFile( file, memoryCache_, size, &dummy, NULL );
			CloseHandle( file );
			file = CreateFile( fname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
				OPEN_EXISTING, 0, NULL );
			SetFilePointer( file, buffer - memoryCache_ , NULL, FILE_BEGIN );
			SetEndOfFile( file );
			CloseHandle( file );
		}

		return true;
	}

	bool createChunks( bool withPurge )
	{
		subDir_.clear();
		bool good = true;

		progress_.SetRange32( 0, (maxX_-minX_+1) * (maxZ_-minZ_+1) );
		progress_.SetPos( 0 );
		progress_.SetStep( 1 );

		int i = 0;
		
		int start = GetTickCount();
		
		for (int x = minX_; x <= maxX_; ++x)
		{
			int loop = GetTickCount();
			
			for (int z = minZ_; z <= maxZ_; ++z)
			{
				good &= createChunk( x, z );
				progress_.StepIt();
				if ((withPurge) && (++i > 150))
				{
					BWResource::instance().purgeAll();
					i = 0;
				}
			}
			
			int last = GetTickCount();

			float average = float( (last - start) / (x - minX_ + 1) );

			DEBUG_MSG( "output row %d of %d , %d , %2.0f\n", x-minX_+1, maxX_-minX_+1, last - loop,  average);
		}

		return good;
	}
};

void NewSpaceDlg::OnBnClickedOk()
{
	CWaitCursor wait;

	if ( !validateDim( width_ ) ||
		!validateDim( height_ ) )
	{
		return;
	}
	// Create the space!
	CString s, p, w, h, ny, my;
	space_.GetWindowText( s );
	spacePath_.GetWindowText( p );
	width_.GetWindowText( w );
	height_.GetWindowText( h );
	ny = "-2000000";
	my = "2000000";

	if ( !p.IsEmpty() && p.Right( 1 ) != "/" && p.Right( 1 ) != "\\" )
		p = p + "/";

	std::string spaceName = ( p + s ).GetBuffer();

	if ( s.IsEmpty() )
	{
		std::string msg = "There is no space name specified.\n\nFill in the space name, e.g. \"space001\".";
		MessageBox( msg.c_str(), "No space name given", MB_ICONERROR );
		space_.SetSel( 0, -1 ); // Select the space name field
		space_.SetFocus();
		return;
	}
	
	if ( p.IsEmpty() || BWResource::dissolveFilename( (LPCSTR)p ) == "" )
	{
		std::string msg = "The space cannot be located in the root directory."
			"\n\nWould you like to create it in \""
			+ BWResource::resolveFilename( defaultSpacePath_ ) + "/" + s.GetBuffer() + "\"?";
		if ( MessageBox( msg.c_str(), "No space folder given", MB_ICONINFORMATION | MB_OKCANCEL ) == IDCANCEL )
		{
			spacePath_.SetSel( 0, -1 ); // Select the space path field
			spacePath_.SetFocus();
			return;
		}
		spaceName = BWResource::resolveFilename( defaultSpacePath_ ) + "/" + s.GetBuffer();
	}
	else if ( BWResource::fileExists( spaceName + "/space.settings" ) )
	{
		std::string msg = std::string( "The folder \"" )
			+ spaceName + "\" already contains a space."
			"\nPlease use a different space name or select a different folder.";
		MessageBox( msg.c_str(), "Folder already contains a space", MB_ICONERROR );
		spacePath_.SetSel( 0, -1 ); // Select the space path field
		spacePath_.SetFocus();
		return;
	}
	else if ( BWResource::dissolveFilename( spaceName ) == spaceName )
	{
		std::string msg = std::string( "The folder \"" )
			+ p.GetBuffer() +
			"\" is not in a BigWorld path."
			"\nPlease correct the path, or select a the desired folder using the '...' button";
		MessageBox( msg.c_str(), "Folder not inside a BigWorld path.", MB_ICONERROR );
		spacePath_.SetSel( 0, -1 ); // Select the space path field
		spacePath_.SetFocus();
		return;
	}

	int width = atoi( w.GetBuffer() );
	int height = atoi( h.GetBuffer() );
	int minHeight = atoi( ny.GetBuffer() );
	int maxHeight = atoi( my.GetBuffer() );

	if ( minHeight > maxHeight )
	{
		int h = maxHeight;
		maxHeight = minHeight;
		minHeight = h;
	}

	SpaceMaker maker( spaceName,
		width, height, minHeight, maxHeight, progress_ );
	buttonCreate_.EnableWindow( FALSE );
	buttonCancel_.EnableWindow( FALSE );
	UpdateWindow();
	bool result = maker.create();
	buttonCreate_.EnableWindow( TRUE );
	buttonCancel_.EnableWindow( TRUE );
	if ( !result )
	{
		MessageBox( maker.errorMessage().c_str(), "Unable to create space", MB_ICONERROR );
	}
	else
	{
		createdSpace_ = BWResource::dissolveFilename( spaceName ).c_str();
		OnOK();
	}
}

void NewSpaceDlg::validateFile( CEdit& ctrl, bool isPath )
{
	static CString invalidFileChar = "\\/:*?\"<>| ";
	static CString invalidDirChar = "*?\"<>|";
	CString s;
	ctrl.GetWindowText( s );

	bool changed = false;
	for (int i = 0; i < s.GetLength(); ++i)
	{
		if (isupper(s[i]))
		{
			changed = true;
			s.SetAt(i, tolower(s[i]));
		}
		else if ( !isPath && ( s[i] == ':' || s[i] == '/' ) )
		{
			changed = true;
			s.SetAt(i, '_');
		}
		else if ( isPath && s[i] == '\\' )
		{
			changed = true;
			s.SetAt(i, '/');
		}
		else if ( isPath && invalidDirChar.Find( s[i] ) != -1 )
		{
			changed = true;
			s.SetAt(i, '_');
		}
		else if ( !isPath && invalidFileChar.Find( s[i] ) != -1 )
		{
			changed = true;
			s.SetAt(i, '_');
		}
	}

	if ( changed || s.GetLength() > MAX_PATH )
	{
		int start, end;
		ctrl.GetSel( start, end );
		s = s.Left( MAX_PATH );
		ctrl.SetWindowText( s );
		ctrl.SetSel( start, end );
	}
}

bool NewSpaceDlg::validateDim( CEdit& edit )
{
	CString str;
	edit.GetWindowText( str );

	int dim = atoi( str );
	if ( dim <= 0 || dim > NEWSPACEDLG_MAX_CHUNKS )
	{
		CString msg;
		if ( dim > NEWSPACEDLG_MAX_CHUNKS )
			msg = "Too many chunks. Please reduce the number of chunks.";
		else
			msg = "Cannot create a space with 0 chunks. Please increase the number of chunks.";

		MessageBox( msg, "Invalid chunk number", MB_ICONERROR );
		edit.SetSel( 0, -1 );
		edit.SetFocus();
		return false;
	}

	return true;
}

void NewSpaceDlg::validateDimChars( CEdit& edit )
{
	CString str;
	int ss, se;
	edit.GetSel( ss, se );

	edit.GetWindowText( str );

	bool changed = false;
	for( int i = 0; i < str.GetLength(); )
	{
		if ( str.GetAt( i ) < '0' || str.GetAt( i ) > '9' )
		{
			str = str.Left( i ) + str.Right( str.GetLength() - i - 1 );
			changed = true;
			if ( i <= ss )
				ss--;
			if ( i <= se )
				se--;
		}
		else
			++i;
	}

	if ( changed || str.GetLength() > NEWSPACEDLG_MAX_DIM_CHARS )
	{
		str = str.Left( NEWSPACEDLG_MAX_DIM_CHARS );
		edit.SetWindowText( str );
		edit.SetSel( ss, se );
	}
}

void NewSpaceDlg::formatChunkToKms( CString& str )
{
	int val = atoi( str );

	if ( val * GRID_RESOLUTION >= 1000 )
		str.Format( NEWSPACEDLG_KM_FORMAT, val * GRID_RESOLUTION / 1000.0f );
	else
		str.Format( NEWSPACEDLG_M_FORMAT, (int) ( val * GRID_RESOLUTION ) );
}

void NewSpaceDlg::OnEnChangeSpace()
{
	validateFile( space_, false );
}

void NewSpaceDlg::OnEnChangeSpacePath()
{
	validateFile( spacePath_, true );
}

void NewSpaceDlg::OnBnClickedNewspaceBrowse()
{
	DirDialog dlg;

	dlg.windowTitle_ = _T("New Space Folder");
	dlg.promptText_  = _T("Choose a folder for the new space...");
	dlg.fakeRootDirectory_ = dlg.basePath();

	CString curpath;
	spacePath_.GetWindowText( curpath );

	// Set the start directory, check if exists:
	if ( BWResource::instance().fileSystem()->getFileType(
			(LPCSTR)curpath ) == IFileSystem::FT_DIRECTORY )
		dlg.startDirectory_ = BWResource::resolveFilename(
			(LPCSTR)curpath ).c_str();
	else
		dlg.startDirectory_ = dlg.basePath();

	if (dlg.doBrowse(AfxGetApp()->m_pMainWnd)) 
	{
		spacePath_.SetWindowText( dlg.userSelectedDirectory_ );
	}
}

void NewSpaceDlg::OnEnChangeWidth()
{
	validateDimChars( width_ );

	CString str;

	width_.GetWindowText( str );

	formatChunkToKms( str );

	widthKms_.SetWindowText( str );
}

void NewSpaceDlg::OnEnChangeHeight()
{
	validateDimChars( height_ );

	CString str;

	height_.GetWindowText( str );

	formatChunkToKms( str );

	heightKms_.SetWindowText( str );
}
