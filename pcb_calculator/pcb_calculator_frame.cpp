/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 1992-2015 jean-pierre.charras
 * Copyright (C) 1992-2015 Kicad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <wx/wx.h>

#include <pgm_base.h>
#include <pcb_calculator.h>
#include <pcb_calculator_settings.h>
#include <UnitSelector.h>
#include <bitmaps.h>
#include <geometry/shape_poly_set.h>


// extension of pcb_calculator data filename:
const wxString DataFileNameExt( wxT("pcbcalc") );

PCB_CALCULATOR_FRAME::PCB_CALCULATOR_FRAME( KIWAY* aKiway, wxWindow* aParent ) :
    PCB_CALCULATOR_FRAME_BASE( aParent )
{
    SetKiway( this, aKiway );
    m_currTransLine     = NULL;
    m_currTransLineType = DEFAULT_TYPE;
    m_currAttenuator    = NULL;
    m_RegulatorListChanged = false;
    m_TWMode = TW_MASTER_CURRENT;
    m_TWNested = false;

    SHAPE_POLY_SET dummy;   // A ugly trick to force the linker to include
                            // some methods in code and avoid link errors

    // Populate transline list ordered like in dialog menu list
    const static TRANSLINE_TYPE_ID tltype_list[8] =
    {
        MICROSTRIP_TYPE,    CPW_TYPE,  GROUNDED_CPW_TYPE,
        RECTWAVEGUIDE_TYPE, COAX_TYPE, C_MICROSTRIP_TYPE,
        STRIPLINE_TYPE,     TWISTEDPAIR_TYPE
    };

    for( int ii = 0; ii < 8; ii++ )
        m_transline_list.push_back( new TRANSLINE_IDENT( tltype_list[ii] ) );

    // Populate attenuator list ordered like in dialog menu list
    m_attenuator_list.push_back( new ATTENUATOR_PI() );
    m_attenuator_list.push_back( new ATTENUATOR_TEE() );
    m_attenuator_list.push_back( new ATTENUATOR_BRIDGE() );
    m_attenuator_list.push_back( new ATTENUATOR_SPLITTER() );
    m_currAttenuator = m_attenuator_list[0];

    LoadSettings( config() );

    ReadDataFile();

    TranslineTypeSelection( m_currTransLineType );
    m_TranslineSelection->SetSelection( m_currTransLineType );

    TW_Init();
    VS_Init();

    SetAttenuator( m_AttenuatorsSelection->GetSelection() );

    ToleranceSelection( m_rbToleranceSelection->GetSelection() );

    BoardClassesUpdateData( m_BoardClassesUnitsSelector->GetUnitScale() );

    ElectricalSpacingUpdateData( m_ElectricalSpacingUnitsSelector->GetUnitScale() );

    m_choiceRegulatorSelector->Append( m_RegulatorList.GetRegList() );
    SelectLastSelectedRegulator();

    // Give an icon
    wxIcon icon;
    icon.CopyFromBitmap( KiBitmap( icon_pcbcalculator_xpm ) );
    SetIcon( icon );

    GetSizer()->SetSizeHints( this );

    // Set previous size and position
    SetSize( m_FramePos.x, m_FramePos.y, m_FrameSize.x, m_FrameSize.y );

    if( m_FramePos == wxDefaultPosition )
        Centre();
}


PCB_CALCULATOR_FRAME::~PCB_CALCULATOR_FRAME()
{
    for( unsigned ii = 0; ii < m_transline_list.size(); ii++ )
        delete m_transline_list[ii];

    for( unsigned ii = 0; ii < m_attenuator_list.size(); ii++ )
        delete m_attenuator_list[ii];

    /* This needed for OSX: avoids furter OnDraw processing after this
     * destructor and before the native window is destroyed
     */
    this->Freeze();
}


