/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// bigbang2doc.hpp : interface of the BigBang2Doc class
//


#pragma once

class BigBang2Doc : public CDocument
{
protected: // create from serialization only
	BigBang2Doc();
	DECLARE_DYNCREATE(BigBang2Doc)

// Attributes
public:

// Operations
public:

// Overrides
	public:
	virtual BOOL OnNewDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual void OnCloseDocument();

	virtual void Serialize(CArchive& ar);

// Implementation
public:
	virtual ~BigBang2Doc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	static BigBang2Doc& instance() { return *s_instance_; }

protected:
	static BigBang2Doc * s_instance_;

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};
