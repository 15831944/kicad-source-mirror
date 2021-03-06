/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2011-2016 Wayne Stambaugh <stambaughw@verizon.net>
 * Copyright (C) 1992-2016 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

/**
 * @file erc.cpp
 * @brief Electrical Rules Check implementation.
 */

#include "connection_graph.h"
#include <erc.h>
#include <fctsys.h>
#include <kicad_string.h>
#include <lib_pin.h>
#include <sch_edit_frame.h>
#include <sch_marker.h>
#include <sch_reference_list.h>
#include <sch_sheet.h>
#include <schematic.h>
#include <ws_draw_item.h>
#include <ws_proxy_view_item.h>
#include <wx/ffile.h>


/* ERC tests :
 *  1 - conflicts between connected pins ( example: 2 connected outputs )
 *  2 - minimal connections requirements ( 1 input *must* be connected to an
 * output, or a passive pin )
 */


/*
 *  Minimal ERC requirements:
 *  All pins *must* be connected (except ELECTRICAL_PINTYPE::PT_NC).
 *  When a pin is not connected in schematic, the user must place a "non
 * connected" symbol to this pin.
 *  This ensures a forgotten connection will be detected.
 */

/* Messages for conflicts :
 *  ELECTRICAL_PINTYPE::PT_INPUT, ELECTRICAL_PINTYPE::PT_OUTPUT, ELECTRICAL_PINTYPE:PT_:BIDI, ELECTRICAL_PINTYPE::PT_TRISTATE, ELECTRICAL_PINTYPE::PT_PASSIVE,
 *  ELECTRICAL_PINTYPE::PT_UNSPECIFIED, ELECTRICAL_PINTYPE::PT_POWER_IN, ELECTRICAL_PINTYPE::PT_POWER_OUT, ELECTRICAL_PINTYPE::PT_OPENCOLLECTOR,
 *  ELECTRICAL_PINTYPE::PT_OPENEMITTER, ELECTRICAL_PINTYPE::PT_NC
 *  These messages are used to show the ERC matrix in ERC dialog
 */

// Messages for matrix rows:
const wxString CommentERC_H[] =
{
    _( "Input Pin" ),
    _( "Output Pin" ),
    _( "Bidirectional Pin" ),
    _( "Tri-State Pin" ),
    _( "Passive Pin" ),
    _( "Unspecified Pin" ),
    _( "Power Input Pin" ),
    _( "Power Output Pin" ),
    _( "Open Collector" ),
    _( "Open Emitter" ),
    _( "No Connection" )
};

// Messages for matrix columns
const wxString CommentERC_V[] =
{
    _( "Input Pin" ),
    _( "Output Pin" ),
    _( "Bidirectional Pin" ),
    _( "Tri-State Pin" ),
    _( "Passive Pin" ),
    _( "Unspecified Pin" ),
    _( "Power Input Pin" ),
    _( "Power Output Pin" ),
    _( "Open Collector" ),
    _( "Open Emitter" ),
    _( "No Connection" )
};


int ERC_TESTER::TestDuplicateSheetNames( bool aCreateMarker )
{
    SCH_SCREEN* screen;
    int         err_count = 0;

    SCH_SCREENS screenList( m_schematic->Root() );

    for( screen = screenList.GetFirst(); screen != nullptr; screen = screenList.GetNext() )
    {
        std::vector<SCH_SHEET*> list;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SHEET_T ) )
            list.push_back( static_cast<SCH_SHEET*>( item ) );

        for( size_t i = 0; i < list.size(); i++ )
        {
            SCH_SHEET* sheet = list[i];

            for( size_t j = i + 1; j < list.size(); j++ )
            {
                SCH_SHEET* test_item = list[j];

                // We have found a second sheet: compare names
                // we are using case insensitive comparison to avoid mistakes between
                // similar names like Mysheet and mysheet
                if( sheet->GetName().CmpNoCase( test_item->GetName() ) == 0 )
                {
                    if( aCreateMarker )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_DUPLICATE_SHEET_NAME );
                        ercItem->SetItems( sheet, test_item );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, sheet->GetPosition() );
                        screen->Append( marker );
                    }

                    err_count++;
                }
            }
        }
    }

    return err_count;
}