void PCB_CALCULATOR_FRAME::OnClosePcbCalc( wxCloseEvent& event )
{
    if( m_RegulatorListChanged )
    {
        if( GetDataFilename().IsEmpty() )
        {
            int opt = wxMessageBox(
                _("Data modified, and no data filename to save modifications\n"\
                  "Do you want to exit and abandon your change?"),
                _("Regulator list change"),
                wxYES_NO | wxICON_QUESTION );

            if( opt == wxNO )
                return;
        }
        else
        {
            if( !WriteDataFile() )
            {
                wxString msg;
                msg.Printf( _("Unable to write file \"%s\"\n"\
                            "Do you want to exit and abandon your change?"),
                            GetDataFilename().c_str() );

                int opt = wxMessageBox( msg, _("Write Data File Error"),
                                        wxYES_NO | wxICON_ERROR );
                if( opt  == wxNO )
                    return;
            }
        }
    }

    event.Skip();
//    Destroy();
}

void PCB_CALCULATOR_FRAME::LoadSettings( APP_SETTINGS_BASE* aCfg )
{
    if( aCfg == NULL )
        return;

    EDA_BASE_FRAME::LoadSettings( aCfg );

    auto cfg = static_cast<PCB_CALCULATOR_SETTINGS*>( aCfg );

    m_currTransLineType = static_cast<TRANSLINE_TYPE_ID>( cfg->m_TransLine.type );
    m_Notebook->ChangeSelection( cfg->m_LastPage );
    m_rbToleranceSelection->SetSelection( cfg->m_ColorCodeTolerance );
    m_AttenuatorsSelection->SetSelection( cfg->m_Attenuators.type );
    m_BoardClassesUnitsSelector->SetSelection( cfg->m_BoardClassUnits );

    // Regul panel config:
    m_RegulR1Value->SetValue( cfg->m_Regulators.r1 );
    m_RegulR2Value->SetValue( cfg->m_Regulators.r2 );
    m_RegulVrefValue->SetValue( cfg->m_Regulators.vref );
    m_RegulVoutValue->SetValue( cfg->m_Regulators.vout );
    SetDataFilename( cfg->m_Regulators.data_file );
    m_lastSelectedRegulatorName = cfg->m_Regulators.selected_regulator;
    m_choiceRegType->SetSelection( cfg->m_Regulators.type );

    wxRadioButton* regprms[3] = { m_rbRegulR1, m_rbRegulR2, m_rbRegulVout };

    if( cfg->m_Regulators.last_param >= 3 )
        cfg->m_Regulators.last_param = 0;

    for( int ii = 0; ii < 3; ii++ )
        regprms[ii]->SetValue( cfg->m_Regulators.last_param == ii );

    // Electrical panel config
    m_ElectricalSpacingUnitsSelector->SetSelection( cfg->m_Electrical.spacing_units );
    m_ElectricalSpacingVoltage->SetValue( cfg->m_Electrical.spacing_voltage );

    for( auto& transline : m_transline_list )
        transline->ReadConfig();

    for( auto& attenuator : m_attenuator_list )
        attenuator->ReadConfig();
}


