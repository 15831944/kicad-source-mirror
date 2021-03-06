/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014-2017 CERN
 * Copyright (C) 2018-2020 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Maciej Suminski <maciej.suminski@cern.ch>
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

#include "drawing_tool.h"
#include "pcb_actions.h"
#include <pcb_edit_frame.h>
#include <confirm.h>
#include <import_gfx/dialog_import_gfx.h>
#include <view/view_controls.h>
#include <view/view.h>
#include <tool/tool_manager.h>
#include <geometry/geometry_utils.h>
#include <board_commit.h>
#include <scoped_set_reset.h>
#include <bitmaps.h>
#include <painter.h>
#include <status_popup.h>
#include <dialogs/dialog_text_properties.h>
#include <preview_items/arc_assistant.h>

#include <class_board.h>
#include <class_edge_mod.h>
#include <class_pcb_text.h>
#include <class_dimension.h>
#include <class_zone.h>
#include <class_module.h>

#include <preview_items/two_point_assistant.h>
#include <preview_items/two_point_geom_manager.h>
#include <ratsnest/ratsnest_data.h>
#include <tools/grid_helper.h>
#include <tools/point_editor.h>
#include <tools/selection_tool.h>
#include <tools/tool_event_utils.h>
#include <tools/zone_create_helper.h>
#include <pcbnew_id.h>
#include <dialogs/dialog_track_via_size.h>

using SCOPED_DRAW_MODE = SCOPED_SET_RESET<DRAWING_TOOL::MODE>;


class VIA_SIZE_MENU : public ACTION_MENU
{
public:
    VIA_SIZE_MENU() :
        ACTION_MENU( true )
    {
        SetIcon( width_track_via_xpm );
        SetTitle( _( "Select Via Size" ) );
    }

protected:
    ACTION_MENU* create() const override
    {
        return new VIA_SIZE_MENU();
    }

    void update() override
    {
        PCB_EDIT_FRAME*        frame = (PCB_EDIT_FRAME*) getToolManager()->GetToolHolder();
        EDA_UNITS              units = frame->GetUserUnits();
        BOARD_DESIGN_SETTINGS& bds = frame->GetBoard()->GetDesignSettings();
        bool                   useIndex = !bds.m_UseConnectedTrackWidth
                                                && !bds.UseCustomTrackViaSize();
        wxString               msg;

        Clear();

        Append( ID_POPUP_PCB_SELECT_CUSTOM_WIDTH, _( "Use Custom Values..." ),
                _( "Specify custom track and via sizes" ), wxITEM_CHECK );
        Check( ID_POPUP_PCB_SELECT_CUSTOM_WIDTH, bds.UseCustomTrackViaSize() );

        AppendSeparator();

        for( unsigned i = 1; i < bds.m_ViasDimensionsList.size(); i++ )
        {
            VIA_DIMENSION via = bds.m_ViasDimensionsList[i];

            if( via.m_Drill > 0 )
                msg.Printf( _("Via %s, drill %s" ),
                            MessageTextFromValue( units, via.m_Diameter, true ),
                            MessageTextFromValue( units, via.m_Drill, true ) );
            else
                msg.Printf( _( "Via %s" ), MessageTextFromValue( units, via.m_Diameter, true ) );

            int menuIdx = ID_POPUP_PCB_SELECT_VIASIZE1 + i;
            Append( menuIdx, msg, wxEmptyString, wxITEM_CHECK );
            Check( menuIdx, useIndex && bds.GetViaSizeIndex() == i );
        }
    }

    OPT_TOOL_EVENT eventHandler( const wxMenuEvent& aEvent ) override
    {
        PCB_EDIT_FRAME*        frame = (PCB_EDIT_FRAME*) getToolManager()->GetToolHolder();
        BOARD_DESIGN_SETTINGS& bds = frame->GetBoard()->GetDesignSettings();
        int                    id = aEvent.GetId();

        // On Windows, this handler can be called with an event ID not existing in any
        // menuitem, so only set flags when we have an ID match.

        if( id == ID_POPUP_PCB_SELECT_CUSTOM_WIDTH )
        {
            DIALOG_TRACK_VIA_SIZE sizeDlg( frame, bds );

            if( sizeDlg.ShowModal() )
            {
                bds.UseCustomTrackViaSize( true );
                bds.m_UseConnectedTrackWidth = false;
            }
        }
        else if( id >= ID_POPUP_PCB_SELECT_VIASIZE1 && id <= ID_POPUP_PCB_SELECT_VIASIZE16 )
        {
            bds.UseCustomTrackViaSize( false );
            bds.m_UseConnectedTrackWidth = false;
            bds.SetViaSizeIndex( id - ID_POPUP_PCB_SELECT_VIASIZE1 );
        }

        return OPT_TOOL_EVENT( PCB_ACTIONS::trackViaSizeChanged.MakeEvent() );
    }
};


DRAWING_TOOL::DRAWING_TOOL() :
    PCB_TOOL_BASE( "pcbnew.InteractiveDrawing" ),
    m_view( nullptr ), m_controls( nullptr ),
    m_board( nullptr ), m_frame( nullptr ), m_mode( MODE::NONE ),
    m_lineWidth( 1 )
{
}


DRAWING_TOOL::~DRAWING_TOOL()
{
}


bool DRAWING_TOOL::Init()
{
    auto activeToolFunctor = [this]( const SELECTION& aSel )
                             {
                                 return m_mode != MODE::NONE;
                             };

    // some interactive drawing tools can undo the last point
    auto canUndoPoint = [this]( const SELECTION& aSel )
                        {
                            return (   m_mode == MODE::ARC
                                    || m_mode == MODE::ZONE
                                    || m_mode == MODE::KEEPOUT
                                    || m_mode == MODE::GRAPHIC_POLYGON );
                        };

    // functor for tools that can automatically close the outline
    auto canCloseOutline = [this]( const SELECTION& aSel )
                           {
                                return (   m_mode == MODE::ZONE
                                        || m_mode == MODE::KEEPOUT
                                        || m_mode == MODE::GRAPHIC_POLYGON );
                           };

    auto viaToolActive = [this]( const SELECTION& aSel )
                         {
                             return m_mode == MODE::VIA;
                         };

    auto& ctxMenu = m_menu.GetMenu();

    // cancel current tool goes in main context menu at the top if present
    ctxMenu.AddItem( ACTIONS::cancelInteractive, activeToolFunctor, 1 );
    ctxMenu.AddSeparator( 1 );

    // tool-specific actions
    ctxMenu.AddItem( PCB_ACTIONS::closeOutline,    canCloseOutline, 200 );
    ctxMenu.AddItem( PCB_ACTIONS::deleteLastPoint, canUndoPoint, 200 );

    ctxMenu.AddSeparator( 500 );

    std::shared_ptr<VIA_SIZE_MENU> viaSizeMenu = std::make_shared<VIA_SIZE_MENU>();
    viaSizeMenu->SetTool( this );
    m_menu.AddSubMenu( viaSizeMenu );
    ctxMenu.AddMenu( viaSizeMenu.get(),            viaToolActive, 500 );

    ctxMenu.AddSeparator( 500 );

    // Type-specific sub-menus will be added for us by other tools
    // For example, zone fill/unfill is provided by the PCB control tool

    // Finally, add the standard zoom/grid items
    getEditFrame<PCB_BASE_FRAME>()->AddStandardSubMenus( m_menu );

    return true;
}


void DRAWING_TOOL::Reset( RESET_REASON aReason )
{
    // Init variables used by every drawing tool
    m_view = getView();
    m_controls = getViewControls();
    m_board = getModel<BOARD>();
    m_frame = getEditFrame<PCB_BASE_EDIT_FRAME>();
}


DRAWING_TOOL::MODE DRAWING_TOOL::GetDrawingMode() const
{
    return m_mode;
}


