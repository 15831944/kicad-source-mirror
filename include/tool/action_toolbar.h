/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 KiCad Developers, see CHANGELOG.txt for contributors.
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

#ifndef ACTION_TOOLBAR_H
#define ACTION_TOOLBAR_H

#include <map>
#include <wx/bitmap.h>          // Needed for the auibar include
#include <wx/aui/auibar.h>
#include <tool/tool_event.h>

class ACTION_MENU;
class EDA_BASE_FRAME;
class TOOL_MANAGER;
class TOOL_ACTION;

/**
 * ACTION_TOOLBAR
 *
 * Defines the structure of a toolbar with buttons that invoke ACTIONs.
 */
class ACTION_TOOLBAR : public wxAuiToolBar
{
public:
    ACTION_TOOLBAR( EDA_BASE_FRAME* parent, wxWindowID id = wxID_ANY,
                    const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                    long style = wxAUI_TB_DEFAULT_STYLE );

    virtual ~ACTION_TOOLBAR();

    /**
     * Adds a TOOL_ACTION-based button to the toolbar. After selecting the entry,
     * a TOOL_EVENT command containing name of the action is sent.
     *
     * @param aAction is the action to add
     * @param aIsToggleEntry makes the toolbar item a toggle entry when true
     * @param aIsCancellable when true, cancels the tool if clicked when tool is active
     */
    void Add( const TOOL_ACTION& aAction, bool aIsToggleEntry = false,
              bool aIsCancellable = false );

    /**
     * Adds a large button such as used in the Kicad Manager Frame's launch bar.
     *
     * @param aAction
     */
    void AddButton( const TOOL_ACTION& aAction );

    /**
     * Add a separator that introduces space on either side to not squash the tools
     * when scaled.
     *
     * @param aWindow is the window to get the scaling factor of
     */
    void AddScaledSeparator( wxWindow* aWindow );

    /**
     * Add a context menu to a specific tool item on the toolbar.
     * This toolbar gets ownership of the menu object, and will delete it when the
     * ClearToolbar() function is called.
     *
     * @param aAction is the action to get the menu
     * @param aMenu is the context menu
     */
    void AddToolContextMenu( const TOOL_ACTION& aAction, ACTION_MENU* aMenu );

    /**
     * Clear the toolbar and remove all associated menus.
     */
    void ClearToolbar();

    /**
     * Updates the bitmap of a particular tool.  Not icon-based because we use it
     * for the custom-drawn layer pair bitmap.
     */
    void SetToolBitmap( const TOOL_ACTION& aAction, const wxBitmap& aBitmap );

    /**
     * Applies the default toggle action.  For checked items this is check/uncheck; for
     * non-checked items it's enable/disable.
     */
    void Toggle( const TOOL_ACTION& aAction, bool aState );

    void Toggle( const TOOL_ACTION& aAction, bool aEnabled, bool aChecked );

    static constexpr bool TOGGLE = true;
    static constexpr bool CANCEL = true;

protected:
    ///> The default tool event handler.
    void onToolEvent( wxAuiToolBarEvent& aEvent );

    ///> Handle a right-click on a menu item
    void onToolRightClick( wxAuiToolBarEvent& aEvent );

protected:
    TOOL_MANAGER* m_toolManager;
    std::map<int, bool>               m_toolKinds;
    std::map<int, bool>               m_toolCancellable;
    std::map<int, const TOOL_ACTION*> m_toolActions;
    std::map<int, ACTION_MENU*>       m_toolMenus;
};

#endif
