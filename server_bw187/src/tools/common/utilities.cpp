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
#include "utilities.hpp"

#ifdef _MFC_VER

/*static*/ void Utilities::stretchToRight( CWnd* parent, CWnd& widget, int pageWidth, int border )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	widget.SetWindowPos( 0, rect.left, rect.top, pageWidth - rect.left - border, rect.Height(), SWP_NOZORDER );
}

/*static*/ void Utilities::stretchToBottomRight( CWnd* parent, CWnd& widget, int pageWidth, int bx, int pageHeight, int by )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	widget.SetWindowPos( 0, rect.left, rect.top, pageWidth - rect.left - bx, pageHeight - rect.top - by, SWP_NOZORDER );
}

/*static*/ void Utilities::moveToRight( CWnd* parent, CWnd& widget, int pageWidth, int border )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	int width = rect.right - rect.left;
	widget.SetWindowPos( 0, pageWidth - width - border , rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

/*static*/ void Utilities::moveToBottom( CWnd* parent, CWnd& widget, int pageHeight, int border )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	int height = rect.bottom - rect.top;
	widget.SetWindowPos( 0, rect.left, pageHeight - height - border, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

/*static*/ void Utilities::moveToTopRight( CWnd* parent, CWnd& widget, int pageWidth, int bx, int py )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	int width = rect.right - rect.left;
	widget.SetWindowPos( 0, pageWidth - bx , py, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

/*static*/ void Utilities::moveToBottomLeft( CWnd* parent, CWnd& widget, int pageHeight, int by, int px )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	int height = rect.bottom - rect.top;
	widget.SetWindowPos( 0, px , pageHeight - height - by, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

/*static*/ void Utilities::centre( CWnd* parent, CWnd& widget, int pageWidth )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    parent->ScreenToClient( &rect );
	int width = rect.right - rect.left;
	widget.SetWindowPos( 0, pageWidth/2 - width/2 , rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

/*static*/ void Utilities::fieldEnabledState( CEdit& field, bool enable, const std::string& text )
{
	field.SetWindowText( text.c_str() );
	field.SetReadOnly( !enable );
	field.ModifyStyle( enable ? WS_DISABLED : 0, enable ? 0 : WS_DISABLED );
}

#endif // _MFC_VER

/*static*/ std::string Utilities::pythonSafeName( const std::string& s )
{
    std::string r;

    bool isFirst = true;

	std::string::const_iterator i = s.begin();
	for (; i != s.end(); ++i)
    {
        if ((isFirst) && ((isdigit( *i )) || (*i == ' ') || (*i == '_')))
			continue;
		
		char c = tolower( *i );

        if ((c >= 'a' && c <= 'z') || isdigit( c ) || (c == '_'))
        {
			r.push_back( *i );
        }
        else if (c == ' ')
            r.push_back('_');

		isFirst = false;
    }

	return r;
}

/*static*/ std::string Utilities::memorySizeToStr( uint32 memorySizeInBytes )
{
    std::string result;
    int suffixIndex = 0;
    float memorySize = (float)memorySizeInBytes;

    while ( ( memorySize >= 1000.f ) && ( suffixIndex < 4 ))
    {
        memorySize /= 1024.f;
        suffixIndex++;
    }

	char buf[256];
	sprintf( buf, "%.1f", memorySize );
	result = std::string( buf );

    switch ( suffixIndex )
    {
        case 0:
            result += " Bytes";
            break;
        case 1:
            result += " KB";
            break;
        case 2:
            result += " MB";
            break;
        case 3:
            result += " GB";
            break;
        case 4:
            result += " TB";
            break;
    }

    return result;
}

/*static*/ std::set<std::string> 
Utilities::gatherFilesInDirectory
( 
    const std::string &     dir,
	const std::string &     pattern,
	const std::string &     prefix              /*= ""*/,
	bool                    ignoreDotFiles      /*= true*/,
	bool                    recursive           /*= true*/
)
{
	WIN32_FIND_DATA fileData;

	std::string search = dir + "\\" + pattern;

	std::set<std::string> result;

	HANDLE hFind = FindFirstFile( search.c_str(), &fileData );

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!ignoreDotFiles || fileData.cFileName[0] != '.')
			{
				if( ( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
				{
					*strchr( fileData.cFileName, '.' ) = 0;
					result.insert( prefix + fileData.cFileName );
				}
			}
		}
		while (FindNextFile( hFind, &fileData ));

		FindClose(hFind);
	}

	if( recursive )
	{
		search = dir + "\\*.*";

		hFind = FindFirstFile( search.c_str(), &fileData );

		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if ( ( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) &&
					fileData.cFileName != std::string( "." ) &&
					fileData.cFileName != std::string( ".." ) )
				{
					std::set<std::string> temp = gatherFilesInDirectory( dir + '\\' + fileData.cFileName,
						pattern, prefix + fileData.cFileName + "/", ignoreDotFiles, true );
					result.insert( temp.begin(), temp.end() );
				}
			}
			while (FindNextFile( hFind, &fileData ));

			FindClose(hFind);
		}
	}

	return result;
}
