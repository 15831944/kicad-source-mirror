/////////////////////////////////////////////////////////////////////////////

// Name:        sheet.cpp
// Purpose:
// Author:      jean-pierre Charras
// Modified by:
// Created:     08/02/2006 18:37:02
// RCS-ID:
// Copyright:   License GNU
// Licence:
/////////////////////////////////////////////////////////////////////////////

// Generated by DialogBlocks (unregistered), 08/02/2006 18:37:02

#if defined (__GNUG__) && !defined (NO_GCC_PRAGMA)
#pragma implementation "sheet.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
////@end includes
#include "fctsys.h"
#include "gr_basic.h"

#include "common.h"
#include "program.h"
#include "libcmp.h"
#include "general.h"

#include "protos.h"

/* Routines Locales */
static void ExitSheet( WinEDA_DrawPanel* Panel, wxDC* DC );
static void DeplaceSheet( WinEDA_DrawPanel* panel, wxDC* DC, bool erase );

/* Variables locales */
static int     s_SheetMindx, s_SheetMindy;
static wxPoint s_OldPos;    /* Ancienne pos pour annulation ReSize ou move */


#include "sheet.h"

////@begin XPM images
////@end XPM images

/*!
 * WinEDA_SheetPropertiesFrame type definition
 */

IMPLEMENT_DYNAMIC_CLASS( WinEDA_SheetPropertiesFrame, wxDialog )

/*!
 * WinEDA_SheetPropertiesFrame event table definition
 */

BEGIN_EVENT_TABLE( WinEDA_SheetPropertiesFrame, wxDialog )

////@begin WinEDA_SheetPropertiesFrame event table entries
    EVT_BUTTON( wxID_CANCEL, WinEDA_SheetPropertiesFrame::OnCancelClick )

    EVT_BUTTON( wxID_OK, WinEDA_SheetPropertiesFrame::OnOkClick )

////@end WinEDA_SheetPropertiesFrame event table entries

END_EVENT_TABLE()

/*!
 * WinEDA_SheetPropertiesFrame constructors
 */

WinEDA_SheetPropertiesFrame::WinEDA_SheetPropertiesFrame()
{
}


WinEDA_SheetPropertiesFrame::WinEDA_SheetPropertiesFrame( WinEDA_SchematicFrame* parent,
                                                          DrawSheetStruct*       currentsheet,
                                                          wxWindowID             id,
                                                          const wxString&        caption,
                                                          const wxPoint&         pos,
                                                          const wxSize&          size,
                                                          long                   style )
{
    m_Parent = parent;
    m_CurrentSheet = currentsheet;
    Create( parent, id, caption, pos, size, style );

    AddUnitSymbol( *m_SheetNameTextSize );
    PutValueInLocalUnits( *m_SheetNameSize, m_CurrentSheet->m_SheetNameSize,
        m_Parent->m_InternalUnits );

    AddUnitSymbol( *m_FileNameTextSize );
    PutValueInLocalUnits( *m_FileNameSize, m_CurrentSheet->m_FileNameSize,
        m_Parent->m_InternalUnits );
}


/*!
 * WinEDA_SheetPropertiesFrame creator
 */

