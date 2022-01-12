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
#include "particle_editor.hpp"
#include "main_frame.hpp"
#include "gui/propdlgs/psa_collide_properties.hpp"
#include "controls/dir_dialog.hpp"
#include "particle/actions/collide_psa.hpp"

using namespace std;

namespace 
{
    bool validSoundTag(CString const &filename__)
    {
        return true;
    }
}

DECLARE_DEBUG_COMPONENT2("GUI", 0)

IMPLEMENT_DYNCREATE(PsaCollideProperties, PsaProperties)

BEGIN_MESSAGE_MAP(PsaCollideProperties, PsaProperties)
    ON_BN_CLICKED(IDC_PSA_COLLIDE_SOUND_ENABLED, OnBnClickedPsaCollideSoundEnabled)
    ON_BN_CLICKED(IDC_PSA_COLLIDE_SOUND_TAG_DIRECTORY_BTN, OnBnClickedPsaCollideSoundTagDirectory)
    ON_CBN_SELCHANGE(IDC_PSA_COLLIDE_SOUND_TAG_COMBO, OnCbnSelchangePsaCollideSoundTagCombo)
END_MESSAGE_MAP()

PsaCollideProperties::PsaCollideProperties()
:
PsaProperties(PsaCollideProperties::IDD)
{
    soundSrcIdx_.SetNumericType(controls::EditNumeric::ENT_INTEGER);
}

PsaCollideProperties::~PsaCollideProperties()
{
}

void PsaCollideProperties::OnInitialUpdate()
{
    PsaProperties::OnInitialUpdate();
    soundTagDirectoryBtn_.setBitmapID(IDB_OPEN, IDB_OPEND);
}

void PsaCollideProperties::DoDataExchange(CDataExchange* pDX)
{
    PsaProperties::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PSA_COLLIDE_ELASTICITY, elasticity_);
    DDX_Control(pDX, IDC_PSA_COLLIDE_SOUND_ENABLED           , soundEnabled_         );
    DDX_Control(pDX, IDC_PSA_COLLIDE_SOUND_TAG_COMBO         , soundTag_             );
    DDX_Control(pDX, IDC_PSA_COLLIDE_SOUND_TAG_DIRECTORY_BTN , soundTagDirectoryBtn_ );
    DDX_Control(pDX, IDC_PSA_COLLIDE_SOUND_TAG_DIRECTORY_EDIT, soundTagDirectoryEdit_);
    DDX_Control(pDX, IDC_PSA_COLLIDE_SOUND_IDX               , soundSrcIdx_          );
}

void PsaCollideProperties::SetParameters(SetOperation task)
{
    ASSERT(action_);

    SET_FLOAT_PARAMETER(task, elasticity  );
    SET_CHECK_PARAMETER(task, soundEnabled);
    SET_INT_PARAMETER  (task, soundSrcIdx );

    if (task == SET_CONTROL)
    {
        CollidePSA *collideAction = action();
        string     soundTag       = collideAction->soundTag();
        int id = soundTag_.SelectString(-1, soundTag.c_str());
        if (id == CB_ERR)
        {
            id = soundTag_.AddString(soundTag.c_str());
            soundTag_.SetCurSel(id);
        }
        UpdateState();
    }
}

void PsaCollideProperties::UpdateState()
{
    bool enableWindow = (soundEnabled_.GetCheck() == BST_CHECKED);
    soundTag_            .EnableWindow(enableWindow ? TRUE : FALSE);
    soundTagDirectoryBtn_.EnableWindow(enableWindow ? TRUE : FALSE);
    soundSrcIdx_         .EnableWindow(enableWindow ? TRUE : FALSE);
}

void PsaCollideProperties::OnBnClickedPsaCollideSoundEnabled()
{
	MainFrame::instance()->PotentiallyDirty
    (
        true,
        UndoRedoOp::AK_PARAMETER,
        "Change to Sprite Renderer"
    );
    CollidePSA *collideAction = action();
    collideAction->soundEnabled(!collideAction->soundEnabled());
    UpdateState();
}

void PsaCollideProperties::OnCbnSelchangePsaCollideSoundTagCombo()
{
    MainFrame::instance()->PotentiallyDirty
    (
        true,
        UndoRedoOp::AK_PARAMETER,
        "Change to Sprite Renderer"
    );
    string selection;
    if (soundTag_.GetCurSel() != -1)
    {
        CString csoundTag;
        soundTag_.GetWindowText(csoundTag);
        selection = csoundTag.GetBuffer();
    }
    CollidePSA *collideAction = action();
    collideAction->soundTag(selection);
}

void PsaCollideProperties::OnBnClickedPsaCollideSoundTagDirectory()
{
	DirDialog dlg; 

	dlg.windowTitle_       = "Open";
	dlg.promptText_        = "Choose directory..";
	dlg.fakeRootDirectory_ = dlg.basePath();

	CString startDir;
	soundTagDirectoryEdit_.GetWindowText(startDir);
    dlg.startDirectory_ 
        = BWResource::resolveFilename(startDir.GetBuffer()).c_str();

	if (dlg.doBrowse(AfxGetApp()->m_pMainWnd)) 
	{
		dlg.userSelectedDirectory_ += "/";
		std::string relativeDirectory = 
            BWResource::dissolveFilename
            (
                dlg.userSelectedDirectory_.GetBuffer()
            );
		soundTagDirectoryEdit_.SetWindowText(relativeDirectory.c_str());
		PopulateComboBoxWithFilenames
        (
            soundTag_, 
            relativeDirectory, 
            validSoundTag
        );
		soundTag_.SetCurSel(-1);
	}
}
