/////////////////////////////////////////////////////////////////////////////

// Name:        plotps.cpp
// Purpose:
// Author:      jean-pierre Charras
// Modified by:
// Created:     01/02/2006 08:37:24
// RCS-ID:
// Copyright:   GNU License
// License:
/////////////////////////////////////////////////////////////////////////////

// Generated by DialogBlocks (unregistered), 01/02/2006 08:37:24

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "plotps.h"
#endif

#include "fctsys.h"
#include "gr_basic.h"
#include "common.h"
#include "confirm.h"
#include "worksheet.h"
#include "plot_common.h"

#include "program.h"
#include "general.h"
#include "protos.h"

enum PageFormatReq {
    PAGE_SIZE_AUTO,
    PAGE_SIZE_A4,
    PAGE_SIZE_A
};

/* Variables locales : */
static int  PS_SizeSelect  = PAGE_SIZE_AUTO;
static bool Plot_Sheet_Ref = TRUE;

#include "plotps.h"

////@begin XPM images
////@end XPM images


void WinEDA_SchematicFrame::ToPlot_PS( wxCommandEvent& event )
{
    wxPoint pos;

    pos = GetPosition();

    pos.x += 10;
    pos.y += 20;

    WinEDA_PlotPSFrame* Ps_frame = new WinEDA_PlotPSFrame( this );

    Ps_frame->ShowModal();
    Ps_frame->Destroy();
}


/*!
 * WinEDA_PlotPSFrame type definition
 */

IMPLEMENT_DYNAMIC_CLASS( WinEDA_PlotPSFrame, wxDialog )

/*!
 * WinEDA_PlotPSFrame event table definition
 */

BEGIN_EVENT_TABLE( WinEDA_PlotPSFrame, wxDialog )

////@begin WinEDA_PlotPSFrame event table entries
    EVT_BUTTON( ID_PLOT_PS_CURRENT_EXECUTE,
                WinEDA_PlotPSFrame::OnPlotPsCurrentExecuteClick )

    EVT_BUTTON( ID_PLOT_PS_ALL_EXECUTE,
                WinEDA_PlotPSFrame::OnPlotPsAllExecuteClick )

    EVT_BUTTON( wxID_CANCEL, WinEDA_PlotPSFrame::OnCancelClick )

////@end WinEDA_PlotPSFrame event table entries

END_EVENT_TABLE()
/*!
 * WinEDA_PlotPSFrame constructors
 */

WinEDA_PlotPSFrame::WinEDA_PlotPSFrame()
{
}


WinEDA_PlotPSFrame::WinEDA_PlotPSFrame( WinEDA_DrawFrame* parent,
                                        wxWindowID        id,
                                        const wxString&   caption,
                                        const wxPoint&    pos,
                                        const wxSize&     size,
                                        long              style )
{
    m_Parent = parent;
    PlotPSColorOpt = false;
    Create( parent, id, caption, pos, size, style );
}


/*!
 * WinEDA_PlotPSFrame creator
 */

bool WinEDA_PlotPSFrame::Create( wxWindow*       parent,
                                 wxWindowID      id,
                                 const wxString& caption,
                                 const wxPoint&  pos,
                                 const wxSize&   size,
                                 long            style )
{
////@begin WinEDA_PlotPSFrame member initialization
    m_SizeOption = NULL;
    m_PlotPSColorOption = NULL;
    m_Plot_Sheet_Ref    = NULL;
    m_btClose = NULL;
    m_DefaultLineSizeCtrlSizer = NULL;
    m_MsgBox = NULL;

////@end WinEDA_PlotPSFrame member initialisation

////@begin WinEDA_PlotPSFrame creation
    SetExtraStyle( wxWS_EX_BLOCK_EVENTS );
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    if( GetSizer() )
    {
        GetSizer()->SetSizeHints( this );
    }
    Centre();

////@end WinEDA_PlotPSFrame creation
    return true;
}


/*!
 * Control creation for WinEDA_PlotPSFrame
 */