int DRAWING_TOOL::DrawLine( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    MODULE*          module = dynamic_cast<MODULE*>( m_frame->GetModel() );
    DRAWSEGMENT*     line = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
    BOARD_COMMIT     commit( m_frame );
    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::LINE );
    OPT<VECTOR2D>    startingPoint = boost::make_optional<VECTOR2D>( false, VECTOR2D( 0, 0 ) );

    line->SetFlags( IS_NEW );

    if( aEvent.HasPosition() )
        startingPoint = getViewControls()->GetCursorPosition( !aEvent.Modifier( MD_ALT ) );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    while( drawSegment( tool, S_SEGMENT, &line, startingPoint ) )
    {
        if( line )
        {
            if( m_editModules )
                static_cast<EDGE_MODULE*>( line )->SetLocalCoord();

            commit.Add( line );
            commit.Push( _( "Draw a line segment" ) );
            startingPoint = VECTOR2D( line->GetEnd() );
        }
        else
        {
            startingPoint = NULLOPT;
        }

        line = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
        line->SetFlags( IS_NEW );
    }

    return 0;
}


int DRAWING_TOOL::DrawRectangle( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    MODULE*          module = dynamic_cast<MODULE*>( m_frame->GetModel() );
    DRAWSEGMENT*     rect = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
    BOARD_COMMIT     commit( m_frame );
    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::RECTANGLE );
    OPT<VECTOR2D>    startingPoint = boost::make_optional<VECTOR2D>( false, VECTOR2D( 0, 0 ) );

    rect->SetFlags(IS_NEW );

    if( aEvent.HasPosition() )
        startingPoint = getViewControls()->GetCursorPosition( !aEvent.Modifier( MD_ALT ) );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    while( drawSegment( tool, S_RECT, &rect, startingPoint ) )
    {
        if( rect )
        {
            if( m_editModules )
                static_cast<EDGE_MODULE*>( rect )->SetLocalCoord();

            commit.Add( rect );
            commit.Push( _( "Draw a rectangle" ) );

            m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, rect );
        }

        rect = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
        rect->SetFlags(IS_NEW );
        startingPoint = NULLOPT;
    }

    return 0;
}


int DRAWING_TOOL::DrawCircle( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    MODULE*          module = dynamic_cast<MODULE*>( m_frame->GetModel() );
    DRAWSEGMENT*     circle = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
    BOARD_COMMIT     commit( m_frame );
    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::CIRCLE );
    OPT<VECTOR2D>    startingPoint = boost::make_optional<VECTOR2D>( false, VECTOR2D( 0, 0 ) );

    circle->SetFlags( IS_NEW );

    if( aEvent.HasPosition() )
        startingPoint = getViewControls()->GetCursorPosition( !aEvent.Modifier( MD_ALT ) );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    while( drawSegment( tool, S_CIRCLE, &circle, startingPoint ) )
    {
        if( circle )
        {
            if( m_editModules )
                static_cast<EDGE_MODULE*>( circle )->SetLocalCoord();

            commit.Add( circle );
            commit.Push( _( "Draw a circle" ) );

            m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, circle );
        }

        circle = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
        circle->SetFlags( IS_NEW );
        startingPoint = NULLOPT;
    }

    return 0;
}


int DRAWING_TOOL::DrawArc( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    MODULE*          module = dynamic_cast<MODULE*>( m_frame->GetModel() );
    DRAWSEGMENT*     arc = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
    BOARD_COMMIT     commit( m_frame );
    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::ARC );
    bool             immediateMode = aEvent.HasPosition();

    arc->SetFlags( IS_NEW );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    while( drawArc( tool, &arc, immediateMode ) )
    {
        if( arc )
        {
            if( m_editModules )
                static_cast<EDGE_MODULE*>( arc )->SetLocalCoord();

            commit.Add( arc );
            commit.Push( _( "Draw an arc" ) );

            m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, arc );
        }

        arc = m_editModules ? new EDGE_MODULE( module ) : new DRAWSEGMENT;
        arc->SetFlags( IS_NEW );
        immediateMode = false;
    }

    return 0;
}


int DRAWING_TOOL::PlaceText( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    BOARD_ITEM* text = NULL;
    const BOARD_DESIGN_SETTINGS& dsnSettings = m_frame->GetDesignSettings();
    BOARD_COMMIT commit( m_frame );

    m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );
    m_controls->ShowCursor( true );
    // do not capture or auto-pan until we start placing some text

    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::TEXT );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    // Prime the pump
    if( aEvent.HasPosition() )
        m_toolMgr->RunAction( ACTIONS::cursorClick );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        m_frame->GetCanvas()->SetCurrentCursor( text ? wxCURSOR_ARROW : wxCURSOR_PENCIL );
        VECTOR2I cursorPos = m_controls->GetCursorPosition();

        auto cleanup = [&]()
                       {
                           m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );
                           m_controls->ForceCursorPosition( false );
                           m_controls->ShowCursor( true );
                           m_controls->SetAutoPan( false );
                           m_controls->CaptureCursor( false );
                           delete text;
                           text = NULL;
                       };

        if( evt->IsCancelInteractive() )
        {
            if( text )
                cleanup();
            else
            {
                m_frame->PopTool( tool );
                break;
            }
        }
        else if( evt->IsActivate() )
        {
            if( text )
                cleanup();

            if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( tool );
                break;
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        else if( evt->IsClick( BUT_LEFT ) )
        {
            bool placing = text != nullptr;

            if( !text )
            {
                m_controls->ForceCursorPosition( true, m_controls->GetCursorPosition() );
                PCB_LAYER_ID layer = m_frame->GetActiveLayer();

                // Init the new item attributes
                if( m_editModules )
                {
                    TEXTE_MODULE* textMod = new TEXTE_MODULE( (MODULE*) m_frame->GetModel() );

                    textMod->SetLayer( layer );
                    textMod->SetTextSize( dsnSettings.GetTextSize( layer ) );
                    textMod->SetTextThickness( dsnSettings.GetTextThickness( layer ) );
                    textMod->SetItalic( dsnSettings.GetTextItalic( layer ) );
                    textMod->SetKeepUpright( dsnSettings.GetTextUpright( layer ) );
                    textMod->SetTextPos( (wxPoint) cursorPos );

                    text = textMod;

                    DIALOG_TEXT_PROPERTIES textDialog( m_frame, textMod );
                    bool cancelled;

                    RunMainStack( [&]()
                                  {
                                      cancelled = !textDialog.ShowModal()
                                                    || textMod->GetText().IsEmpty();
                                  } );

                    if( cancelled )
                    {
                        delete text;
                        text = nullptr;
                    }
                    else if( textMod->GetTextPos() != (wxPoint) cursorPos )
                    {
                        // If the user modified the location then go ahead and place it there.
                        // Otherwise we'll drag.
                        placing = true;
                    }
                }
                else
                {
                    TEXTE_PCB* textPcb = new TEXTE_PCB( m_frame->GetModel() );
                    // TODO we have to set IS_NEW, otherwise InstallTextPCB.. creates an undo entry :| LEGACY_CLEANUP
                    textPcb->SetFlags( IS_NEW );

                    textPcb->SetLayer( layer );

                    // Set the mirrored option for layers on the BACK side of the board
                    if( IsBackLayer( layer ) )
                        textPcb->SetMirrored( true );

                    textPcb->SetTextSize( dsnSettings.GetTextSize( layer ) );
                    textPcb->SetTextThickness( dsnSettings.GetTextThickness( layer ) );
                    textPcb->SetItalic( dsnSettings.GetTextItalic( layer ) );
                    textPcb->SetTextPos( (wxPoint) cursorPos );

                    RunMainStack( [&]()
                                  {
                                      m_frame->InstallTextOptionsFrame( textPcb );
                                  } );

                    if( textPcb->GetText().IsEmpty() )
                        delete textPcb;
                    else
                        text = textPcb;
                }

                if( text )
                {
                    m_controls->WarpCursor( text->GetPosition(), true );
                    m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, text );
                    m_view->Update( &selection() );
                }
            }

            if( placing )
            {
                text->ClearFlags();
                m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );

                commit.Add( text );
                commit.Push( _( "Place a text" ) );

                m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, text );

                text = nullptr;
            }

            m_controls->ForceCursorPosition( false );
            m_controls->ShowCursor( true );
            m_controls->CaptureCursor( text != nullptr );
            m_controls->SetAutoPan( text != nullptr );
        }
        else if( text && evt->IsMotion() )
        {
            text->SetPosition( (wxPoint) cursorPos );
            selection().SetReferencePoint( cursorPos );
            m_view->Update( &selection() );
        }
        else if( evt->IsAction( &PCB_ACTIONS::properties ) )
        {
            if( text )
            {
                frame()->OnEditItemRequest( text );
                m_view->Update( &selection() );
                frame()->SetMsgPanel( text );
            }
        }
        else
            evt->SetPassEvent();
    }

    frame()->SetMsgPanel( board() );
    return 0;
}


