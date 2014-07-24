/*
  Copyright 2014 Andreas Lauser

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <config.h>

#if HAVE_DYNAMIC_BOOST_TEST
#define BOOST_TEST_DYN_LINK
#endif

#define BOOST_TEST_MODULE TransmissibilityMultipliers

#include <opm/autodiff/GeoProps.hpp>
#include <opm/autodiff/BlackoilPropsAdFromDeck.hpp>

#include <opm/core/grid/GridManager.hpp>
#include <opm/core/utility/parameters/ParameterGroup.hpp>

#include <opm/parser/eclipse/Parser/Parser.hpp>
#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>

#if HAVE_DUNE_CORNERPOINT
#include <dune/common/version.hh>
#if DUNE_VERSION_NEWER(DUNE_GRID, 2,3)
#include <dune/common/parallel/mpihelper.hh>
#else
#include <dune/common/mpihelper.hh>
#endif
#include <dune/grid/CpGrid.hpp>
#endif

#include <boost/test/unit_test.hpp>

#include <string>

#include <stdlib.h>

// as surprising as it seems, this is a minimal deck required to get to the point where
// the transmissibilities are calculated. The problem here is that the OPM property
// objects mix fluid and rock properties, so that all properties need to be defined even
// if they are not of interest :/
std::string deckPreMult =
    "RUNSPEC\n"
    "TABDIMS\n"
    "/\n"
    "OIL\n"
    "GAS\n"
    "WATER\n"
    "METRIC\n"
    "DIMENS\n"
    "2 2 2/\n"
    "GRID\n"
    "DXV\n"
    "1.0 2.0 /\n"
    "DYV\n"
    "3.0 4.0 /\n"
    "DZV\n"
    "5.0 6.0/\n"
    "TOPS\n"
    "4*100 /\n";
std::string deckPostMult =
    "PROPS\n"
    "DENSITY\n"
    "100 200 300 /\n"
    "PVTW\n"
    " 100 1 1e-6 1.0 0 /\n"
    "PVDG\n"
    "1 1 1e-2\n"
    "100 0.25 2e-2 /\n"
    "PVTO\n"
    "1e-3 1.0 1.0 1.0\n"
    "     100.0 1.0 1.0\n"
    "/\n"
    "1.0 10.0 1.1 0.9\n"
    "    100.0 1.1 0.9\n"
    "/\n"
    "/\n"
    "SWOF\n"
    "0.0 0.0 1.0 0.0\n"
    "1.0 1.0 0.0 1.0/\n"
    "SGOF\n"
    "0.0 0.0 1.0 0.0\n"
    "1.0 1.0 0.0 1.0/\n"
    "PORO\n"
    "8*0.3 /\n"
    "PERMX\n"
    "8*1 /\n"
    "SCHEDULE\n"
    "TSTEP\n"
    "1.0 2.0 3.0 4.0 /\n"
    "/\n";

std::string origDeckString = deckPreMult + deckPostMult;
std::string multDeckString =
    deckPreMult +
    "MULTX\n" +
    "1 2 3 4 5 6 7 8 /\n" +
    "MULTY\n" +
    "1 2 3 4 5 6 7 8 /\n" +
    "MULTZ\n" +
    "1 2 3 4 5 6 7 8 /\n" +
    deckPostMult;

std::string multMinusDeckString =
    deckPreMult +
    "MULTX-\n" +
    "1 2 3 4 5 6 7 8 /\n" +
    "MULTY-\n" +
    "1 2 3 4 5 6 7 8 /\n" +
    "MULTZ-\n" +
    "1 2 3 4 5 6 7 8 /\n" +
    deckPostMult;

// the NTG values get harmonically averaged for the transmissibilites. If the value
// is the same on both sides, the averaging buils down to a no-op, though...
std::string ntgDeckString =
    deckPreMult +
    "NTG\n" +
    "8*0.5 /\n" +
    deckPostMult;


BOOST_AUTO_TEST_CASE(TransmissibilityMultipliersLegacyGridInterface)
{
    Opm::parameter::ParameterGroup param;
    Opm::ParserPtr parser(new Opm::Parser() );

    /////
    // create a DerivedGeology object without any multipliers involved
    Opm::DeckConstPtr origDeck = parser->parseString(origDeckString);
    Opm::EclipseStateConstPtr origEclipseState(new Opm::EclipseState(origDeck));

    auto origGridManager = std::make_shared<Opm::GridManager>(origEclipseState->getEclipseGrid());
    auto origProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(origDeck, origEclipseState, *(origGridManager->c_grid()));

    Opm::DerivedGeology origGeology(*(origGridManager->c_grid()), *origProps, origEclipseState);
    /////

    /////
    // create a DerivedGeology object _with_ transmissibility multipliers involved
    Opm::DeckConstPtr multDeck = parser->parseString(multDeckString);
    Opm::EclipseStateConstPtr multEclipseState(new Opm::EclipseState(multDeck));

    auto multGridManager = std::make_shared<Opm::GridManager>(multEclipseState->getEclipseGrid());
    auto multProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(multDeck, multEclipseState, *(multGridManager->c_grid()));

    Opm::DerivedGeology multGeology(*(multGridManager->c_grid()), *multProps, multEclipseState);
    /////

    /////
    // create a DerivedGeology object _with_ transmissibility multipliers involved for
    // the negative faces
    Opm::DeckConstPtr multMinusDeck = parser->parseString(multMinusDeckString);
    Opm::EclipseStateConstPtr multMinusEclipseState(new Opm::EclipseState(multMinusDeck));

    auto multMinusGridManager = std::make_shared<Opm::GridManager>(multMinusEclipseState->getEclipseGrid());
    auto multMinusProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(multMinusDeck, multMinusEclipseState, *(multMinusGridManager->c_grid()));

    Opm::DerivedGeology multMinusGeology(*(multMinusGridManager->c_grid()), *multMinusProps, multMinusEclipseState);
    /////

    /////
    // create a DerivedGeology object with the NTG keyword involved
    Opm::DeckConstPtr ntgDeck = parser->parseString(ntgDeckString);
    Opm::EclipseStateConstPtr ntgEclipseState(new Opm::EclipseState(ntgDeck));

    auto ntgGridManager = std::make_shared<Opm::GridManager>(ntgEclipseState->getEclipseGrid());
    auto ntgProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(ntgDeck, ntgEclipseState, *(ntgGridManager->c_grid()));

    Opm::DerivedGeology ntgGeology(*(ntgGridManager->c_grid()), *ntgProps, ntgEclipseState);
    /////

    // compare the transmissibilities (note that for this we assume that the multipliers
    // do not change the grid topology)
    const UnstructuredGrid cGrid =  *(origGridManager->c_grid());
    int numFaces = cGrid.number_of_faces;
    for (int faceIdx = 0; faceIdx < numFaces; ++ faceIdx) {
        // in DUNE-speak, a face here is more like an intersection which is not specific
        // to a codim-0 entity (i.e., cell)

        // get the cell indices of the compressed grid for the face's interior and
        // exterior cell
        int insideCellIdx = cGrid.face_cells[2*faceIdx + 0];
        int outsideCellIdx = cGrid.face_cells[2*faceIdx + 1];

        if (insideCellIdx < 0 || outsideCellIdx < 0) {
            // do not consider cells at the domain boundary: Their would only be used for
            // Dirichlet-like boundary conditions which have not been implemented so
            // far...
            continue;
        }

        // translate these to canonical indices (i.e., the logically Cartesian ones used by the deck)
        if (cGrid.global_cell) {
            insideCellIdx = cGrid.global_cell[insideCellIdx];
            outsideCellIdx = cGrid.global_cell[outsideCellIdx];
        }

        double origTrans = origGeology.transmissibility()[faceIdx];
        double multTrans = multGeology.transmissibility()[faceIdx];
        double multMinusTrans = multMinusGeology.transmissibility()[faceIdx];
        double ntgTrans = ntgGeology.transmissibility()[faceIdx];
        BOOST_CHECK_CLOSE(origTrans*(insideCellIdx + 1), multTrans, 1e-6);
        BOOST_CHECK_CLOSE(origTrans*(outsideCellIdx + 1), multMinusTrans, 1e-6);

        int insideCellKIdx = insideCellIdx/(cGrid.cartdims[0]*cGrid.cartdims[1]);
        int outsideCellKIdx = outsideCellIdx/(cGrid.cartdims[0]*cGrid.cartdims[1]);
        if (insideCellKIdx == outsideCellKIdx)
            // NTG only reduces the permebility of the X-Y plane
            BOOST_CHECK_CLOSE(origTrans*0.5, ntgTrans, 1e-6);
    }
}

#if HAVE_DUNE_CORNERPOINT
BOOST_AUTO_TEST_CASE(TransmissibilityMultipliersCpGrid)
{
    int argc = 1;
    char **argv;
    argv = new (char*);
    argv[0] = strdup("footest");

    Dune::MPIHelper::instance(argc, argv);

    Opm::parameter::ParameterGroup param;
    Opm::ParserPtr parser(new Opm::Parser() );

#warning TODO: there seems to be some index mess-up in DerivedGeology in the case of Dune::CpGrid
#if 0

    /////
    // create a DerivedGeology object without any multipliers involved
    Opm::DeckConstPtr origDeck = parser->parseString(origDeckString);
    Opm::EclipseStateConstPtr origEclipseState(new Opm::EclipseState(origDeck));

    auto origGrid = std::make_shared<Dune::CpGrid>();
    origGrid->processEclipseFormat(origEclipseState->getEclipseGrid(), 0.0, false);

    auto origProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(origDeck,
                                                                    origEclipseState,
                                                                    *origGrid);

    Opm::DerivedGeology origGeology(*origGrid, *origProps, origEclipseState);
    /////

    /////
    // create a DerivedGeology object _with_ transmissibility multipliers involved
    Opm::DeckConstPtr multDeck = parser->parseString(multDeckString);
    Opm::EclipseStateConstPtr multEclipseState(new Opm::EclipseState(multDeck));

    auto multGrid = std::make_shared<Dune::CpGrid>();
    multGrid->processEclipseFormat(multEclipseState->getEclipseGrid(), 0.0, false);

    auto multProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(multDeck, multEclipseState, *multGrid);

    Opm::DerivedGeology multGeology(*multGrid, *multProps, multEclipseState);
    /////

    /////
    // create a DerivedGeology object _with_ transmissibility multipliers involved for
    // the negative faces
    Opm::DeckConstPtr multMinusDeck = parser->parseString(multMinusDeckString);
    Opm::EclipseStateConstPtr multMinusEclipseState(new Opm::EclipseState(multMinusDeck));

    auto multMinusGrid = std::make_shared<Dune::CpGrid>();
    multMinusGrid->processEclipseFormat(multMinusEclipseState->getEclipseGrid(), 0.0, false);

    auto multMinusProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(multMinusDeck, multMinusEclipseState, *multMinusGrid);

    Opm::DerivedGeology multMinusGeology(*multMinusGrid, *multMinusProps, multMinusEclipseState);
    /////

    /////
    // create a DerivedGeology object with the NTG keyword involved
    Opm::DeckConstPtr ntgDeck = parser->parseString(ntgDeckString);
    Opm::EclipseStateConstPtr ntgEclipseState(new Opm::EclipseState(ntgDeck));

    auto ntgGrid = std::make_shared<Dune::CpGrid>();
    ntgGrid->processEclipseFormat(ntgEclipseState->getEclipseGrid(), 0.0, false);

    auto ntgProps = std::make_shared<Opm::BlackoilPropsAdFromDeck>(ntgDeck, ntgEclipseState, *ntgGrid);

    Opm::DerivedGeology ntgGeology(*ntgGrid, *ntgProps, ntgEclipseState);
    /////

    // compare the transmissibilities (note that for this we assume that the multipliers
    // do not change the grid topology)
#if DUNE_VERSION_NEWER(DUNE_GRID, 2,3)
    auto gridView = origGrid->leafGridView();
#else
    auto gridView = origGrid->leafView();
#endif
    typedef Dune::MultipleCodimMultipleGeomTypeMapper<Dune::CpGrid::LeafGridView,
                                                      Dune::MCMGElementLayout> ElementMapper;
    ElementMapper elementMapper(gridView);
    auto eIt = gridView.begin<0>();
    const auto& eEndIt = gridView.end<0>();
    for (; eIt < eEndIt; ++ eIt) {
        // loop over the intersections of the current element
        auto isIt = gridView.ibegin(*eIt);
        const auto& isEndIt = gridView.iend(*eIt);
        for (; isIt != isEndIt; ++isIt) {
            if (isIt->boundary())
                continue; // ignore domain the boundaries

            // get the cell indices of the compressed grid for the face's interior and
            // exterior cell
            int insideCellIdx = elementMapper.map(*isIt->inside());
            int outsideCellIdx = elementMapper.map(*isIt->outside());

            // translate these to canonical indices (i.e., the logically Cartesian ones used by the deck)
            insideCellIdx = origGrid->globalCell()[insideCellIdx];
            outsideCellIdx = origGrid->globalCell()[outsideCellIdx];

#warning TODO: how to get the intersection index for the compressed grid??
#if 0
            int globalIsIdx = 0; // <- how to get this??
            double origTrans = origGeology.transmissibility()[globalIsIdx];
            double multTrans = multGeology.transmissibility()[globalIsIdx];
            double multMinusTrans = multMinusGeology.transmissibility()[globalIsIdx];
            double ntgTrans = ntgGeology.transmissibility()[globalIsIdx];
            BOOST_CHECK_CLOSE(origTrans*(insideCellIdx + 1), multTrans, 1e-6);
            BOOST_CHECK_CLOSE(origTrans*(outsideCellIdx + 1), multMinusTrans, 1e-6);

            std::array<int, 3> ijkInside, ijkOutside;
            origGrid->getIJK(insideCellIdx, ijkInside);
            origGrid->getIJK(outsideCellIdx, ijkOutside);
            if (ijkInside[2] == ijkOutside[2])
                // NTG only reduces the permebility of the X-Y plane
                BOOST_CHECK_CLOSE(origTrans*0.5, ntgTrans, 1e-6);
#endif
        }
    }
#endif
}

#endif