void WinEDA_PlotPSFrame::CreateControls()
{
////@begin WinEDA_PlotPSFrame content construction
    // Generated by DialogBlocks, 24/04/2009 14:25:24 (unregistered)

    WinEDA_PlotPSFrame* itemDialog1 = this;

    wxBoxSizer*         itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );

    itemDialog1->SetSizer( itemBoxSizer2 );

    wxBoxSizer*   itemBoxSizer3 = new wxBoxSizer( wxHORIZONTAL );
    itemBoxSizer2->Add( itemBoxSizer3, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5 );

    wxArrayString m_SizeOptionStrings;
    m_SizeOptionStrings.Add( _( "Auto" ) );
    m_SizeOptionStrings.Add( _( "Page Size A4" ) );
    m_SizeOptionStrings.Add( _( "Page Size A" ) );
    m_SizeOption =
        new wxRadioBox( itemDialog1, ID_RADIOBOX1, _( "Plot page size:" ),
                        wxDefaultPosition, wxDefaultSize, m_SizeOptionStrings,
                        1, wxRA_SPECIFY_COLS );
    m_SizeOption->SetSelection( 0 );
    itemBoxSizer3->Add( m_SizeOption, 0, wxGROW | wxALL, 5 );

    itemBoxSizer3->Add( 5, 5, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    wxStaticBox*      itemStaticBoxSizer6Static =
        new wxStaticBox( itemDialog1, wxID_ANY, _( "Plot Options:" ) );
    wxStaticBoxSizer* itemStaticBoxSizer6 =
        new wxStaticBoxSizer( itemStaticBoxSizer6Static, wxVERTICAL );
    itemBoxSizer3->Add( itemStaticBoxSizer6,
                        0,
                        wxALIGN_CENTER_VERTICAL | wxALL,
                        5 );

    wxArrayString m_PlotPSColorOptionStrings;
    m_PlotPSColorOptionStrings.Add( _( "B/W" ) );
    m_PlotPSColorOptionStrings.Add( _( "Color" ) );
    m_PlotPSColorOption =
        new wxRadioBox( itemDialog1, ID_RADIOBOX, _( "Plot Color:" ),
                        wxDefaultPosition, wxDefaultSize,
                        m_PlotPSColorOptionStrings, 1, wxRA_SPECIFY_COLS );
    m_PlotPSColorOption->SetSelection( 0 );
    itemStaticBoxSizer6->Add( m_PlotPSColorOption, 0, wxGROW | wxALL, 5 );

    m_Plot_Sheet_Ref =
        new wxCheckBox( itemDialog1, ID_CHECKBOX, _( "Print Sheet Ref" ),
                        wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    m_Plot_Sheet_Ref->SetValue( false );
    itemStaticBoxSizer6->Add( m_Plot_Sheet_Ref, 0, wxGROW | wxALL, 5 );

    itemBoxSizer3->Add( 5, 5, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    wxBoxSizer* itemBoxSizer10 = new wxBoxSizer( wxVERTICAL );
    itemBoxSizer3->Add( itemBoxSizer10, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    wxButton*   itemButton11 = new wxButton( itemDialog1,
                                             ID_PLOT_PS_CURRENT_EXECUTE,
                                             _( "&Plot Page" ),
                                             wxDefaultPosition,
                                             wxDefaultSize,
                                             0 );
    itemButton11->SetDefault();
    itemBoxSizer10->Add( itemButton11, 0, wxGROW | wxALL, 5 );

    wxButton* itemButton12 = new wxButton( itemDialog1,
                                           ID_PLOT_PS_ALL_EXECUTE,
                                           _( "Plot A&LL" ),
                                           wxDefaultPosition,
                                           wxDefaultSize,
                                           0 );
    itemBoxSizer10->Add( itemButton12, 0, wxGROW | wxALL, 5 );

    m_btClose = new wxButton( itemDialog1, wxID_CANCEL, _( "Close" ),
                              wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer10->Add( m_btClose, 0, wxGROW | wxALL, 5 );

    m_DefaultLineSizeCtrlSizer = new wxBoxSizer( wxVERTICAL );
    itemBoxSizer2->Add( m_DefaultLineSizeCtrlSizer, 0, wxGROW | wxALL, 5 );

    wxStaticText* itemStaticText15 = new wxStaticText( itemDialog1,
                                                       wxID_STATIC,
                                                       _( "Messages :" ),
                                                       wxDefaultPosition,
                                                       wxDefaultSize,
                                                       0 );
    itemBoxSizer2->Add( itemStaticText15,
                        0,
                        wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP |
                        wxADJUST_MINSIZE,
                        5 );

    m_MsgBox = new wxTextCtrl( itemDialog1, ID_TEXTCTRL, _T( "" ),
                               wxDefaultPosition, wxSize( -1, 200 ),
                               wxTE_MULTILINE );
    itemBoxSizer2->Add( m_MsgBox, 0, wxGROW | wxALL | wxFIXED_MINSIZE, 5 );

    // Set validators
    m_SizeOption->SetValidator( wxGenericValidator( &PS_SizeSelect ) );
    m_PlotPSColorOption->SetValidator( wxGenericValidator( &PlotPSColorOpt ) );
    m_Plot_Sheet_Ref->SetValidator( wxGenericValidator( &Plot_Sheet_Ref ) );

////@end WinEDA_PlotPSFrame content construction

    SetFocus(); // make the ESC work
    m_DefaultLineSizeCtrl = new WinEDA_ValueCtrl( this,
                                                  _( "Default Line Width" ),
                                                  g_DrawDefaultLineThickness,
                                                  g_UnitMetric,
                                                  m_DefaultLineSizeCtrlSizer,
                                                  EESCHEMA_INTERNAL_UNIT );
}


/*!
 * Should we show tooltips?
 */

bool WinEDA_PlotPSFrame::ShowToolTips()
{
    return true;
}


/*!
 * Get bitmap resources
 */

wxBitmap WinEDA_PlotPSFrame::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin WinEDA_PlotPSFrame bitmap retrieval
    wxUnusedVar( name );
    return wxNullBitmap;

////@end WinEDA_PlotPSFrame bitmap retrieval
}


/*!
 * Get icon resources
 */

wxIcon WinEDA_PlotPSFrame::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin WinEDA_PlotPSFrame icon retrieval
    wxUnusedVar( name );
    return wxNullIcon;

////@end WinEDA_PlotPSFrame icon retrieval
}


/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_BUTTON
 */

void WinEDA_PlotPSFrame::OnPlotPsCurrentExecuteClick( wxCommandEvent& event )
{
    int Select_PlotAll = FALSE;

    InitOptVars();
    CreatePSFile( Select_PlotAll, PS_SizeSelect );
    m_MsgBox->AppendText( wxT( "*****\n" ) );
}


/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_BUTTON1
 */

void WinEDA_PlotPSFrame::OnPlotPsAllExecuteClick( wxCommandEvent& event )
{
    int Select_PlotAll = TRUE;

    InitOptVars();
    CreatePSFile( Select_PlotAll, PS_SizeSelect );
    m_MsgBox->AppendText( wxT( "*****\n" ) );
}


/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL
 */

void WinEDA_PlotPSFrame::OnCancelClick( wxCommandEvent& event )
{
    InitOptVars();
    EndModal( 0 );
}


void WinEDA_PlotPSFrame::InitOptVars()
{
    Plot_Sheet_Ref = m_Plot_Sheet_Ref->GetValue();
    PlotPSColorOpt = m_PlotPSColorOption->GetSelection();
    PS_SizeSelect  = m_SizeOption->GetSelection();
    g_DrawDefaultLineThickness = m_DefaultLineSizeCtrl->GetValue();
    if( g_DrawDefaultLineThickness < 1 )
        g_DrawDefaultLineThickness = 1;
}


void WinEDA_PlotPSFrame::CreatePSFile( int AllPages, int pagesize )
{
    WinEDA_SchematicFrame* schframe  = (WinEDA_SchematicFrame*) m_Parent;
    SCH_SCREEN*            screen    = schframe->GetScreen();
    SCH_SCREEN*            oldscreen = screen;
    DrawSheetPath*         sheetpath, * oldsheetpath = schframe->GetSheet();
    wxString PlotFileName;
    Ki_PageDescr*          PlotSheet, * RealSheet;
    wxPoint plot_offset;

    /* When printing all pages, the printed page is not the current page.
     *  In complex hierarchies, we must setup references and others parameters
     *   in the printed SCH_SCREEN
     *  because in complex hierarchies a SCH_SCREEN (a schematic drawings)
     *  is shared between many sheets
     */
    EDA_SheetList SheetList( NULL );

    sheetpath = SheetList.GetFirst();
    DrawSheetPath list;

    while( true )
    {
        if( AllPages )
        {
            if( sheetpath == NULL )
                break;
            list.Clear();
            if( list.BuildSheetPathInfoFromSheetPathValue( sheetpath->Path() ) )
            {
                schframe->m_CurrentSheet = &list;
                schframe->m_CurrentSheet->UpdateAllScreenReferences();
                schframe->SetSheetNumberAndCount();
                screen = schframe->m_CurrentSheet->LastScreen();
                ActiveScreen = screen;
            }
            else  // Should not happen
                return;
            sheetpath = SheetList.GetNext();
        }
        PlotSheet = screen->m_CurrentSheetDesc;
        switch( pagesize )
        {
        case PAGE_SIZE_A:
            RealSheet = &g_Sheet_A;
            break;

        case PAGE_SIZE_A4:
            RealSheet = &g_Sheet_A4;
            break;

        case PAGE_SIZE_AUTO:
        default:
            RealSheet = PlotSheet;
            break;
        }

        double scalex = (double) RealSheet->m_Size.x / PlotSheet->m_Size.x;
        double scaley = (double) RealSheet->m_Size.y / PlotSheet->m_Size.y;
        double scale  = 10 * MIN( scalex, scaley );

        plot_offset.x = 0;
        plot_offset.y = 0;

        PlotFileName = schframe->GetUniqueFilenameForCurrentSheet() + wxT(
            ".ps" );

        PlotOneSheetPS( PlotFileName, screen, RealSheet, plot_offset, scale );

        if( !AllPages )
            break;
    }

    ActiveScreen = oldscreen;
    schframe->m_CurrentSheet = oldsheetpath;
    schframe->m_CurrentSheet->UpdateAllScreenReferences();
    schframe->SetSheetNumberAndCount();
}


void WinEDA_PlotPSFrame::PlotOneSheetPS( const wxString& FileName,
                                         SCH_SCREEN*     screen,
                                         Ki_PageDescr*   sheet,
                                         wxPoint         plot_offset,
                                         double          scale )
{
    wxString msg;

    FILE*    output_file = wxFopen( FileName, wxT( "wt" ) );

    if( output_file == NULL )
    {
        msg  = wxT( "\n** " );
        msg += _( "Unable to create " ) + FileName + wxT( " **\n\n" );
        m_MsgBox->AppendText( msg );
        wxBell();
        return;
    }

    SetLocaleTo_C_standard();
    msg.Printf( _( "Plot: %s\n" ), FileName.GetData() );
    m_MsgBox->AppendText( msg );

    PS_PLOTTER* plotter = new PS_PLOTTER();
    plotter->set_paper_size( sheet );
    plotter->set_viewport( plot_offset, scale, 0 );
    plotter->set_default_line_width( g_DrawDefaultLineThickness );
    plotter->set_color_mode( PlotPSColorOpt );

    /* Init : */
    plotter->set_creator( wxT( "EESchema-PS" ) );
    plotter->set_filename( FileName );
    plotter->start_plot( output_file );

    if( Plot_Sheet_Ref )
    {
        plotter->set_color( BLACK );
        m_Parent->PlotWorkSheet( plotter, screen );
    }

    PlotDrawlist( plotter, screen->EEDrawList );

    plotter->end_plot();
    delete plotter;
    SetLocaleTo_Default();

    m_MsgBox->AppendText( wxT( "Ok\n" ) );
}