void DRAWING_TOOL::constrainDimension( DIMENSION* aDim )
{
    const VECTOR2I lineVector{ aDim->GetEnd() - aDim->GetStart() };

    aDim->SetEnd( wxPoint( VECTOR2I( aDim->GetStart() ) + GetVectorSnapped45( lineVector ) ) );
}


int DRAWING_TOOL::DrawDimension( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    TOOL_EVENT    originalEvent = aEvent;
    POINT_EDITOR* pointEditor   = m_toolMgr->GetTool<POINT_EDITOR>();
    DIMENSION*    dimension     = nullptr;
    BOARD_COMMIT  commit( m_frame );
    GRID_HELPER   grid( m_toolMgr, m_frame->GetMagneticItemsSettings() );

    const BOARD_DESIGN_SETTINGS& boardSettings = m_board->GetDesignSettings();

    // Add a VIEW_GROUP that serves as a preview for the new item
    PCBNEW_SELECTION preview;

    m_view->Add( &preview );

    m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );
    m_controls->ShowCursor( true );

    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::DIMENSION );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    enum DIMENSION_STEPS
    {
        SET_ORIGIN = 0,
        SET_END,
        SET_HEIGHT,
        FINISHED
    };
    int step = SET_ORIGIN;

    // Prime the pump
    m_toolMgr->RunAction( ACTIONS::refreshPreview );

    if( aEvent.HasPosition() )
        m_toolMgr->PrimeTool( aEvent.Position() );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        if( !pointEditor->HasPoint() )
            m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_PENCIL );

        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( m_frame->IsGridVisible() );
        VECTOR2I cursorPos = grid.BestSnapAnchor(
                evt->IsPrime() ? evt->Position() : m_controls->GetMousePosition(), nullptr );
        m_controls->ForceCursorPosition( true, cursorPos );

        auto cleanup = [&]()
                       {
                           m_controls->SetAutoPan( false );
                           m_controls->CaptureCursor( false );

                           preview.Clear();
                           m_view->Update( &preview );

                           delete dimension;
                           dimension = nullptr;
                           step = SET_ORIGIN;
                       };

        if( evt->IsCancelInteractive() )
        {
            m_controls->SetAutoPan( false );

            if( step != SET_ORIGIN )    // start from the beginning
                cleanup();
            else
            {
                m_frame->PopTool( tool );
                break;
            }
        }
        else if( evt->IsActivate() )
        {
            if( step != SET_ORIGIN )
                cleanup();

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( tool );
                break;
            }
        }
        else if( evt->IsAction( &PCB_ACTIONS::incWidth ) && step != SET_ORIGIN )
        {
            m_lineWidth += WIDTH_STEP;
            dimension->SetLineThickness( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( dimension );
        }
        else if( evt->IsAction( &PCB_ACTIONS::decWidth ) && step != SET_ORIGIN )
        {
            if( m_lineWidth > WIDTH_STEP )
            {
                m_lineWidth -= WIDTH_STEP;
                dimension->SetLineThickness( m_lineWidth );
                m_view->Update( &preview );
                frame()->SetMsgPanel( dimension );
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        else if( evt->IsClick( BUT_LEFT ) )
        {
            switch( step )
            {
            case SET_ORIGIN:
            {
                m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );

                PCB_LAYER_ID layer = m_frame->GetActiveLayer();

                if( layer == Edge_Cuts )        // dimensions are not allowed on EdgeCuts
                    layer = Dwgs_User;

                // Init the new item attributes
                if( originalEvent.IsAction( &PCB_ACTIONS::drawAlignedDimension ) )
                {
                    dimension = new ALIGNED_DIMENSION( m_board );

                    dimension->SetUnitsMode( boardSettings.m_DimensionUnitsMode );
                    dimension->SetUnitsFormat( boardSettings.m_DimensionUnitsFormat );
                    dimension->SetPrecision( boardSettings.m_DimensionPrecision );
                    dimension->SetSuppressZeroes( boardSettings.m_DimensionSuppressZeroes );
                    dimension->SetTextPositionMode( boardSettings.m_DimensionTextPosition );
                    dimension->SetKeepTextAligned( boardSettings.m_DimensionKeepTextAligned );

                    if( boardSettings.m_DimensionUnitsMode == DIM_UNITS_MODE::AUTOMATIC )
                        dimension->SetUnits( m_frame->GetUserUnits(), false );
                }
                else if( originalEvent.IsAction( &PCB_ACTIONS::drawCenterDimension ) )
                {
                    dimension = new CENTER_DIMENSION( m_board );
                }
                else if( originalEvent.IsAction( &PCB_ACTIONS::drawLeader ) )
                {
                    dimension = new LEADER( m_board );
                    dimension->Text().SetPosition( wxPoint( cursorPos ) );
                }
                else
                {
                    wxFAIL_MSG( "Unhandled action in DRAWING_TOOL::DrawDimension" );
                }

                dimension->SetLayer( layer );
                dimension->Text().SetTextSize( boardSettings.GetTextSize( layer ) );
                dimension->Text().SetTextThickness( boardSettings.GetTextThickness( layer ) );
                dimension->Text().SetItalic( boardSettings.GetTextItalic( layer ) );
                dimension->SetLineThickness( boardSettings.GetLineThickness( layer ) );
                dimension->SetArrowLength( boardSettings.m_DimensionArrowLength );
                dimension->SetExtensionOffset( boardSettings.m_DimensionExtensionOffset );
                dimension->SetStart( (wxPoint) cursorPos );
                dimension->SetEnd( (wxPoint) cursorPos );

                preview.Add( dimension );

                m_controls->SetAutoPan( true );
                m_controls->CaptureCursor( true );
            }
            break;

            case SET_END:
            {
                dimension->SetEnd( (wxPoint) cursorPos );

                if( !!evt->Modifier( MD_CTRL ) || dimension->Type() == PCB_DIM_CENTER_T )
                    constrainDimension( dimension );

                // Dimensions that have origin and end in the same spot are not valid
                if( dimension->GetStart() == dimension->GetEnd() )
                    --step;
                else if( dimension->Type() == PCB_DIM_LEADER_T )
                    dimension->SetText( wxT( "?" ) );

                if( dimension->Type() == PCB_DIM_CENTER_T )
                {
                    // No separate height/text step
                    ++step;
                    KI_FALLTHROUGH;
                }
                else
                    break;
            }

            case SET_HEIGHT:
            {
                if( dimension->Type() == PCB_DIM_LEADER_T )
                {
                    assert( dimension->GetStart() != dimension->GetEnd() );
                    assert( dimension->GetLineThickness() > 0 );

                    preview.Remove( dimension );

                    commit.Add( dimension );
                    commit.Push( _( "Draw a leader" ) );

                    // Run the edit immediately to set the leader text
                    m_toolMgr->RunAction( PCB_ACTIONS::properties, true, dimension );
                }
                else if( (wxPoint) cursorPos != dimension->GetPosition() )
                {
                    assert( dimension->GetStart() != dimension->GetEnd() );
                    assert( dimension->GetLineThickness() > 0 );

                    preview.Remove( dimension );

                    commit.Add( dimension );
                    commit.Push( _( "Draw a dimension" ) );

                    m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, dimension );
                }

                break;
            }
            }

            if( ++step == FINISHED )
            {
                step = SET_ORIGIN;
                m_controls->SetAutoPan( false );
                m_controls->CaptureCursor( false );
            }
        }
        else if( evt->IsMotion() )
        {
            switch( step )
            {
            case SET_END:
                dimension->SetEnd( (wxPoint) cursorPos );

                if( !!evt->Modifier( MD_CTRL ) || dimension->Type() == PCB_DIM_CENTER_T )
                    constrainDimension( dimension );

                break;

            case SET_HEIGHT:
            {
                if( dimension->Type() == PCB_DIM_ALIGNED_T )
                {
                    ALIGNED_DIMENSION* aligned = static_cast<ALIGNED_DIMENSION*>( dimension );

                    // Calculating the direction of travel perpendicular to the selected axis
                    double angle = aligned->GetAngle() + ( M_PI / 2 );

                    wxPoint delta( (wxPoint) cursorPos - dimension->GetEnd() );
                    double  height = ( delta.x * cos( angle ) ) + ( delta.y * sin( angle ) );
                    aligned->SetHeight( height );
                }
                else if( dimension->Type() != PCB_DIM_CENTER_T )
                {
                    wxASSERT( dimension->Type() == PCB_DIM_LEADER_T );

                    VECTOR2I lineVector( cursorPos - dimension->GetEnd() );
                    dimension->Text().SetPosition( wxPoint( VECTOR2I( dimension->GetEnd() ) +
                                                            GetVectorSnapped45( lineVector ) ) );
                    dimension->Update();
                }
            }
            break;
            }

            // Show a preview of the item
            m_view->Update( &preview );
        }
        else if( evt->IsAction( &PCB_ACTIONS::properties ) )
        {
            if( step != SET_ORIGIN )
            {
                frame()->OnEditItemRequest( dimension );
                dimension->Update();
                frame()->SetMsgPanel( dimension );
            }
        }
        else
            evt->SetPassEvent();
    }

    if( step != SET_ORIGIN )
        delete dimension;

    m_controls->SetAutoPan( false );
    m_controls->ForceCursorPosition( false );
    m_controls->CaptureCursor( false );

    m_view->Remove( &preview );
    frame()->SetMsgPanel( board() );
    return 0;
}


