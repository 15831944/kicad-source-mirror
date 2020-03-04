/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 CERN
 * Copyright (C) 2015-2018 KiCad Developers, see AUTHORS.txt for contributors.
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>

#include "wx_html_report_panel.h"

#include <wildcards_and_files_ext.h>
#include <gal/color4d.h>
#include <wx/clipbrd.h>

WX_HTML_REPORT_PANEL::WX_HTML_REPORT_PANEL( wxWindow*      parent,
                                            wxWindowID     id,
                                            const wxPoint& pos,
                                            const wxSize&  size,
                                            long           style ) :
    WX_HTML_REPORT_PANEL_BASE( parent, id, pos, size, style ),
    m_reporter( this ),
    m_severities( -1 ),
    m_lazyUpdate( false )
{
    syncCheckboxes();
    m_htmlView->SetPage( addHeader( "" ) );

    Connect( wxEVT_COMMAND_MENU_SELECTED,
            wxMenuEventHandler( WX_HTML_REPORT_PANEL::onMenuEvent ), NULL, this );
}


WX_HTML_REPORT_PANEL::~WX_HTML_REPORT_PANEL()
{
}


void WX_HTML_REPORT_PANEL::MsgPanelSetMinSize( const wxSize& aMinSize )
{
    m_fgSizer->SetMinSize( aMinSize );
    GetSizer()->SetSizeHints( this );
}


REPORTER& WX_HTML_REPORT_PANEL::Reporter()
{
    return m_reporter;
}


void WX_HTML_REPORT_PANEL::Report( const wxString& aText, SEVERITY aSeverity,
                                   REPORTER::LOCATION aLocation )
{
    REPORT_LINE line;
    line.message = aText;
    line.severity = aSeverity;

    if( aLocation == REPORTER::LOC_HEAD )
        m_reportHead.push_back( line );
    else if( aLocation == REPORTER::LOC_TAIL )
        m_reportTail.push_back( line );
    else
        m_report.push_back( line );

    if( !m_lazyUpdate )
    {
        m_htmlView->AppendToPage( generateHtml( line ) );
        scrollToBottom();
    }
}


void WX_HTML_REPORT_PANEL::SetLazyUpdate( bool aLazyUpdate )
{
    m_lazyUpdate = aLazyUpdate;
}


void WX_HTML_REPORT_PANEL::Flush( bool aSort )
{
    wxString html;

    if( aSort )
    {
        std::sort( m_report.begin(), m_report.end(),
                []( const REPORT_LINE& a, const REPORT_LINE& b)
                {
                    return a.severity < b.severity;
                });
    }

    for( const auto& line : m_reportHead )
        html += generateHtml( line );

    for( const auto& line : m_report )
        html += generateHtml( line );

    for( const auto& line : m_reportTail )
        html += generateHtml( line );

    m_htmlView->SetPage( addHeader( html ) );
    scrollToBottom();
}


void WX_HTML_REPORT_PANEL::scrollToBottom()
{
    int x, y, xUnit, yUnit;

    m_htmlView->GetVirtualSize( &x, &y );
    m_htmlView->GetScrollPixelsPerUnit( &xUnit, &yUnit );
    m_htmlView->Scroll( 0, y / yUnit );

    updateBadges();
}


void WX_HTML_REPORT_PANEL::updateBadges()
{
    int count = Count( SEVERITY_ERROR );
    m_errorsBadge->SetBitmap( MakeBadge( SEVERITY_ERROR, count, m_errorsBadge, 2 ) );

    count = Count( SEVERITY_WARNING );
    m_warningsBadge->SetBitmap( MakeBadge( SEVERITY_WARNING, count, m_warningsBadge, 2 ) );
}


wxString WX_HTML_REPORT_PANEL::addHeader( const wxString& aBody )
{
    wxColour bgcolor = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW );
    wxColour fgcolor = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT );

    return wxString::Format( wxT( "<html><body bgcolor='%s' text='%s'>%s</body></html>" ),
                             bgcolor.GetAsString( wxC2S_HTML_SYNTAX ),
                             fgcolor.GetAsString( wxC2S_HTML_SYNTAX ),
                             aBody );
}


int WX_HTML_REPORT_PANEL::Count( int severityMask )
{
    int count = 0;

    for( const REPORT_LINE& reportLine : m_report )
        if( severityMask & reportLine.severity )
            count++;

    return count;
}


wxString WX_HTML_REPORT_PANEL::generateHtml( const REPORT_LINE& aLine )
{
    wxString retv;

    if( !( m_severities & aLine.severity ) )
        return retv;

    switch( aLine.severity )
    {
    case SEVERITY_ERROR:
        retv = "<font color=\"red\" size=2><b>" + _( "Error: " ) + "</b></font><font size=2>" +
               aLine.message + "</font><br>";
        break;
    case SEVERITY_WARNING:
        retv = "<font color=\"orange\" size=2><b>" + _( "Warning: " ) +
               "</b></font><font size=2>" + aLine.message + "</font><br>";
        break;
    case SEVERITY_INFO:
        retv = "<font color=\"dark gray\" size=2><b>" + _( "Info: " ) + "</b>" + aLine.message +
               "</font><br>";
        break;
    case SEVERITY_ACTION:
        retv = "<font color=\"dark green\" size=2>" + aLine.message + "</font><br>";
        break;
    default:
        retv = "<font size=2>" + aLine.message + "</font><br>";
    }

    return retv;
}


