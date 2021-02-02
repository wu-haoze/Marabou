/*********************                                                        */
/*! \file Test_HeuristicCostManager.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors gurobiWrappered in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief [[ Add one-line brief description here ]]
 **
 ** [[ Add lengthier description here ]]
 **/

#include <cxxtest/TestSuite.h>

#include "context/context.h"
#include "Engine.h"
#include "Equation.h"
#include "FloatUtils.h"
#include "MockSolver.h"
#include "HeuristicCostManager.h"
#include "InputQuery.h"
#include "Options.h"
#include "PiecewiseLinearConstraint.h"
#include "ReluConstraint.h"

class HeuristicCostManagerTestSuite : public CxxTest::TestSuite
{
public:

    void setUp()
    {
    }

    void tearDown()
    {
    }

    void initializeHeuristicCostManager( InputQuery &inputQuery, Engine &engine,
                                         HeuristicCostManager &heuristicCostManager,
                                         MockSolver &solver )
    {
        inputQuery.setNumberOfVariables( 5 );
        ReluConstraint *r1 = new ReluConstraint( 0, 1);
        ReluConstraint *r2 = new ReluConstraint( 2, 3);
        inputQuery.addPiecewiseLinearConstraint( r1 );
        inputQuery.addPiecewiseLinearConstraint( r2 );
        inputQuery.setLowerBound( 0, -1 );
        inputQuery.setUpperBound( 0, 1 );
        inputQuery.setLowerBound( 1, -1 );
        inputQuery.setUpperBound( 1, 1 );
        inputQuery.setLowerBound( 2, -2 );
        inputQuery.setUpperBound( 2, 2 );
        inputQuery.setLowerBound( 3, 0 );
        inputQuery.setUpperBound( 3, 2 );
        inputQuery.setLowerBound( 4, -1 );
        inputQuery.setUpperBound( 4, 1 );

        Equation eq1;
        eq1.addAddend( 1, 0 );
        eq1.addAddend( -1, 4 );
        inputQuery.addEquation( eq1 );

        Equation eq2;
        eq2.addAddend( 1, 2 );
        eq2.addAddend( -2, 4 );
        inputQuery.addEquation( eq2 );

        TS_ASSERT_THROWS_NOTHING( engine.processInputQuery( inputQuery, false ) );

        List<PiecewiseLinearConstraint *> &constraints = engine.getPiecewiseLinearConstraints();
        heuristicCostManager.setPLConstraints( constraints );

        for ( unsigned i = 0; i < inputQuery.getNumberOfVariables(); ++i )
        {
            solver.setLowerBound( Stringf( "x%u", i ), inputQuery.getLowerBound( i ) );
            solver.setUpperBound( Stringf( "x%u", i ), inputQuery.getUpperBound( i ) );
        }

        heuristicCostManager.setGurobi( &solver );

        heuristicCostManager.setNetworkLevelReasoner( engine.getInputQuery()->getNetworkLevelReasoner() );

        for ( const auto &plConstraint : constraints )
        {
            plConstraint->registerGurobi( &solver );
        }
    }

    void test_initiate_heuristic_cost_currentAssignment()
    {
        Options::get()->setString( Options::INITIALIZATION_STRATEGY, "currentAssignment" );
        InputQuery inputQuery;
        Engine engine;
        HeuristicCostManager heuristicCostManager( &engine );
        MockSolver solver;
        initializeHeuristicCostManager( inputQuery, engine, heuristicCostManager, solver );

        solver.setValue( "x0", 0.5 );
        solver.setValue( "x1", 0.7 );
        solver.setValue( "x2", -1 );
        solver.setValue( "x3", 0.5 );
        solver.setValue( "x4", 1 );

        TS_ASSERT_THROWS_NOTHING( heuristicCostManager.initiateCostFunctionForLocalSearch(); );

        Map<unsigned, double> expectedHeuristicCost;
        expectedHeuristicCost[0] = -1;
        expectedHeuristicCost[1] = 1;
        expectedHeuristicCost[3] = 1;

        auto heuristicCost = heuristicCostManager.getHeuristicCost();
        TS_ASSERT_EQUALS( heuristicCost.size(), expectedHeuristicCost.size() );
        for ( const auto &pair : heuristicCost )
            TS_ASSERT_EQUALS( pair.second, expectedHeuristicCost[pair.first] );
    }

    void test_update_heuristic_cost()
    {
        Options::get()->setString( Options::INITIALIZATION_STRATEGY, "currentAssignment" );
        Options::get()->setString( Options::FLIPPING_STRATEGY, "gwsat" );

        InputQuery inputQuery;
        Engine engine;
        HeuristicCostManager heuristicCostManager( &engine );
        MockSolver solver;
        initializeHeuristicCostManager( inputQuery, engine, heuristicCostManager, solver );

        solver.setValue( "x0", 0.5 );
        solver.setValue( "x1", 0.7 );
        solver.setValue( "x2", -1 );
        solver.setValue( "x3", 0.5 );
        solver.setValue( "x4", 1 );

        TS_ASSERT_THROWS_NOTHING( heuristicCostManager.initiateCostFunctionForLocalSearch(); );

        solver.setValue( "x0", -1 );

        TS_ASSERT_THROWS_NOTHING( heuristicCostManager.updateHeuristicCost() );
        heuristicCostManager.dumpHeuristicCost();

        Map<unsigned, double> expectedHeuristicCost;
        expectedHeuristicCost[0] = 0;
        expectedHeuristicCost[1] = 1;
        expectedHeuristicCost[3] = 1;

        auto heuristicCost = heuristicCostManager.getHeuristicCost();
        TS_ASSERT_EQUALS( heuristicCost.size(), expectedHeuristicCost.size() );
        for ( const auto &pair : heuristicCost )
            TS_ASSERT_EQUALS( pair.second, expectedHeuristicCost[pair.first] );

        solver.setValue( "x2", 1 );
        TS_ASSERT_THROWS_NOTHING( heuristicCostManager.updateHeuristicCost() );
        heuristicCostManager.dumpHeuristicCost();

        expectedHeuristicCost.clear();
        expectedHeuristicCost[0] = 0;
        expectedHeuristicCost[1] = 1;
        expectedHeuristicCost[2] = -1;
        expectedHeuristicCost[3] = 1;

        heuristicCost = heuristicCostManager.getHeuristicCost();
        TS_ASSERT_EQUALS( heuristicCost.size(), expectedHeuristicCost.size() );
        for ( const auto &pair : heuristicCost )
            TS_ASSERT_EQUALS( pair.second, expectedHeuristicCost[pair.first] );

        TS_ASSERT_THROWS_NOTHING( heuristicCostManager.undoLastHeuristicCostUpdate() );
        heuristicCostManager.dumpHeuristicCost();

        expectedHeuristicCost.clear();
        expectedHeuristicCost[0] = 0;
        expectedHeuristicCost[1] = 1;
        expectedHeuristicCost[2] = 0;
        expectedHeuristicCost[3] = 1;


        heuristicCost = heuristicCostManager.getHeuristicCost();
        TS_ASSERT_EQUALS( heuristicCost.size(), expectedHeuristicCost.size() );
        for ( const auto &pair : heuristicCost )
            TS_ASSERT_EQUALS( pair.second, expectedHeuristicCost[pair.first] );

    }
};