void ERC_TESTER::TestTextVars( KIGFX::WS_PROXY_VIEW_ITEM* aWorksheet )
{
    WS_DRAW_ITEM_LIST wsItems;

    auto unresolved = [this]( wxString str )
    {
        str = ExpandEnvVarSubstitutions( str, &m_schematic->Prj() );
        return str.Matches( wxT( "*${*}*" ) );
    };

    if( aWorksheet )
    {
        wsItems.SetMilsToIUfactor( IU_PER_MILS );
        wsItems.SetSheetNumber( 1 );
        wsItems.SetSheetCount( 1 );
        wsItems.SetFileName( "dummyFilename" );
        wsItems.SetSheetName( "dummySheet" );
        wsItems.SetSheetLayer( "dummyLayer" );
        wsItems.SetProject( &m_schematic->Prj() );
        wsItems.BuildWorkSheetGraphicList( aWorksheet->GetPageInfo(), aWorksheet->GetTitleBlock() );
    }

    SCH_SHEET_PATH  savedCurrentSheet = m_schematic->CurrentSheet();
    SCH_SHEET_LIST  sheets = m_schematic->GetSheets();

    for( SCH_SHEET_PATH& sheet : sheets )
    {
        m_schematic->SetCurrentSheet( sheet );
        SCH_SCREEN* screen = sheet.LastScreen();

        for( SCH_ITEM* item : screen->Items().OfType( SCH_LOCATE_ANY_T ) )
        {
            if( item->Type() == SCH_COMPONENT_T )
            {
                SCH_COMPONENT* component = static_cast<SCH_COMPONENT*>( item );

                for( SCH_FIELD& field : component->GetFields() )
                {
                    if( unresolved( field.GetShownText() ) )
                    {
                        wxPoint pos = field.GetPosition() - component->GetPosition();
                        pos = component->GetTransform().TransformCoordinate( pos );
                        pos += component->GetPosition();

                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                        ercItem->SetItems( &field );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, pos );
                        screen->Append( marker );
                    }
                }
            }
            else if( item->Type() == SCH_SHEET_T )
            {
                SCH_SHEET* subSheet = static_cast<SCH_SHEET*>( item );

                for( SCH_FIELD& field : subSheet->GetFields() )
                {
                    if( unresolved( field.GetShownText() ) )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                        ercItem->SetItems( &field );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, field.GetPosition() );
                        screen->Append( marker );
                    }
                }

                for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( item )->GetPins() )
                {
                    if( pin->GetShownText().Matches( wxT( "*${*}*" ) ) )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                        ercItem->SetItems( pin );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, pin->GetPosition() );
                        screen->Append( marker );
                    }
                }
            }
            else if( SCH_TEXT* text = dynamic_cast<SCH_TEXT*>( item ) )
            {
                if( text->GetShownText().Matches( wxT( "*${*}*" ) ) )
                {
                    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                    ercItem->SetItems( text );

                    SCH_MARKER* marker = new SCH_MARKER( ercItem, text->GetPosition() );
                    screen->Append( marker );
                }
            }
        }

        for( WS_DRAW_ITEM_BASE* item = wsItems.GetFirst(); item; item = wsItems.GetNext() )
        {
            if( WS_DRAW_ITEM_TEXT* text = dynamic_cast<WS_DRAW_ITEM_TEXT*>( item ) )
            {
                if( text->GetShownText().Matches( wxT( "*${*}*" ) ) )
                {
                    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                    ercItem->SetErrorMessage( _( "Unresolved text variable in worksheet." ) );

                    SCH_MARKER* marker = new SCH_MARKER( ercItem, text->GetPosition() );
                    screen->Append( marker );
                }
            }
        }
    }

    m_schematic->SetCurrentSheet( savedCurrentSheet );
}


int ERC_TESTER::TestConflictingBusAliases()
{
    wxString    msg;
    int         err_count = 0;

    SCH_SCREENS screens( m_schematic->Root() );
    std::vector< std::shared_ptr<BUS_ALIAS> > aliases;

    for( SCH_SCREEN* screen = screens.GetFirst(); screen != NULL; screen = screens.GetNext() )
    {
        std::unordered_set< std::shared_ptr<BUS_ALIAS> > screen_aliases = screen->GetBusAliases();

        for( const std::shared_ptr<BUS_ALIAS>& alias : screen_aliases )
        {
            for( const std::shared_ptr<BUS_ALIAS>& test : aliases )
            {
                if( alias->GetName() == test->GetName() && alias->Members() != test->Members() )
                {
                    msg.Printf( _( "Bus alias %s has conflicting definitions on %s and %s" ),
                                alias->GetName(),
                                alias->GetParent()->GetFileName(),
                                test->GetParent()->GetFileName() );

                    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_BUS_ALIAS_CONFLICT );
                    ercItem->SetErrorMessage( msg );

                    SCH_MARKER* marker = new SCH_MARKER( ercItem, wxPoint() );
                    test->GetParent()->Append( marker );

                    ++err_count;
                }
            }
        }

        aliases.insert( aliases.end(), screen_aliases.begin(), screen_aliases.end() );
    }

    return err_count;
}