wxString WX_HTML_REPORT_PANEL::generatePlainText( const REPORT_LINE& aLine )
{
    switch( aLine.severity )
    {
    case SEVERITY_ERROR:
        return _( "Error: " ) + aLine.message + wxT( "\n" );
    case SEVERITY_WARNING:
        return _( "Warning: " ) + aLine.message + wxT( "\n" );
    case SEVERITY_INFO:
        return _( "Info: " ) + aLine.message + wxT( "\n" );
    default:
        return aLine.message + wxT( "\n" );
    }
}


void WX_HTML_REPORT_PANEL::onRightClick( wxMouseEvent& event )
{
    wxMenu popup;
    popup.Append( wxID_COPY, "Copy" );
    PopupMenu( &popup );
}


void WX_HTML_REPORT_PANEL::onMenuEvent( wxMenuEvent& event )
{
    if( event.GetId() == wxID_COPY )
    {
        if( wxTheClipboard->Open() )
        {
            bool primarySelection = wxTheClipboard->IsUsingPrimarySelection();
            wxTheClipboard->UsePrimarySelection( false );   // required to use the main clipboard
            wxTheClipboard->SetData( new wxTextDataObject( m_htmlView->SelectionToText() ) );
            wxTheClipboard->Close();
            wxTheClipboard->UsePrimarySelection( primarySelection );
        }
    }
}


// Don't globally define this; different facilities use different definitions of "ALL"
static int SEVERITY_ALL = SEVERITY_WARNING | SEVERITY_ERROR | SEVERITY_INFO | SEVERITY_ACTION;


void WX_HTML_REPORT_PANEL::onCheckBoxShowAll( wxCommandEvent& event )
{
    if( event.IsChecked() )
        m_severities = SEVERITY_ALL;
    else
        m_severities = SEVERITY_ERROR;

    syncCheckboxes();
    Flush( true );
}


void WX_HTML_REPORT_PANEL::syncCheckboxes()
{
    m_checkBoxShowAll->SetValue( m_severities == SEVERITY_ALL );
    m_checkBoxShowWarnings->SetValue( m_severities & SEVERITY_WARNING );
    m_checkBoxShowErrors->SetValue( m_severities & SEVERITY_ERROR );
    m_checkBoxShowInfos->SetValue( m_severities & SEVERITY_INFO );
    m_checkBoxShowActions->SetValue( m_severities & SEVERITY_ACTION );
}


void WX_HTML_REPORT_PANEL::onCheckBoxShowWarnings( wxCommandEvent& event )
{
    if( event.IsChecked() )
        m_severities |= SEVERITY_WARNING;
    else
        m_severities &= ~SEVERITY_WARNING;

    syncCheckboxes();
    Flush( true );
}


void WX_HTML_REPORT_PANEL::onCheckBoxShowErrors( wxCommandEvent& event )
{
    if( event.IsChecked() )
        m_severities |= SEVERITY_ERROR;
    else
        m_severities &= ~SEVERITY_ERROR;

    syncCheckboxes();
    Flush( true );
}


void WX_HTML_REPORT_PANEL::onCheckBoxShowInfos( wxCommandEvent& event )
{
    if( event.IsChecked() )
        m_severities |= SEVERITY_INFO;
    else
        m_severities &= ~SEVERITY_INFO;

    syncCheckboxes();
    Flush( true );
}


void WX_HTML_REPORT_PANEL::onCheckBoxShowActions( wxCommandEvent& event )
{
    if( event.IsChecked() )
        m_severities |= SEVERITY_ACTION;
    else
        m_severities &= ~SEVERITY_ACTION;

    syncCheckboxes();
    Flush( true );
}


void WX_HTML_REPORT_PANEL::onBtnSaveToFile( wxCommandEvent& event )
{
    wxFileName fn( "./report.txt" );

    wxFileDialog dlg( this, _( "Save Report to File" ), fn.GetPath(), fn.GetFullName(),
                      TextFileWildcard(), wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() != wxID_OK )
        return;

    fn = dlg.GetPath();

    if( fn.GetExt().IsEmpty() )
        fn.SetExt( "txt" );

    wxFile f( fn.GetFullPath(), wxFile::write );

    if( !f.IsOpened() )
    {
        wxString msg;

        msg.Printf( _( "Cannot write report to file \"%s\"." ),
                    fn.GetFullPath().GetData() );
        wxMessageBox( msg, _( "File save error" ), wxOK | wxICON_ERROR, this );
        return;
    }

    for( const REPORT_LINE& l : m_report )
    {
        f.Write( generatePlainText( l ) );
    }

    f.Close();
}


void WX_HTML_REPORT_PANEL::Clear()
{
    m_report.clear();
    m_reportHead.clear();
    m_reportTail.clear();
}


void WX_HTML_REPORT_PANEL::SetLabel( const wxString& aLabel )
{
    m_box->GetStaticBox()->SetLabel( aLabel );
}


void WX_HTML_REPORT_PANEL::SetVisibleSeverities( int aSeverities )
{
    if( aSeverities < 0 )
        m_severities = SEVERITY_ALL;
    else
        m_severities = aSeverities;

    syncCheckboxes();
}


int WX_HTML_REPORT_PANEL::GetVisibleSeverities()
{
    return m_severities;
}
