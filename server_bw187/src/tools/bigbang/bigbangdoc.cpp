/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// bigbang2doc.cpp : implementation of the BigBang2Doc class
//

#include "pch.hpp"
#include "bigbang.h"

#include "bigbangdoc.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// BigBang2Doc
BigBang2Doc * BigBang2Doc::s_instance_ = NULL;


IMPLEMENT_DYNCREATE(BigBang2Doc, CDocument)

BEGIN_MESSAGE_MAP(BigBang2Doc, CDocument)
END_MESSAGE_MAP()


// BigBang2Doc construction/destruction

BigBang2Doc::BigBang2Doc()
{
	// TODO: add one-time construction code here
}

BigBang2Doc::~BigBang2Doc()
{
	s_instance_ = NULL;
}

BOOL BigBang2Doc::OnNewDocument()
{
	ASSERT(!s_instance_);
	s_instance_ = this;

	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}

BOOL BigBang2Doc::OnOpenDocument(LPCTSTR lpszPathName)
{
	ASSERT(!s_instance_);
	s_instance_ = this;
	return TRUE;
}

void BigBang2Doc::OnCloseDocument()
{
	// TODO: Add your specialized code here and/or call the base class
	CDocument::OnCloseDocument();
}




// BigBang2Doc serialization

void BigBang2Doc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}


// BigBang2Doc diagnostics

#ifdef _DEBUG
void BigBang2Doc::AssertValid() const
{
	CDocument::AssertValid();
}

void BigBang2Doc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// BigBang2Doc commands
