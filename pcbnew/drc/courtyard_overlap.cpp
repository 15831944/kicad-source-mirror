/**
 * @file drc_marker_functions.cpp
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2010 Dick Hollenbeck, dick@softplc.com
 * Copyright (C) 2004-2017 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2018 KiCad Developers, see AUTHORS.txt for contributors.
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


#include <drc/courtyard_overlap.h>

#include <class_module.h>
#include <drc/drc.h>

#include <widgets/ui_common.h>

#include <memory>

/**
 * Flag to enable courtyard DRC debug tracing.
 *
 * Use "KICAD_DRC_COURTYARD" to enable.
 *
 * @ingroup trace_env_vars
 */
static const wxChar* DRC_COURTYARD_TRACE = wxT( "KICAD_DRC_COURTYARD" );


DRC_COURTYARD_OVERLAP::DRC_COURTYARD_OVERLAP( MARKER_HANDLER aMarkerHandler ) :
        DRC_PROVIDER( aMarkerHandler )
{
}


bool DRC_COURTYARD_OVERLAP::RunDRC( EDA_UNITS aUnits, BOARD& aBoard ) const
{
    wxLogTrace( DRC_COURTYARD_TRACE, "Running DRC: Courtyard" );

    // Detects missing (or malformed) footprint courtyard,
    // and for footprint with courtyard, courtyards overlap.
    wxString msg;
    bool     success = true;

    // Update courtyard polygons, and test for missing courtyard definition:
    for( MODULE* footprint : aBoard.Modules() )
    {
        wxPoint pos = footprint->GetPosition();
        bool    is_ok = footprint->BuildPolyCourtyard();

        if( !is_ok && !aBoard.GetDesignSettings().Ignore( DRCE_OVERLAPPING_FOOTPRINTS ) )
        {
            auto m = std::make_unique<MARKER_PCB>( aUnits, DRCE_MALFORMED_COURTYARD_IN_FOOTPRINT,
                                                   pos, footprint );
            HandleMarker( std::move( m ) );
            success = false;
        }

        if( aBoard.GetDesignSettings().Ignore( DRCE_MISSING_COURTYARD_IN_FOOTPRINT ) )
            continue;

        if( footprint->GetPolyCourtyardFront().OutlineCount() == 0
                && footprint->GetPolyCourtyardBack().OutlineCount() == 0 && is_ok )
        {
            auto m = std::make_unique<MARKER_PCB>( aUnits, DRCE_MISSING_COURTYARD_IN_FOOTPRINT,
                                                   pos, footprint );
            HandleMarker( std::move( m ) );
            success = false;
        }
    }

    if( aBoard.GetDesignSettings().Ignore( DRCE_OVERLAPPING_FOOTPRINTS ) )
        return success;

    wxLogTrace( DRC_COURTYARD_TRACE, "Checking for courtyard overlap" );

    // Now test for overlapping on top layer:
    SHAPE_POLY_SET courtyard; // temporary storage of the courtyard of current footprint

    for( auto it1 = aBoard.Modules().begin(); it1 != aBoard.Modules().end(); it1++ )
    {
        MODULE* footprint = *it1;

        if( footprint->GetPolyCourtyardFront().OutlineCount() == 0 )
            continue; // No courtyard defined

        for( auto it2 = it1 + 1; it2 != aBoard.Modules().end(); it2++ )
        {
            MODULE* candidate = *it2;

            if( candidate->GetPolyCourtyardFront().OutlineCount() == 0 )
                continue; // No courtyard defined

            courtyard.RemoveAllContours();
            courtyard.Append( footprint->GetPolyCourtyardFront() );

            // Build the common area between footprint and the candidate:
            courtyard.BooleanIntersection( candidate->GetPolyCourtyardFront(),
                                           SHAPE_POLY_SET::PM_FAST );

            // If no overlap, courtyard is empty (no common area).
            // Therefore if a common polygon exists, this is a DRC error
            if( courtyard.OutlineCount() )
            {
                //Overlap between footprint and candidate
                auto  m = std::make_unique<MARKER_PCB>( aUnits, DRCE_OVERLAPPING_FOOTPRINTS,
                                                        (wxPoint) courtyard.CVertex( 0, 0, -1 ),
                                                        footprint, candidate );
                HandleMarker( std::move( m ) );
                success = false;
            }
        }
    }

    // Test for overlapping on bottom layer:
    for( auto it1 = aBoard.Modules().begin(); it1 != aBoard.Modules().end(); it1++ )
    {
        MODULE* footprint = *it1;

        if( footprint->GetPolyCourtyardBack().OutlineCount() == 0 )
            continue; // No courtyard defined

        for( auto it2 = it1 + 1; it2 != aBoard.Modules().end(); it2++ )
        {
            MODULE* candidate = *it2;

            if( candidate->GetPolyCourtyardBack().OutlineCount() == 0 )
                continue; // No courtyard defined

            courtyard.RemoveAllContours();
            courtyard.Append( footprint->GetPolyCourtyardBack() );

            // Build the common area between footprint and the candidate:
            courtyard.BooleanIntersection( candidate->GetPolyCourtyardBack(),
                                           SHAPE_POLY_SET::PM_FAST );

            // If no overlap, courtyard is empty (no common area).
            // Therefore if a common polygon exists, this is a DRC error
            if( courtyard.OutlineCount() )
            {
                //Overlap between footprint and candidate
                auto  m = std::make_unique<MARKER_PCB>( aUnits, DRCE_OVERLAPPING_FOOTPRINTS,
                                                        (wxPoint) courtyard.CVertex( 0, 0, -1 ),
                                                        footprint, candidate );
                HandleMarker( std::move( m ) );
                success = false;
            }
        }
    }

    return success;
}