int DRAWING_TOOL::PlaceImportedGraphics( const TOOL_EVENT& aEvent )
{
    if( !m_frame->GetModel() )
        return 0;

    // Note: PlaceImportedGraphics() will convert PCB_LINE_T and PCB_TEXT_T to module graphic
    // items if needed
    DIALOG_IMPORT_GFX dlg( m_frame, m_editModules );
    int dlgResult = dlg.ShowModal();

    std::list<std::unique_ptr<EDA_ITEM>>& list = dlg.GetImportedItems();

    if( dlgResult != wxID_OK )
        return 0;

    // Ensure the list is not empty:
    if( list.empty() )
    {
        wxMessageBox( _( "No graphic items found in file to import") );
        return 0;
    }

    m_toolMgr->RunAction( ACTIONS::cancelInteractive, true );

    // Add a VIEW_GROUP that serves as a preview for the new item
    PCBNEW_SELECTION preview;
    BOARD_COMMIT     commit( m_frame );
    PCB_GROUP*       grp = nullptr;

    if( dlg.ShouldGroupItems() )
        grp = new PCB_GROUP( m_frame->GetBoard() );

    // Build the undo list & add items to the current view
    for( auto& ptr : list)
    {
        EDA_ITEM* item = ptr.get();

        if( m_editModules )
            wxASSERT( item->Type() == PCB_MODULE_EDGE_T || item->Type() == PCB_MODULE_TEXT_T );
        else
            wxASSERT( item->Type() == PCB_LINE_T || item->Type() == PCB_TEXT_T );

        if( grp )
            grp->AddItem( static_cast<BOARD_ITEM*>( item ) );
        else if( dlg.IsPlacementInteractive() )
            preview.Add( item );
        else
            commit.Add( item );

        ptr.release();
    }

    if( !dlg.IsPlacementInteractive() )
    {
        if( grp )
        {
            grp->AddChildrenToCommit( commit );
            commit.Add( grp );
        }

        commit.Push( _( "Place a DXF_SVG drawing" ) );
        return 0;
    }

    if( grp )
        preview.Add( grp );

    std::vector<BOARD_ITEM*> newItems;

    for( EDA_ITEM* item : preview )
        newItems.push_back( static_cast<BOARD_ITEM*>( item ) );

    BOARD_ITEM* firstItem = static_cast<BOARD_ITEM*>( preview.Front() );
    m_view->Add( &preview );

    // Clear the current selection then select the drawings so that edit tools work on them
    m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );
    m_toolMgr->RunAction( PCB_ACTIONS::selectItems, true, &newItems );

    m_controls->ShowCursor( true );
    m_controls->ForceCursorPosition( false );

    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::DXF );

    // Now move the new items to the current cursor position:
    VECTOR2I cursorPos = m_controls->GetCursorPosition();
    VECTOR2I delta = cursorPos - firstItem->GetPosition();

    for( EDA_ITEM* item : preview )
        static_cast<BOARD_ITEM*>( item )->Move( (wxPoint) delta );

    m_view->Update( &preview );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_ARROW );
        cursorPos = m_controls->GetCursorPosition();

        if( evt->IsCancelInteractive() || evt->IsActivate() )
        {
            m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );

            // If a group is being used, we must delete the items themselves,
            // since they are only in the group and not in the preview
            if( grp )
            {
                grp->RunOnChildren( [&]( BOARD_ITEM* bItem )
                                    {
                                        delete bItem ;
                                    } );
            }

            preview.FreeItems();
            break;
        }
        else if( evt->IsMotion() )
        {
            delta = cursorPos - firstItem->GetPosition();

            for( auto item : preview )
                static_cast<BOARD_ITEM*>( item )->Move( (wxPoint) delta );

            m_view->Update( &preview );
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        else if( evt->IsClick( BUT_LEFT ) )
        {
            // Place the imported drawings
            for( EDA_ITEM* item : preview )
            {
                if( item->Type() == PCB_GROUP_T )
                    static_cast<PCB_GROUP*>( item )->AddChildrenToCommit( commit );

                commit.Add( item );
            }

            commit.Push( _( "Place a DXF_SVG drawing" ) );
            break;   // This is a one-shot command, not a tool
        }
        else
            evt->SetPassEvent();
    }

    preview.Clear();
    m_view->Remove( &preview );
    m_frame->PopTool( tool );
    return 0;
}


int DRAWING_TOOL::SetAnchor( const TOOL_EVENT& aEvent )
{
    assert( m_editModules );

    if( !m_frame->GetModel() )
        return 0;

    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::ANCHOR );
    GRID_HELPER      grid( m_toolMgr, m_frame->GetMagneticItemsSettings() );

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();

    m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );
    m_controls->ShowCursor( true );
    m_controls->SetAutoPan( true );
    m_controls->CaptureCursor( false );

    while( TOOL_EVENT* evt = Wait() )
    {
        m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_BULLSEYE );

        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( m_frame->IsGridVisible() );
        VECTOR2I    cursorPos = grid.BestSnapAnchor( m_controls->GetMousePosition(), LSET::AllLayersMask() );
        m_controls->ForceCursorPosition( true, cursorPos );

        if( evt->IsClick( BUT_LEFT ) )
        {
            MODULE* module = (MODULE*) m_frame->GetModel();
            BOARD_COMMIT commit( m_frame );
            commit.Modify( module );

            // set the new relative internal local coordinates of footprint items
            wxPoint     moveVector = module->GetPosition() - (wxPoint) cursorPos;
            module->MoveAnchorPosition( moveVector );

            commit.Push( _( "Move the footprint reference anchor" ) );

            // Usually, we do not need to change twice the anchor position,
            // so deselect the active tool
            m_frame->PopTool( tool );
            break;
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        else if( evt->IsCancelInteractive() || evt->IsActivate() )
        {
            m_frame->PopTool( tool );
            break;
        }
        else
            evt->SetPassEvent();
    }

    return 0;
}


/**
 * Update an DRAWSEGMENT from the current state
 * of an Two POINT Geometry Manager
 */
static void updateSegmentFromConstructionMgr(
        const KIGFX::PREVIEW::TWO_POINT_GEOMETRY_MANAGER& aMgr, DRAWSEGMENT* aGraphic )
{
    auto vec = aMgr.GetOrigin();

    aGraphic->SetStart( { vec.x, vec.y } );

    vec = aMgr.GetEnd();
    aGraphic->SetEnd( { vec.x, vec.y } );
}