void PCB_CALCULATOR_FRAME::SaveSettings( APP_SETTINGS_BASE* aCfg )
{
    if( aCfg == NULL )
        return;

    EDA_BASE_FRAME::SaveSettings( aCfg );

#if 0
    aCfg->Write( KEYWORD_TRANSLINE_SELECTION, (long) m_currTransLineType );
    aCfg->Write( KEYWORD_PAGE_SELECTION, m_Notebook->GetSelection() );
    aCfg->Write( KEYWORD_COLORCODE_SELECTION, m_rbToleranceSelection->GetSelection() );
    aCfg->Write( KEYWORD_ATTENUATORS_SELECTION, m_AttenuatorsSelection->GetSelection() );
    aCfg->Write( KEYWORD_BRDCLASS_SELECTION, m_BoardClassesUnitsSelector->GetSelection() );

    aCfg->Write( KEYWORD_REGUL_R1, m_RegulR1Value->GetValue() );
    aCfg->Write( KEYWORD_REGUL_R2, m_RegulR2Value->GetValue() );
    aCfg->Write( KEYWORD_REGUL_VREF, m_RegulVrefValue->GetValue() );
    aCfg->Write( KEYWORD_REGUL_VOUT, m_RegulVoutValue->GetValue() );
    aCfg->Write( KEYWORD_DATAFILE_FILENAME, GetDataFilename() );
    aCfg->Write( KEYWORD_REGUL_SELECTED, m_lastSelectedRegulatorName );
    aCfg->Write( KEYWORD_REGUL_TYPE,
                     m_choiceRegType->GetSelection() );
    wxRadioButton * regprms[3] =
    {   m_rbRegulR1, m_rbRegulR2, m_rbRegulVout
    };
    for( int ii = 0; ii < 3; ii++ )
    {
        if( regprms[ii]->GetValue() )
        {
            aCfg->Write( KEYWORD_REGUL_LAST_PARAM, ii );
            break;
        }
    }


    aCfg->Write( KEYWORD_ELECTRICAL_SPACING_SELECTION,
                     m_ElectricalSpacingUnitsSelector->GetSelection() );
    aCfg->Write( KEYWORD_ELECTRICAL_SPACING_VOLTAGE,
                     m_ElectricalSpacingVoltage->GetValue() );
#endif
    TW_WriteConfig();

    VS_WriteConfig();

    for( unsigned ii = 0; ii < m_transline_list.size(); ii++ )
        m_transline_list[ii]->WriteConfig();

    for( unsigned ii = 0; ii < m_attenuator_list.size(); ii++ )
        m_attenuator_list[ii]->WriteConfig();
}


/**
 * Function OnTranslineAnalyse
 * Run a new analyse for the current transline with current parameters
 * and displays the electrical parameters
 */
void PCB_CALCULATOR_FRAME::OnTranslineAnalyse( wxCommandEvent& event )
{
    if( m_currTransLine )
    {
        TransfDlgDataToTranslineParams();
        m_currTransLine->analyze();
    }
}


/**
 * Function OnTranslineSynthetize
 * Run a new synthezis for the current transline with current parameters
 * and displays the geometrical parameters
 */
void PCB_CALCULATOR_FRAME::OnTranslineSynthetize( wxCommandEvent& event )
{
    if( m_currTransLine )
    {
        TransfDlgDataToTranslineParams();
        m_currTransLine->synthesize();
    }
}


void PCB_CALCULATOR_FRAME::OnPaintTranslinePanel( wxPaintEvent& event )
{
    wxPaintDC           dc( m_panelDisplayshape );

    TRANSLINE_IDENT*    tr_ident = m_transline_list[m_currTransLineType];

    if( tr_ident )
    {
        wxSize size = m_panelDisplayshape->GetSize();
        size.x  -= tr_ident->m_Icon->GetWidth();
        size.y  -= tr_ident->m_Icon->GetHeight();
        dc.DrawBitmap( *tr_ident->m_Icon, size.x / 2, size.y / 2 );
    }

    event.Skip();
}

/* returns the full filename of the selected pcb_calculator data file
 * the extension file is forced
 */
const wxString PCB_CALCULATOR_FRAME::GetDataFilename()
{
    if( m_regulators_fileNameCtrl->GetValue().IsEmpty() )
        return wxEmptyString;

    wxFileName fn( m_regulators_fileNameCtrl->GetValue() );
    fn.SetExt( DataFileNameExt );
    return fn.GetFullPath();
}

/* Initialize the full filename of the selected pcb_calculator data file
 * force the standard extension of the file (.pcbcalc)
 * aFilename = the full filename, with or without extension
 */
void PCB_CALCULATOR_FRAME::SetDataFilename( const wxString & aFilename)
{
    if( aFilename.IsEmpty() )
        m_regulators_fileNameCtrl->SetValue( wxEmptyString );

    else
    {
        wxFileName fn( aFilename );
        fn.SetExt( DataFileNameExt );
        m_regulators_fileNameCtrl->SetValue( fn.GetFullPath() );
    }
}
