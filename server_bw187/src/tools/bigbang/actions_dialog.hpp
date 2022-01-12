/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once
#include "afxwin.h"

#include "property_list.hpp"
#include "worldeditor/editor_chunk_entity.hpp"


// ActionsDialog dialog

class ActionsDialog : public CDialog
{
	DECLARE_DYNAMIC(ActionsDialog)

public:
	ActionsDialog(EditorChunkEntity * entity, CWnd* pParent = NULL);   // standard constructor
	virtual ~ActionsDialog();

	virtual BOOL OnInitDialog();

// Dialog Data
	enum { IDD = IDD_DIALOG_ACTIONS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

private:
	EditorChunkEntity * edEntity_;

	void loadCurrentActions();

	typedef std::pair< DataSectionPtr, DataSectionPtr > EventActionSections;
	typedef std::map< int, EventActionSections > IndexSectionMap;
	IndexSectionMap listIndexMap_;

	std::vector<uint> actionIndexes_;

	PropertyList currentActions_;
	CComboBox events_;
	CComboBox actions_;

	afx_msg void OnBnClickedActionDelete();
	afx_msg void OnBnClickedActionAdd();
	afx_msg void OnBnClickedActionChangeEvent();
	afx_msg void OnBnClickedOk();
};
