/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ULTILITIES_HPP
#define ULTILITIES_HPP

#include <string>
#include <vector>
#include <set>

class Utilities
{
public:
#ifdef _MFC_VER
	static void stretchToRight( CWnd* parent, CWnd& widget, int pageWidth, int border );
	static void stretchToBottomRight( CWnd* parent, CWnd& widget, int pageWidth, int bx, int pageHeight, int by );

	static void moveToRight( CWnd* parent, CWnd& widget, int pageWidth, int border );
	static void moveToBottom( CWnd* parent, CWnd& widget, int pageHeight, int border );

	static void moveToTopRight( CWnd* parent, CWnd& widget, int pageWidth, int bx, int py );
	static void moveToBottomLeft( CWnd* parent, CWnd& widget, int pageHeight, int by, int px );

	static void centre( CWnd* parent, CWnd& widget, int pageWidth );

	static void fieldEnabledState( CEdit& field, bool enable, const std::string& text = "" );
#endif // _MFC_VER

	static std::string pythonSafeName( const std::string& s );
	static std::string memorySizeToStr( uint32 memorySizeInBytes );

    static std::set<std::string> gatherFilesInDirectory( const std::string & dir,
        const std::string & pattern, const std::string & prefix = "",
        bool ignoreDotFiles = true, bool recursive = true );
	
};

#endif // ULTILITIES_HPP