bool DRAWING_TOOL::drawSegment( const std::string& aTool, int aShape, DRAWSEGMENT** aGraphic,
                                OPT<VECTOR2D> aStartingPoint )
{
    // Only three shapes are currently supported
    assert( aShape == S_SEGMENT || aShape == S_CIRCLE || aShape == S_RECT );
    GRID_HELPER   grid( m_toolMgr, m_frame->GetMagneticItemsSettings() );
    POINT_EDITOR* pointEditor = m_toolMgr->GetTool<POINT_EDITOR>();
    DRAWSEGMENT*& graphic = *aGraphic;

    m_lineWidth = getSegmentWidth( m_frame->GetActiveLayer() );

    // geometric construction manager
    KIGFX::PREVIEW::TWO_POINT_GEOMETRY_MANAGER twoPointManager;

    // drawing assistant overlay
    // TODO: workaround because STROKE_T is not visible from commons.
    KIGFX::PREVIEW::GEOM_SHAPE geomShape( static_cast<KIGFX::PREVIEW::GEOM_SHAPE>( aShape ) );
    KIGFX::PREVIEW::TWO_POINT_ASSISTANT twoPointAsst(
            twoPointManager, m_frame->GetUserUnits(), geomShape );

    // Add a VIEW_GROUP that serves as a preview for the new item
    PCBNEW_SELECTION preview;
    m_view->Add( &preview );
    m_view->Add( &twoPointAsst );

    m_controls->ShowCursor( true );

    bool     started = false;
    bool     cancelled = false;
    bool     isLocalOriginSet = ( m_frame->GetScreen()->m_LocalOrigin != VECTOR2D( 0, 0 ) );
    VECTOR2I cursorPos = m_controls->GetMousePosition();

    // Prime the pump
    m_toolMgr->RunAction( ACTIONS::refreshPreview );

    if( aStartingPoint )
        m_toolMgr->RunAction( ACTIONS::cursorClick );

    frame()->SetMsgPanel( graphic );
    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        if( !pointEditor->HasPoint() )
            m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_PENCIL );

        m_frame->SetMsgPanel( graphic );

        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( m_frame->IsGridVisible() );
        cursorPos = grid.BestSnapAnchor( m_controls->GetMousePosition(), m_frame->GetActiveLayer() );
        m_controls->ForceCursorPosition( true, cursorPos );

        // 45 degree angle constraint enabled with an option and toggled with Ctrl
        bool limit45 = frame()->Settings().m_Use45DegreeGraphicSegments;

        if( evt->Modifier( MD_CTRL ) )
            limit45 = !limit45;

        auto cleanup = [&] () {
            preview.Clear();
            m_view->Update( &preview );
            delete graphic;
            graphic = nullptr;

            if( !isLocalOriginSet )
                m_frame->GetScreen()->m_LocalOrigin = VECTOR2D( 0, 0 );
        };

        if( evt->IsCancelInteractive() )
        {
            cleanup();

            if( !started )
            {
                m_frame->PopTool( aTool );
                cancelled = true;
            }

            break;
        }
        else if( evt->IsActivate() )
        {
            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                cleanup();
                // leave ourselves on the stack so we come back after the move
                cancelled = true;
                break;
            }
            else
            {
                cleanup();
                m_frame->PopTool( aTool );
                cancelled = true;
                break;
            }
        }
        else if( evt->IsAction( &PCB_ACTIONS::layerChanged ) )
        {
            m_lineWidth = getSegmentWidth( m_frame->GetActiveLayer() );
            graphic->SetLayer( m_frame->GetActiveLayer() );
            graphic->SetWidth( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( graphic );
        }
        else if( evt->IsAction( &PCB_ACTIONS::properties ) )
        {
            if( started )
            {
                frame()->OnEditItemRequest( graphic );
                m_view->Update( &preview );
                frame()->SetMsgPanel( graphic );
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        else if( evt->IsClick( BUT_LEFT ) || evt->IsDblClick( BUT_LEFT ) )
        {
            if( !started )
            {
                m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );

                if( aStartingPoint )
                {
                    cursorPos = aStartingPoint.get();
                    aStartingPoint = NULLOPT;
                }

                m_lineWidth = getSegmentWidth( m_frame->GetActiveLayer() );

                // Init the new item attributes
                graphic->SetShape( (STROKE_T) aShape );
                graphic->SetWidth( m_lineWidth );
                graphic->SetLayer( m_frame->GetActiveLayer() );
                grid.SetSkipPoint( cursorPos );

                twoPointManager.SetOrigin( (wxPoint) cursorPos );
                twoPointManager.SetEnd( (wxPoint) cursorPos );

                if( !isLocalOriginSet )
                    m_frame->GetScreen()->m_LocalOrigin = cursorPos;

                preview.Add( graphic );
                frame()->SetMsgPanel( board() );
                m_controls->SetAutoPan( true );
                m_controls->CaptureCursor( true );

                updateSegmentFromConstructionMgr( twoPointManager, graphic );

                started = true;
            }
            else
            {
                auto snapItem = dyn_cast<DRAWSEGMENT*>( grid.GetSnapped() );

                if( twoPointManager.GetOrigin() == twoPointManager.GetEnd()
                        || ( evt->IsDblClick( BUT_LEFT ) && aShape == S_SEGMENT ) || snapItem )
                // User has clicked twice in the same spot
                //  or clicked on the end of an existing segment (closing a path)
                {
                    BOARD_COMMIT commit( m_frame );

                    // If the user clicks on an existing snap point from a drawsegment
                    //  we finish the segment as they are likely closing a path
                    if( snapItem && (  aShape == S_RECT || graphic->GetLength() > 0.0 ) )
                    {
                        commit.Add( graphic );
                        commit.Push( _( "Draw a line segment" ) );
                        m_toolMgr->RunAction( PCB_ACTIONS::selectItem, true, graphic );
                    }
                    else
                    {
                        delete graphic;
                    }

                    graphic = nullptr;
                }

                preview.Clear();
                break;
            }

            twoPointManager.SetEnd( cursorPos );
        }
        else if( evt->IsMotion() )
        {
            // 45 degree lines
            if( ( limit45 && aShape == S_SEGMENT )
                    || ( evt->Modifier( MD_CTRL ) && aShape == S_RECT ) )
            {
                const VECTOR2I lineVector( cursorPos - VECTOR2I( twoPointManager.GetOrigin() ) );

                // get a restricted 45/H/V line from the last fixed point to the cursor
                auto newEnd = GetVectorSnapped45( lineVector, ( aShape == S_RECT ) );
                m_controls->ForceCursorPosition( true, VECTOR2I( twoPointManager.GetEnd() ) );
                twoPointManager.SetEnd( twoPointManager.GetOrigin() + (wxPoint) newEnd );
                twoPointManager.SetAngleSnap( true );
            }
            else
            {
                twoPointManager.SetEnd( (wxPoint) cursorPos );
                twoPointManager.SetAngleSnap( false );
            }

            updateSegmentFromConstructionMgr( twoPointManager, graphic );
            m_view->Update( &preview );
            m_view->Update( &twoPointAsst );
        }
        else if( evt->IsAction( &PCB_ACTIONS::incWidth ) )
        {
            m_lineWidth += WIDTH_STEP;
            graphic->SetWidth( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( graphic );
        }
        else if( evt->IsAction( &PCB_ACTIONS::decWidth ) && ( m_lineWidth > WIDTH_STEP ) )
        {
            m_lineWidth -= WIDTH_STEP;
            graphic->SetWidth( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( graphic );
        }
        else if( evt->IsAction( &ACTIONS::resetLocalCoords ) )
        {
            isLocalOriginSet = true;
        }
        else
            evt->SetPassEvent();
    }

    if( !isLocalOriginSet ) // reset the relative coordinte if it was not set before
        m_frame->GetScreen()->m_LocalOrigin = VECTOR2D( 0, 0 );

    m_view->Remove( &twoPointAsst );
    m_view->Remove( &preview );
    frame()->SetMsgPanel( board() );
    m_controls->SetAutoPan( false );
    m_controls->CaptureCursor( false );
    m_controls->ForceCursorPosition( false );

    return !cancelled;
}


/**
 * Update an arc DRAWSEGMENT from the current state
 * of an Arc Geometry Manager
 */
static void updateArcFromConstructionMgr( const KIGFX::PREVIEW::ARC_GEOM_MANAGER& aMgr,
                                          DRAWSEGMENT& aArc )
{
    auto vec = aMgr.GetOrigin();

    aArc.SetCenter( { vec.x, vec.y } );

    vec = aMgr.GetStartRadiusEnd();
    aArc.SetArcStart( { vec.x, vec.y } );

    aArc.SetAngle( RAD2DECIDEG( -aMgr.GetSubtended() ) );

    vec = aMgr.GetEndRadiusEnd();
    aArc.SetArcEnd( { vec.x, vec.y } );
}


bool DRAWING_TOOL::drawArc( const std::string& aTool, DRAWSEGMENT** aGraphic, bool aImmediateMode )
{
    DRAWSEGMENT*& graphic = *aGraphic;
    m_lineWidth = getSegmentWidth( m_frame->GetActiveLayer() );

    // Arc geometric construction manager
    KIGFX::PREVIEW::ARC_GEOM_MANAGER arcManager;

    // Arc drawing assistant overlay
    KIGFX::PREVIEW::ARC_ASSISTANT arcAsst( arcManager, m_frame->GetUserUnits() );

    // Add a VIEW_GROUP that serves as a preview for the new item
    PCBNEW_SELECTION preview;
    m_view->Add( &preview );
    m_view->Add( &arcAsst );
    GRID_HELPER grid( m_toolMgr, m_frame->GetMagneticItemsSettings() );

    m_controls->ShowCursor( true );

    bool firstPoint = false;
    bool cancelled = false;

    // Prime the pump
    m_toolMgr->RunAction( ACTIONS::refreshPreview );

    if( aImmediateMode )
        m_toolMgr->RunAction( ACTIONS::cursorClick );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        PCB_LAYER_ID layer = m_frame->GetActiveLayer();
        graphic->SetLayer( layer );

        m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_PENCIL );
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( m_frame->IsGridVisible() );
        VECTOR2I cursorPos = grid.BestSnapAnchor( m_controls->GetMousePosition(), graphic );
        m_controls->ForceCursorPosition( true, cursorPos );

        auto cleanup = [&] () {
            preview.Clear();
            delete *aGraphic;
            *aGraphic = nullptr;
        };

        if( evt->IsCancelInteractive() )
        {
            cleanup();

            if( !firstPoint )
            {
                m_frame->PopTool( aTool );
                cancelled = true;
            }

            break;
        }
        else if( evt->IsActivate() )
        {
            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                cleanup();
                // leave ourselves on the stack so we come back after the move
                cancelled = true;
                break;
            }
            else
            {
                cleanup();
                m_frame->PopTool( aTool );
                cancelled = true;
                break;
            }
        }
        else if( evt->IsClick( BUT_LEFT ) )
        {
            if( !firstPoint )
            {
                m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );

                m_controls->SetAutoPan( true );
                m_controls->CaptureCursor( true );

                m_lineWidth = getSegmentWidth( m_frame->GetActiveLayer() );

                // Init the new item attributes
                // (non-geometric, those are handled by the manager)
                graphic->SetShape( S_ARC );
                graphic->SetWidth( m_lineWidth );

                preview.Add( graphic );
                firstPoint = true;
            }

            arcManager.AddPoint( cursorPos, true );
        }
        else if( evt->IsAction( &PCB_ACTIONS::deleteLastPoint ) )
        {
            arcManager.RemoveLastPoint();
        }
        else if( evt->IsMotion() )
        {
            // set angle snap
            arcManager.SetAngleSnap( evt->Modifier( MD_CTRL ) );

            // update, but don't step the manager state
            arcManager.AddPoint( cursorPos, false );
        }
        else if( evt->IsAction( &PCB_ACTIONS::layerChanged ) )
        {
            m_lineWidth = getSegmentWidth( m_frame->GetActiveLayer() );
            graphic->SetLayer( m_frame->GetActiveLayer() );
            graphic->SetWidth( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( graphic );
        }
        else if( evt->IsAction( &PCB_ACTIONS::properties ) )
        {
            if( firstPoint )
            {
                frame()->OnEditItemRequest( graphic );
                m_view->Update( &preview );
                frame()->SetMsgPanel( graphic );
            }
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        else if( evt->IsAction( &PCB_ACTIONS::incWidth ) )
        {
            m_lineWidth += WIDTH_STEP;
            graphic->SetWidth( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( graphic );
        }
        else if( evt->IsAction( &PCB_ACTIONS::decWidth ) && m_lineWidth > WIDTH_STEP )
        {
            m_lineWidth -= WIDTH_STEP;
            graphic->SetWidth( m_lineWidth );
            m_view->Update( &preview );
            frame()->SetMsgPanel( graphic );
        }
        else if( evt->IsAction( &PCB_ACTIONS::arcPosture ) )
        {
            arcManager.ToggleClockwise();
        }
        else
            evt->SetPassEvent();

        if( arcManager.IsComplete() )
        {
            break;
        }
        else if( arcManager.HasGeometryChanged() )
        {
            updateArcFromConstructionMgr( arcManager, *graphic );
            m_view->Update( &preview );
            m_view->Update( &arcAsst );

            if( firstPoint )
                frame()->SetMsgPanel( graphic );
            else
                frame()->SetMsgPanel( board() );
        }
    }

    preview.Remove( graphic );
    m_view->Remove( &arcAsst );
    m_view->Remove( &preview );
    frame()->SetMsgPanel( board() );
    m_controls->SetAutoPan( false );
    m_controls->CaptureCursor( false );
    m_controls->ForceCursorPosition( false );

    return !cancelled;
}


bool DRAWING_TOOL::getSourceZoneForAction( ZONE_MODE aMode, ZONE_CONTAINER** aZone )
{
    bool clearSelection = false;
    *aZone = nullptr;

    // not an action that needs a source zone
    if( aMode == ZONE_MODE::ADD || aMode == ZONE_MODE::GRAPHIC_POLYGON )
        return true;

    SELECTION_TOOL*         selTool = m_toolMgr->GetTool<SELECTION_TOOL>();
    const PCBNEW_SELECTION& selection = selTool->GetSelection();

    if( selection.Empty() )
    {
        clearSelection = true;
        m_toolMgr->RunAction( PCB_ACTIONS::selectionCursor, true );
    }

    // we want a single zone
    if( selection.Size() == 1 )
        *aZone = dyn_cast<ZONE_CONTAINER*>( selection[0] );

    // expected a zone, but didn't get one
    if( !*aZone )
    {
        if( clearSelection )
            m_toolMgr->RunAction( PCB_ACTIONS::selectionClear, true );

        return false;
    }

    return true;
}

int DRAWING_TOOL::DrawZone( const TOOL_EVENT& aEvent )
{
    if( m_editModules && !m_frame->GetModel() )
        return 0;

    ZONE_MODE zoneMode = aEvent.Parameter<ZONE_MODE>();
    MODE      drawMode = MODE::ZONE;

    if( aEvent.IsAction( &PCB_ACTIONS::drawZoneKeepout ) )
        drawMode = MODE::KEEPOUT;

    if( aEvent.IsAction( &PCB_ACTIONS::drawPolygon ) )
        drawMode = MODE::GRAPHIC_POLYGON;

    SCOPED_DRAW_MODE scopedDrawMode( m_mode, drawMode );

    // get a source zone, if we need one. We need it for:
    // ZONE_MODE::CUTOUT (adding a hole to the source zone)
    // ZONE_MODE::SIMILAR (creating a new zone using settings of source zone
    ZONE_CONTAINER* sourceZone = nullptr;

    if( !getSourceZoneForAction( zoneMode, &sourceZone ) )
        return 0;

    ZONE_CREATE_HELPER::PARAMS params;

    params.m_keepout = drawMode == MODE::KEEPOUT;
    params.m_mode = zoneMode;
    params.m_sourceZone = sourceZone;

    if( zoneMode == ZONE_MODE::SIMILAR )
        params.m_layer = sourceZone->GetLayer();
    else
        params.m_layer = m_frame->GetActiveLayer();

    ZONE_CREATE_HELPER zoneTool( *this, params );

    // the geometry manager which handles the zone geometry, and
    // hands the calculated points over to the zone creator tool
    POLYGON_GEOM_MANAGER polyGeomMgr( zoneTool );
    bool constrainAngle = false;

    std::string tool = aEvent.GetCommandStr().get();
    m_frame->PushTool( tool );
    Activate();    // register for events

    m_controls->ShowCursor( true );

    bool    started     = false;
    GRID_HELPER grid( m_toolMgr, m_frame->GetMagneticItemsSettings() );
    STATUS_TEXT_POPUP status( m_frame );
    status.SetTextColor( wxColour( 255, 0, 0 ) );
    status.SetText( _( "Self-intersecting polygons are not allowed" ) );

    // Prime the pump
    if( aEvent.HasPosition() )
        m_toolMgr->PrimeTool( aEvent.Position() );

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        m_frame->GetCanvas()->SetCurrentCursor( wxCURSOR_PENCIL );
        LSET layers( m_frame->GetActiveLayer() );
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( m_frame->IsGridVisible() );
        VECTOR2I cursorPos = grid.BestSnapAnchor( evt->IsPrime() ? evt->Position()
                                                                 : m_controls->GetMousePosition(),
                                                  layers );
        m_controls->ForceCursorPosition( true, cursorPos );

        if( ( sourceZone && sourceZone->GetHV45() ) || constrainAngle || evt->Modifier( MD_CTRL ) )
            polyGeomMgr.SetLeaderMode( POLYGON_GEOM_MANAGER::LEADER_MODE::DEG45 );
        else
            polyGeomMgr.SetLeaderMode( POLYGON_GEOM_MANAGER::LEADER_MODE::DIRECT );

        auto cleanup = [&] ()
                       {
                           polyGeomMgr.Reset();
                           started = false;
                           grid.ClearSkipPoint();
                           m_controls->SetAutoPan( false );
                           m_controls->CaptureCursor( false );
                       };

        if( evt->IsCancelInteractive())
        {
            if( polyGeomMgr.IsPolygonInProgress() )
                cleanup();
            else
            {
                m_frame->PopTool( tool );
                break;
            }
        }
        else if( evt->IsActivate() )
        {
            if( polyGeomMgr.IsPolygonInProgress() )
                cleanup();

            if( evt->IsPointEditor() )
            {
                // don't exit (the point editor runs in the background)
            }
            else if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( tool );
                break;
            }
        }
        else if( evt->IsAction( &PCB_ACTIONS::layerChanged ) )
        {
            if( zoneMode != ZONE_MODE::SIMILAR )
                params.m_layer = frame()->GetActiveLayer();
        }
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            m_menu.ShowContextMenu( selection() );
        }
        // events that lock in nodes
        else if( evt->IsClick( BUT_LEFT )
                 || evt->IsDblClick( BUT_LEFT )
                 || evt->IsAction( &PCB_ACTIONS::closeOutline ) )
        {
            // Check if it is double click / closing line (so we have to finish the zone)
            const bool endPolygon = evt->IsDblClick( BUT_LEFT )
                                    || evt->IsAction( &PCB_ACTIONS::closeOutline )
                                    || polyGeomMgr.NewPointClosesOutline( cursorPos );

            if( endPolygon )
            {
                polyGeomMgr.SetFinished();
                polyGeomMgr.Reset();

                started = false;
                m_controls->SetAutoPan( false );
                m_controls->CaptureCursor( false );
            }

            // adding a corner
            else if( polyGeomMgr.AddPoint( cursorPos ) )
            {
                if( !started )
                {
                    started = true;
                    constrainAngle = ( polyGeomMgr.GetLeaderMode() ==
                            POLYGON_GEOM_MANAGER::LEADER_MODE::DEG45 );
                    m_controls->SetAutoPan( true );
                    m_controls->CaptureCursor( true );
                }
            }
        }
        else if( evt->IsAction( &PCB_ACTIONS::deleteLastPoint ) )
        {
            polyGeomMgr.DeleteLastCorner();

            if( !polyGeomMgr.IsPolygonInProgress() )
            {
                // report finished as an empty shape
                polyGeomMgr.SetFinished();

                // start again
                started = false;
                m_controls->SetAutoPan( false );
                m_controls->CaptureCursor( false );
            }
        }
        else if( polyGeomMgr.IsPolygonInProgress()
                 && ( evt->IsMotion() || evt->IsDrag( BUT_LEFT ) ) )
        {
            polyGeomMgr.SetCursorPosition( cursorPos );

            if( polyGeomMgr.IsSelfIntersecting( true ) )
            {
                wxPoint p = wxGetMousePosition() + wxPoint( 20, 20 );
                status.Move( p );
                status.PopupFor( 1500 );
            }
            else
            {
                status.Hide();
            }
        }
        else if( evt->IsAction( &PCB_ACTIONS::properties ) )
        {
            if( started )
            {
                frame()->OnEditItemRequest( zoneTool.GetZone() );
                zoneTool.OnGeometryChange( polyGeomMgr );
                frame()->SetMsgPanel( zoneTool.GetZone() );
            }
        }
        else
            evt->SetPassEvent();

    }    // end while

    m_controls->ForceCursorPosition( false );
    return 0;
}