int ERC_TESTER::TestMultiunitFootprints()
{
    SCH_SHEET_LIST sheets = m_schematic->GetSheets();

    int errors = 0;
    std::map<wxString, LIB_ID> footprints;
    SCH_MULTI_UNIT_REFERENCE_MAP refMap;
    sheets.GetMultiUnitComponents( refMap, true );

    for( std::pair<const wxString, SCH_REFERENCE_LIST>& component : refMap )
    {
        SCH_REFERENCE_LIST& refList = component.second;

        if( refList.GetCount() == 0 )
        {
            wxFAIL;   // it should not happen
            continue;
        }

        // Reference footprint
        SCH_COMPONENT* unit = nullptr;
        wxString       unitName;
        wxString       unitFP;

        for( unsigned i = 0; i < refList.GetCount(); ++i )
        {
            SCH_SHEET_PATH sheetPath = refList.GetItem( i ).GetSheetPath();
            unitFP = refList.GetItem( i ).GetFootprint();

            if( !unitFP.IsEmpty() )
            {
                unit = refList.GetItem( i ).GetComp();
                unitName = unit->GetRef( &sheetPath, true );
                break;
            }
        }

        for( unsigned i = 0; i < refList.GetCount(); ++i )
        {
            SCH_REFERENCE& secondRef = refList.GetItem( i );
            SCH_COMPONENT* secondUnit = secondRef.GetComp();
            wxString       secondName = secondUnit->GetRef( &secondRef.GetSheetPath(), true );
            const wxString secondFp = secondRef.GetFootprint();
            wxString       msg;

            if( unit && !secondFp.IsEmpty() && unitFP != secondFp )
            {
                msg.Printf( _( "Different footprints assigned to %s and %s" ),
                            unitName, secondName );

                std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_DIFFERENT_UNIT_FP );
                ercItem->SetErrorMessage( msg );
                ercItem->SetItems( unit, secondUnit );

                SCH_MARKER* marker = new SCH_MARKER( ercItem, secondUnit->GetPosition() );
                secondRef.GetSheetPath().LastScreen()->Append( marker );

                ++errors;
            }
        }
    }

    return errors;
}


int ERC_TESTER::TestNoConnectPins()
{
    int err_count = 0;

    for( const SCH_SHEET_PATH& sheet : m_schematic->GetSheets() )
    {
        std::map<wxPoint, std::vector<SCH_PIN*>> pinMap;

        for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_COMPONENT_T ) )
        {
            SCH_COMPONENT* comp = static_cast<SCH_COMPONENT*>( item );

            for( SCH_PIN* pin : comp->GetPins( &sheet ) )
            {
                if( pin->GetLibPin()->GetType() == ELECTRICAL_PINTYPE::PT_NC )
                    pinMap[pin->GetPosition()].emplace_back( pin );
            }
        }

        for( auto& pair : pinMap )
        {
            if( pair.second.size() > 1 )
            {
                err_count++;

                std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_NOCONNECT_CONNECTED );

                ercItem->SetItems( pair.second[0], pair.second[1],
                                   pair.second.size() > 2 ? pair.second[2] : nullptr,
                                   pair.second.size() > 3 ? pair.second[3] : nullptr );
                ercItem->SetErrorMessage( _( "Pins with \"no connection\" type are connected" ) );

                SCH_MARKER* marker = new SCH_MARKER( ercItem, pair.first );
                sheet.LastScreen()->Append( marker );
            }
        }
    }

    return err_count;
}