bool WinEDA_SheetPropertiesFrame::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                          const wxPoint& pos, const wxSize& size, long style )
{
////@begin WinEDA_SheetPropertiesFrame member initialisation
    m_FileNameWin = NULL;
    m_SheetNameWin = NULL;
    m_FileNameTextSize = NULL;
    m_FileNameSize = NULL;
    m_SheetNameTextSize = NULL;
    m_SheetNameSize = NULL;
    m_btClose = NULL;
////@end WinEDA_SheetPropertiesFrame member initialisation

////@begin WinEDA_SheetPropertiesFrame creation
    SetExtraStyle(wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    if (GetSizer())
    {
        GetSizer()->SetSizeHints(this);
    }
    Centre();
////@end WinEDA_SheetPropertiesFrame creation
    return true;
}


/*!
 * Control creation for WinEDA_SheetPropertiesFrame
 */

void WinEDA_SheetPropertiesFrame::CreateControls()
{
    SetFont( *g_DialogFont );

////@begin WinEDA_SheetPropertiesFrame content construction
    // Generated by DialogBlocks, 29/04/2008 21:25:45 (unregistered)

    WinEDA_SheetPropertiesFrame* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer3, 0, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer4 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer3->Add(itemBoxSizer4, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _("Filename:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer4->Add(itemStaticText5, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxTOP|wxADJUST_MINSIZE, 5);

    m_FileNameWin = new wxTextCtrl( itemDialog1, ID_TEXTCTRL1, _T(""), wxDefaultPosition, wxSize(300, -1), wxTE_PROCESS_ENTER );
    itemBoxSizer4->Add(m_FileNameWin, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxStaticText* itemStaticText7 = new wxStaticText( itemDialog1, wxID_STATIC, _("Sheetname:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer4->Add(itemStaticText7, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxTOP|wxADJUST_MINSIZE, 5);

    m_SheetNameWin = new wxTextCtrl( itemDialog1, ID_TEXTCTRL, _T(""), wxDefaultPosition, wxSize(300, -1), 0 );
    itemBoxSizer4->Add(m_SheetNameWin, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxBoxSizer* itemBoxSizer9 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer3->Add(itemBoxSizer9, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    m_FileNameTextSize = new wxStaticText( itemDialog1, wxID_STATIC, _("Size"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer9->Add(m_FileNameTextSize, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxTOP|wxADJUST_MINSIZE, 5);

    m_FileNameSize = new wxTextCtrl( itemDialog1, ID_TEXTCTRL2, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer9->Add(m_FileNameSize, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    m_SheetNameTextSize = new wxStaticText( itemDialog1, wxID_STATIC, _("Size"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer9->Add(m_SheetNameTextSize, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxTOP|wxADJUST_MINSIZE, 5);

    m_SheetNameSize = new wxTextCtrl( itemDialog1, ID_TEXTCTRL3, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer9->Add(m_SheetNameSize, 0, wxALIGN_LEFT|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    itemBoxSizer2->Add(5, 5, 1, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer15 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer15, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    m_btClose = new wxButton( itemDialog1, wxID_CANCEL, _("&Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    m_btClose->SetForegroundColour(wxColour(0, 0, 255));
    itemBoxSizer15->Add(m_btClose, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton17 = new wxButton( itemDialog1, wxID_OK, _("&OK"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton17->SetDefault();
    itemButton17->SetForegroundColour(wxColour(196, 0, 0));
    itemBoxSizer15->Add(itemButton17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    // Set validators
    m_SheetNameWin->SetValidator( wxTextValidator(wxFILTER_NONE, & m_CurrentSheet->m_SheetName) );
////@end WinEDA_SheetPropertiesFrame content construction

    m_btClose->SetFocus();
    m_FileNameWin->SetValue( m_CurrentSheet->GetFileName() );
}


/*!
 * Should we show tooltips?
 */

bool WinEDA_SheetPropertiesFrame::ShowToolTips()
{
    return true;
}


/*!
 * Get bitmap resources
 */

wxBitmap WinEDA_SheetPropertiesFrame::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin WinEDA_SheetPropertiesFrame bitmap retrieval
    wxUnusedVar(name);
    return wxNullBitmap;
////@end WinEDA_SheetPropertiesFrame bitmap retrieval
}


/*!
 * Get icon resources
 */

wxIcon WinEDA_SheetPropertiesFrame::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin WinEDA_SheetPropertiesFrame icon retrieval
    wxUnusedVar(name);
    return wxNullIcon;
////@end WinEDA_SheetPropertiesFrame icon retrieval
}


/*****************************************************************************/
void WinEDA_SheetPropertiesFrame::SheetPropertiesAccept( wxCommandEvent& event )
/*****************************************************************************/

/** Function SheetPropertiesAccept
 * Set the new sheets properties:
 * sheetname and filename (text and size)
 */
{
    wxString FileName, msg;

    FileName = m_FileNameWin->GetValue();
    FileName.Trim( FALSE ); FileName.Trim( TRUE );

    if( FileName.IsEmpty() )
    {
        DisplayError( this, _( "No Filename! Aborted" ) );
        EndModal( FALSE );
        return;
    }

    ChangeFileNameExt( FileName, g_SchExtBuffer );

    /* m_CurrentSheet->m_AssociatedScreen must be a valide screen, and the sheet must have a valid associated filename,
     * so we must call m_CurrentSheet->ChangeFileName to set a filename,
     * AND always when a new sheet is created ( when m_CurrentSheet->m_AssociatedScreen is null ),
     * to create or set an Associated Screen
     */
    if( ( FileName != m_CurrentSheet->GetFileName() )
       || ( m_CurrentSheet->m_AssociatedScreen == NULL) )
    {
        msg = _( "Changing a Filename can change all the schematic structure and cannot be undone" );
        msg << wxT( "\n" );
        msg << _( "Ok to continue renaming?" );
        if( m_CurrentSheet->m_AssociatedScreen == NULL || IsOK( NULL, msg ) )
        { //do not prompt on a new sheet. in fact, we should not allow a sheet to be created
          //without a valid associated filename to be read from.
            m_Parent->GetScreen()->ClearUndoRedoList();
            m_CurrentSheet->ChangeFileName( m_Parent, FileName );   // set filename and the associated screen
        }
    }

    msg = m_FileNameSize->GetValue();
    m_CurrentSheet->m_FileNameSize =
        ReturnValueFromString( g_UnitMetric,
            msg, m_Parent->m_InternalUnits );

    m_CurrentSheet->m_SheetName = m_SheetNameWin->GetValue();
    msg = m_SheetNameSize->GetValue();
    m_CurrentSheet->m_SheetNameSize =
        ReturnValueFromString( g_UnitMetric,
            msg, m_Parent->m_InternalUnits );

    if( ( m_CurrentSheet->m_SheetName.IsEmpty() ) )
    {
        m_CurrentSheet->m_SheetName.Printf( wxT( "Sheet%8.8lX" ), GetTimeStamp() );
    }


    EndModal( TRUE );
}


/*************************************************************************/
bool WinEDA_SchematicFrame::EditSheet( DrawSheetStruct* Sheet, wxDC* DC )
/*************************************************************************/
/* Routine to edit the SheetName and the FileName for the sheet "Sheet" */
{
    WinEDA_SheetPropertiesFrame* frame;
    bool edit = TRUE;

    if( Sheet == NULL )
        return FALSE;

    /* Get the new texts */
    RedrawOneStruct( DrawPanel, DC, Sheet, g_XorMode );

    DrawPanel->m_IgnoreMouseEvents = TRUE;
    frame = new WinEDA_SheetPropertiesFrame( this, Sheet );
    edit  = frame->ShowModal(); frame->Destroy();
    DrawPanel->MouseToCursorSchema();
    DrawPanel->m_IgnoreMouseEvents = FALSE;

    RedrawOneStruct( DrawPanel, DC, Sheet, GR_DEFAULT_DRAWMODE );
    return edit;
}


#define SHEET_MIN_WIDTH  500
#define SHEET_MIN_HEIGHT 150
/****************************************************************/
DrawSheetStruct* WinEDA_SchematicFrame::CreateSheet( wxDC* DC )
/****************************************************************/
/* Routine de Creation d'une feuille de hierarchie (Sheet) */
{
    g_ItemToRepeat = NULL;

    DrawSheetStruct* Sheet = new DrawSheetStruct( GetScreen()->m_Curseur );

    Sheet->m_Flags     = IS_NEW | IS_RESIZED;
    Sheet->m_TimeStamp = GetTimeStamp();
    Sheet->m_Parent    = GetScreen();
    Sheet->m_AssociatedScreen = NULL;
    s_SheetMindx = SHEET_MIN_WIDTH;
    s_SheetMindy = SHEET_MIN_HEIGHT;

    //need to check if this is being added to the EEDrawList.
    //also need to update the hierarchy, if we are adding
    // a sheet to a screen that already has multiple instances (!)
    GetScreen()->SetCurItem( Sheet );

    DrawPanel->ManageCurseur = DeplaceSheet;
    DrawPanel->ForceCloseManageCurseur = ExitSheet;

    DrawPanel->ManageCurseur( DrawPanel, DC, FALSE );

    return Sheet;
}


/*******************************************************************************/
void WinEDA_SchematicFrame::ReSizeSheet( DrawSheetStruct* Sheet, wxDC* DC )
/*******************************************************************************/
{
    Hierarchical_PIN_Sheet_Struct* sheetlabel;

    if( Sheet == NULL )
        return;
    if( Sheet->m_Flags & IS_NEW )
        return;

    if( Sheet->Type() != DRAW_SHEET_STRUCT_TYPE )
    {
        DisplayError( this, wxT( "WinEDA_SchematicFrame::ReSizeSheet: Bad SructType" ) );
        return;
    }

    GetScreen()->SetModify();
    Sheet->m_Flags |= IS_RESIZED;

    /* sauvegarde des anciennes valeurs */
    s_OldPos.x = Sheet->m_Size.x;
    s_OldPos.y = Sheet->m_Size.y;

    /* Recalcul des dims min de la sheet */
    s_SheetMindx = SHEET_MIN_WIDTH;
    s_SheetMindy = SHEET_MIN_HEIGHT;
    sheetlabel   = Sheet->m_Label;
    while( sheetlabel )
    {
        s_SheetMindx = MAX( s_SheetMindx,
            (int) ( (sheetlabel->GetLength() +
                     1) * sheetlabel->m_Size.x ) );
        s_SheetMindy = MAX( s_SheetMindy, sheetlabel->m_Pos.y - Sheet->m_Pos.y );
        sheetlabel   = (Hierarchical_PIN_Sheet_Struct*) sheetlabel->Pnext;
    }

    DrawPanel->ManageCurseur = DeplaceSheet;
    DrawPanel->ForceCloseManageCurseur = ExitSheet;
    DrawPanel->ManageCurseur( DrawPanel, DC, TRUE );
}


/*********************************************************************************/
void WinEDA_SchematicFrame::StartMoveSheet( DrawSheetStruct* Sheet, wxDC* DC )
/*********************************************************************************/
{
    if( (Sheet == NULL) || ( Sheet->Type() != DRAW_SHEET_STRUCT_TYPE) )
        return;

    DrawPanel->CursorOff( DC );
    GetScreen()->m_Curseur = Sheet->m_Pos;
    DrawPanel->MouseToCursorSchema();

    s_OldPos = Sheet->m_Pos;
    Sheet->m_Flags |= IS_MOVED;
    DrawPanel->ManageCurseur = DeplaceSheet;
    DrawPanel->ForceCloseManageCurseur = ExitSheet;
    DrawPanel->ManageCurseur( DrawPanel, DC, TRUE );

    DrawPanel->CursorOn( DC );
}


/********************************************************/
/*	Routine de deplacement (move) Sheet, lie au curseur.*/
/*  Appele par GeneralControle grace a  ManageCurseur.  */
/********************************************************/
static void DeplaceSheet( WinEDA_DrawPanel* panel, wxDC* DC, bool erase )
{
    wxPoint               move_vector;
    Hierarchical_PIN_Sheet_Struct* SheetLabel;
    BASE_SCREEN*          screen = panel->GetScreen();

    DrawSheetStruct*      Sheet = (DrawSheetStruct*)
                                  screen->GetCurItem();

    /* Effacement du composant: tj apres depl curseur */
    if( erase )
        RedrawOneStruct( panel, DC, Sheet, g_XorMode );

    if( Sheet->m_Flags & IS_RESIZED )
    {
        Sheet->m_Size.x = MAX( s_SheetMindx, screen->m_Curseur.x - Sheet->m_Pos.x );
        Sheet->m_Size.y = MAX( s_SheetMindy, screen->m_Curseur.y - Sheet->m_Pos.y );
        SheetLabel = Sheet->m_Label;
        while( SheetLabel )
        {
            if( SheetLabel->m_Edge )
                SheetLabel->m_Pos.x = Sheet->m_Pos.x + Sheet->m_Size.x;
            SheetLabel = (Hierarchical_PIN_Sheet_Struct*) SheetLabel->Pnext;
        }
    }
    else             /* Move Sheet */
    {
        move_vector.x = screen->m_Curseur.x - Sheet->m_Pos.x;
        move_vector.y = screen->m_Curseur.y - Sheet->m_Pos.y;
        MoveOneStruct( Sheet, move_vector );
    }

    RedrawOneStruct( panel, DC, Sheet, g_XorMode );
}


/****************************************/
/*  Routine de sortie du Menu de Sheet  */
/****************************************/
static void ExitSheet( WinEDA_DrawPanel* Panel, wxDC* DC )
{
    SCH_SCREEN*      Screen = (SCH_SCREEN*) Panel->GetScreen();
    DrawSheetStruct* Sheet  = (DrawSheetStruct*) Screen->GetCurItem();

    if( Sheet == NULL )
        return;

    /* Deplacement composant en cours */
    if( Sheet->m_Flags & IS_NEW )             /* Nouveau Placement en cours, on l'efface */
    {
        RedrawOneStruct( Panel, DC, Sheet, g_XorMode );
        SAFE_DELETE( Sheet );
    }
    else if( Sheet->m_Flags & IS_RESIZED )             /* resize en cours: on l'annule */
    {
        RedrawOneStruct( Panel, DC, Sheet, g_XorMode );
        Sheet->m_Size.x = s_OldPos.x;
        Sheet->m_Size.y = s_OldPos.y;
        RedrawOneStruct( Panel, DC, Sheet, GR_DEFAULT_DRAWMODE );
        Sheet->m_Flags = 0;
    }
    else if( Sheet->m_Flags & IS_MOVED )             /* move en cours: on l'annule */
    {
        wxPoint curspos = Screen->m_Curseur;
        Panel->GetScreen()->m_Curseur = s_OldPos;
        DeplaceSheet( Panel, DC, TRUE );
        RedrawOneStruct( Panel, DC, Sheet, GR_DEFAULT_DRAWMODE );
        Sheet->m_Flags    = 0;
        Screen->m_Curseur = curspos;
    }
    else
        Sheet->m_Flags = 0;

    Screen->SetCurItem( NULL );
    Panel->ManageCurseur = NULL;
    Panel->ForceCloseManageCurseur = NULL;
}


/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL
 */

void WinEDA_SheetPropertiesFrame::OnCancelClick( wxCommandEvent& event )
{
    EndModal( 0 );
}


/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK
 */

void WinEDA_SheetPropertiesFrame::OnOkClick( wxCommandEvent& event )
{
    SheetPropertiesAccept( event );
}