int DRAWING_TOOL::DrawVia( const TOOL_EVENT& aEvent )
{
    struct VIA_PLACER : public INTERACTIVE_PLACER_BASE
    {
        GRID_HELPER m_gridHelper;

        VIA_PLACER( PCB_BASE_EDIT_FRAME* aFrame ) :
            m_gridHelper( aFrame->GetToolManager(), aFrame->GetMagneticItemsSettings() )
        {}

        virtual ~VIA_PLACER()
        {
        }

        TRACK* findTrack( VIA* aVia )
        {
            const LSET lset = aVia->GetLayerSet();
            wxPoint position = aVia->GetPosition();
            BOX2I bbox = aVia->GetBoundingBox();

            std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> items;
            auto view = m_frame->GetCanvas()->GetView();
            std::vector<TRACK*> possible_tracks;

            view->Query( bbox, items );

            for( auto it : items )
            {
                BOARD_ITEM* item = static_cast<BOARD_ITEM*>( it.first );

                if( !(item->GetLayerSet() & lset ).any() )
                    continue;

                if( auto track = dyn_cast<TRACK*>( item ) )
                {
                    if( TestSegmentHit( position, track->GetStart(), track->GetEnd(),
                                        ( track->GetWidth() + aVia->GetWidth() ) / 2 ) )
                        possible_tracks.push_back( track );
                }
            }

            TRACK* return_track = nullptr;
            int min_d = std::numeric_limits<int>::max();
            for( auto track : possible_tracks )
            {
                SEG test( track->GetStart(), track->GetEnd() );
                auto dist = ( test.NearestPoint( position ) - position ).EuclideanNorm();

                if( dist < min_d )
                {
                    min_d = dist;
                    return_track = track;
                }
            }

            return return_track;
        }


        bool hasDRCViolation( VIA* aVia )
        {
            const LSET lset = aVia->GetLayerSet();
            wxPoint position = aVia->GetPosition();
            int drillRadius = aVia->GetDrillValue() / 2;
            BOX2I bbox = aVia->GetBoundingBox();

            std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> items;
            int net = 0;
            int clearance = 0;
            auto view = m_frame->GetCanvas()->GetView();
            int holeToHoleMin = m_frame->GetBoard()->GetDesignSettings().m_HoleToHoleMin;

            view->Query( bbox, items );

            for( auto it : items )
            {
                BOARD_ITEM* item = static_cast<BOARD_ITEM*>( it.first );

                if( !(item->GetLayerSet() & lset ).any() )
                    continue;

                if( TRACK* track = dyn_cast<TRACK*>( item ) )
                {
                    int max_clearance = std::max( clearance,
                                                  track->GetClearance( track->GetLayer() ) );

                    if( TestSegmentHit( position, track->GetStart(), track->GetEnd(),
                            ( track->GetWidth() + aVia->GetWidth() ) / 2  + max_clearance ) )
                    {
                        if( net && track->GetNetCode() != net )
                            return true;

                        net = track->GetNetCode();
                        clearance = track->GetClearance( track->GetLayer() );
                    }
                }

                if( VIA* via = dyn_cast<VIA*>( item ) )
                {
                    int dist = KiROUND( GetLineLength( position, via->GetPosition() ) );

                    if( dist < drillRadius + via->GetDrillValue() / 2 + holeToHoleMin )
                        return true;
                }

                if( MODULE* mod = dyn_cast<MODULE*>( item ) )
                {
                    for( D_PAD* pad : mod->Pads() )
                    {
                        for( PCB_LAYER_ID layer : pad->GetLayerSet().Seq() )
                        {
                            int max_clearance = std::max( clearance, pad->GetClearance( layer ) );

                            if( pad->HitTest( aVia->GetBoundingBox(), false, max_clearance ) )
                            {
                                if( net && pad->GetNetCode() != net )
                                    return true;

                                net = pad->GetNetCode();
                                clearance = pad->GetClearance( layer );
                            }

                            if( pad->GetDrillSize().x && pad->GetDrillShape() == PAD_DRILL_SHAPE_CIRCLE )
                            {
                                int dist = KiROUND( GetLineLength( position, pad->GetPosition() ) );

                                if( dist < drillRadius + pad->GetDrillSize().x / 2 + holeToHoleMin )
                                    return true;
                            }
                        }
                    }
                }
            }

            return false;
        }


        int findStitchedZoneNet( VIA* aVia )
        {
            const wxPoint position = aVia->GetPosition();
            const LSET    lset = aVia->GetLayerSet();

            for( auto mod : m_board->Modules() )
            {
                for( D_PAD* pad : mod->Pads() )
                {
                    if( pad->HitTest( position ) && ( pad->GetLayerSet() & lset ).any() )
                        return -1;
                }
            }

            std::vector<ZONE_CONTAINER*> foundZones;

            for( ZONE_CONTAINER* zone : m_board->Zones() )
            {
                for( PCB_LAYER_ID layer : LSET( zone->GetLayerSet() & lset ).Seq() )
                    if( zone->HitTestFilledArea( layer, position ) )
                        foundZones.push_back( zone );
            }

            std::sort( foundZones.begin(), foundZones.end(),
                [] ( const ZONE_CONTAINER* a, const ZONE_CONTAINER* b )
                {
                    return a->GetLayer() < b->GetLayer();
                } );

            // first take the net of the active layer
            for( ZONE_CONTAINER* z : foundZones )
            {
                if( m_frame->GetActiveLayer() == z->GetLayer() )
                    return z->GetNetCode();
            }

            // none? take the topmost visible layer
            for( ZONE_CONTAINER* z : foundZones )
            {
                if( m_board->IsLayerVisible( z->GetLayer() ) )
                    return z->GetNetCode();
            }

            return -1;
        }

        void SnapItem( BOARD_ITEM *aItem ) override
        {
            // If you place a Via on a track but not on its centerline, the current
            // connectivity algorithm will require us to put a kink in the track when
            // we break it (so that each of the two segments ends on the via center).
            // That's not ideal, and is in fact probably worse than forcing snap in
            // this situation.

            m_gridHelper.SetSnap( !( m_modifiers & MD_SHIFT ) );
            auto    via = static_cast<VIA*>( aItem );
            wxPoint position = via->GetPosition();
            TRACK*  track = findTrack( via );

            if( track )
            {
                SEG      trackSeg( track->GetStart(), track->GetEnd() );
                VECTOR2I snap = m_gridHelper.AlignToSegment( position, trackSeg );

                aItem->SetPosition( (wxPoint) snap );
            }
        }

        bool PlaceItem( BOARD_ITEM* aItem, BOARD_COMMIT& aCommit ) override
        {
            VIA*    via = static_cast<VIA*>( aItem );
            wxPoint viaPos = via->GetPosition();
            int     newNet;
            TRACK*  track = findTrack( via );

            if( hasDRCViolation( via ) )
                return false;

            if( track )
            {
                if( viaPos != track->GetStart() && viaPos != track->GetEnd() )
                {
                    aCommit.Modify( track );

                    TRACK* newTrack = dynamic_cast<TRACK*>( track->Clone() );
                    const_cast<KIID&>( newTrack->m_Uuid ) = KIID();

                    track->SetEnd( viaPos );
                    newTrack->SetStart( viaPos );
                    aCommit.Add( newTrack );
                }

                newNet = track->GetNetCode();
            }
            else
                newNet = findStitchedZoneNet( via );

            if( newNet > 0 )
                via->SetNetCode( newNet );

            aCommit.Add( aItem );
            return true;
        }

        std::unique_ptr<BOARD_ITEM> CreateItem() override
        {
            auto&   ds = m_board->GetDesignSettings();
            VIA*    via = new VIA( m_board );

            via->SetNetCode( 0 );
            via->SetViaType( ds.m_CurrentViaType );

            // for microvias, the size and hole will be changed later.
            via->SetWidth( ds.GetCurrentViaSize() );
            via->SetDrill( ds.GetCurrentViaDrill() );

            // Usual via is from copper to component.
            // layer pair is B_Cu and F_Cu.
            via->SetLayerPair( B_Cu, F_Cu );

            PCB_LAYER_ID    first_layer = m_frame->GetActiveLayer();
            PCB_LAYER_ID    last_layer;

            // prepare switch to new active layer:
            if( first_layer != m_frame->GetScreen()->m_Route_Layer_TOP )
                last_layer = m_frame->GetScreen()->m_Route_Layer_TOP;
            else
                last_layer = m_frame->GetScreen()->m_Route_Layer_BOTTOM;

            // Adjust the actual via layer pair
            switch( via->GetViaType() )
            {
            case VIATYPE::BLIND_BURIED:
                via->SetLayerPair( first_layer, last_layer );
                break;

            case VIATYPE::MICROVIA: // from external to the near neighbor inner layer
            {
                PCB_LAYER_ID last_inner_layer =
                    ToLAYER_ID( ( m_board->GetCopperLayerCount() - 2 ) );

                if( first_layer == B_Cu )
                    last_layer = last_inner_layer;
                else if( first_layer == F_Cu )
                    last_layer = In1_Cu;
                else if( first_layer == last_inner_layer )
                    last_layer = B_Cu;
                else if( first_layer == In1_Cu )
                    last_layer = F_Cu;

                // else error: will be removed later
                via->SetLayerPair( first_layer, last_layer );

                // Update diameter and hole size, which where set previously
                // for normal vias
                NETINFO_ITEM* net = via->GetNet();

                if( net )
                {
                    via->SetWidth( net->GetMicroViaSize() );
                    via->SetDrill( net->GetMicroViaDrillSize() );
                }
            }
            break;

            default:
                break;
            }

            return std::unique_ptr<BOARD_ITEM>( via );
        }
    };

    VIA_PLACER placer( frame() );

    SCOPED_DRAW_MODE scopedDrawMode( m_mode, MODE::VIA );

    doInteractiveItemPlacement( aEvent.GetCommandStr().get(), &placer, _( "Place via" ),
                                IPO_REPEAT | IPO_SINGLE_CLICK );

    return 0;
}