int ERC_TESTER::TestPinToPin()
{
    ERC_SETTINGS&  settings = m_schematic->ErcSettings();
    const NET_MAP& nets     = m_schematic->ConnectionGraph()->GetNetMap();

    int errors = 0;

    for( const std::pair<NET_NAME_CODE, std::vector<CONNECTION_SUBGRAPH*>> net : nets )
    {
        std::vector<SCH_PIN*> pins;
        std::unordered_map<EDA_ITEM*, SCH_SCREEN*> pinToScreenMap;

        for( CONNECTION_SUBGRAPH* subgraph: net.second )
        {
            for( EDA_ITEM* item : subgraph->m_items )
            {
                if( item->Type() == SCH_PIN_T )
                {
                    pins.emplace_back( static_cast<SCH_PIN*>( item ) );
                    pinToScreenMap[item] = subgraph->m_sheet.LastScreen();
                }
            }
        }

        // Single-pin nets are handled elsewhere
        if( pins.size() < 2 )
            continue;

        std::set<std::pair<SCH_PIN*, SCH_PIN*>> tested;

        for( SCH_PIN* refPin : pins )
        {
            ELECTRICAL_PINTYPE refType = refPin->GetType();

            for( SCH_PIN* testPin : pins )
            {
                if( testPin == refPin )
                    continue;

                std::pair<SCH_PIN*, SCH_PIN*> pair1 = std::make_pair( refPin, testPin );
                std::pair<SCH_PIN*, SCH_PIN*> pair2 = std::make_pair( testPin, refPin );

                if( tested.count( pair1 ) || tested.count( pair2 ) )
                    continue;

                tested.insert( pair1 );
                tested.insert( pair2 );

                ELECTRICAL_PINTYPE testType = testPin->GetType();

                PIN_ERROR erc = settings.GetPinMapValue( refType, testType );

                if( erc != PIN_ERROR::OK )
                {
                    std::shared_ptr<ERC_ITEM> ercItem =
                            ERC_ITEM::Create( erc == PIN_ERROR::WARNING ? ERCE_PIN_TO_PIN_WARNING :
                                                                          ERCE_PIN_TO_PIN_ERROR );
                    ercItem->SetItems( refPin, testPin );

                    ercItem->SetErrorMessage(
                            wxString::Format( _( "Pins of type %s and %s are connected" ),
                                    ElectricalPinTypeGetText( refType ),
                                    ElectricalPinTypeGetText( testType ) ) );

                    SCH_MARKER* marker =
                            new SCH_MARKER( ercItem, refPin->GetTransformedPosition() );
                    pinToScreenMap[refPin]->Append( marker );
                    errors++;
                }
            }
        }
    }

    return errors;
}


int ERC_TESTER::TestMultUnitPinConflicts()
{
    const NET_MAP& nets = m_schematic->ConnectionGraph()->GetNetMap();

    int errors = 0;

    std::unordered_map<wxString, std::pair<wxString, SCH_PIN*>> pinToNetMap;

    for( const std::pair<NET_NAME_CODE, std::vector<CONNECTION_SUBGRAPH*>> net : nets )
    {
        const wxString& netName = net.first.first;
        std::vector<SCH_PIN*> pins;

        for( CONNECTION_SUBGRAPH* subgraph : net.second )
        {
            for( EDA_ITEM* item : subgraph->m_items )
            {
                if( item->Type() == SCH_PIN_T )
                {
                    SCH_PIN* pin = static_cast<SCH_PIN*>( item );

                    if( !pin->GetLibPin()->GetParent()->IsMulti() )
                        continue;

                    wxString name = ( pin->GetParentComponent()->GetRef( &subgraph->m_sheet ) +
                                      ":" + pin->GetNumber() );

                    if( !pinToNetMap.count( name ) )
                    {
                        pinToNetMap[name] = std::make_pair( netName, pin );
                    }
                    else if( pinToNetMap[name].first != netName )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem =
                                ERC_ITEM::Create( ERCE_DIFFERENT_UNIT_NET );

                        ercItem->SetErrorMessage( wxString::Format(
                                _( "Pin %s is connected to both %s and %s" ),
                                pin->GetNumber(), netName, pinToNetMap[name].first ) );

                        ercItem->SetItems( pin, pinToNetMap[name].second );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem,
                                                             pin->GetTransformedPosition() );
                        subgraph->m_sheet.LastScreen()->Append( marker );
                    }
                }
            }
        }
    }

    return errors;
}


int ERC_TESTER::TestSimilarLabels()
{
    const NET_MAP& nets = m_schematic->ConnectionGraph()->GetNetMap();

    int errors = 0;

    std::unordered_map<wxString, SCH_TEXT*> labelMap;

    for( const std::pair<NET_NAME_CODE, std::vector<CONNECTION_SUBGRAPH*>> net : nets )
    {
        std::vector<SCH_PIN*> pins;

        for( CONNECTION_SUBGRAPH* subgraph : net.second )
        {
            for( EDA_ITEM* item : subgraph->m_items )
            {
                switch( item->Type() )
                {
                case SCH_LABEL_T:
                case SCH_HIER_LABEL_T:
                case SCH_GLOBAL_LABEL_T:
                {
                    SCH_TEXT* text = static_cast<SCH_TEXT*>( item );

                    wxString normalized = text->GetShownText().Lower();

                    if( !labelMap.count( normalized ) )
                    {
                        labelMap[normalized] = text;
                    }
                    else if( labelMap.at( normalized )->GetShownText() != text->GetShownText() )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_SIMILAR_LABELS );
                        ercItem->SetItems( text, labelMap.at( normalized ) );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, text->GetPosition() );
                        subgraph->m_sheet.LastScreen()->Append( marker );
                    }

                    break;
                }

                default:
                    break;
                }
            }
        }
    }

    return errors;
}