int DRAWING_TOOL::getSegmentWidth( PCB_LAYER_ID aLayer ) const
{
    assert( m_board );
    return m_board->GetDesignSettings().GetLineThickness( aLayer );
}


const unsigned int DRAWING_TOOL::WIDTH_STEP = Millimeter2iu( 0.1 );


void DRAWING_TOOL::setTransitions()
{
    Go( &DRAWING_TOOL::DrawLine,              PCB_ACTIONS::drawLine.MakeEvent() );
    Go( &DRAWING_TOOL::DrawZone,              PCB_ACTIONS::drawPolygon.MakeEvent() );
    Go( &DRAWING_TOOL::DrawRectangle,         PCB_ACTIONS::drawRectangle.MakeEvent() );
    Go( &DRAWING_TOOL::DrawCircle,            PCB_ACTIONS::drawCircle.MakeEvent() );
    Go( &DRAWING_TOOL::DrawArc,               PCB_ACTIONS::drawArc.MakeEvent() );
    Go( &DRAWING_TOOL::DrawDimension,         PCB_ACTIONS::drawAlignedDimension.MakeEvent() );
    Go( &DRAWING_TOOL::DrawDimension,         PCB_ACTIONS::drawCenterDimension.MakeEvent() );
    Go( &DRAWING_TOOL::DrawDimension,         PCB_ACTIONS::drawLeader.MakeEvent() );
    Go( &DRAWING_TOOL::DrawZone,              PCB_ACTIONS::drawZone.MakeEvent() );
    Go( &DRAWING_TOOL::DrawZone,              PCB_ACTIONS::drawZoneKeepout.MakeEvent() );
    Go( &DRAWING_TOOL::DrawZone,              PCB_ACTIONS::drawZoneCutout.MakeEvent() );
    Go( &DRAWING_TOOL::DrawZone,              PCB_ACTIONS::drawSimilarZone.MakeEvent() );
    Go( &DRAWING_TOOL::DrawVia,               PCB_ACTIONS::drawVia.MakeEvent() );
    Go( &DRAWING_TOOL::PlaceText,             PCB_ACTIONS::placeText.MakeEvent() );
    Go( &DRAWING_TOOL::PlaceImportedGraphics, PCB_ACTIONS::placeImportedGraphics.MakeEvent() );
    Go( &DRAWING_TOOL::SetAnchor,             PCB_ACTIONS::setAnchor.MakeEvent() );
}
