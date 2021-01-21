/*********************                                                        */
/*! \file Test_LocalSearch.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include <cxxtest/TestSuite.h>

#include "Engine.h"
#include "PiecewiseLinearCaseSplit.h"
#include "PiecewiseLinearConstraint.h"
#include "ReluConstraint.h"

#include <string.h>

class LocalSearchTestSuite : public CxxTest::TestSuite
{
public:

    void setUp()
    {
    }

    void tearDown()
    {
    }

    void test_add_and_remove_cost_component()
    {
        ReluConstraint relu1( 0, 1 );
        ReluConstraint relu2( 2, 3 );
        ReluConstraint relu3( 3, 4 );

        double large = 100;
        relu1.notifyLowerBound( 0, -large );
        relu1.notifyUpperBound( 0, large );
        relu1.notifyLowerBound( 1, -large );
        relu1.notifyUpperBound( 1, large );
        relu2.notifyLowerBound( 2, -large );
        relu2.notifyUpperBound( 2, large );
        relu2.notifyLowerBound( 3, -large );
        relu2.notifyUpperBound( 3, large );
        relu3.notifyLowerBound( 3, -large );
        relu3.notifyUpperBound( 3, large );
        relu3.notifyLowerBound( 4, -large );
        relu3.notifyUpperBound( 4, large );

        relu1.notifyVariableValue( 0, -1 );
        relu1.notifyVariableValue( 1, 1 );
        relu2.notifyVariableValue( 2, 2 );
        relu2.notifyVariableValue( 3, 2 );
        relu3.notifyVariableValue( 3, 2 );
        relu3.notifyVariableValue( 4, 3 );

        Map<unsigned, double> cost;

        TS_ASSERT_THROWS_NOTHING( relu1.addCostFunctionComponent( cost, PhaseStatus::RELU_PHASE_ACTIVE ) );
        TS_ASSERT( cost.size() == 2 );
        TS_ASSERT( cost[0] == -1 );
        TS_ASSERT( cost[1] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu1.addCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 2 );
        TS_ASSERT( cost[0] == 0 );
        TS_ASSERT( cost[1] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu1.addCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 2 );
        TS_ASSERT( cost[0] == 0 );
        TS_ASSERT( cost[1] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu2.addCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 4 );
        TS_ASSERT( cost[0] == 0 );
        TS_ASSERT( cost[1] == 1 );
        TS_ASSERT( cost[2] == -1 );
        TS_ASSERT( cost[3] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu3.addCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 5 );
        TS_ASSERT( cost[0] == 0 );
        TS_ASSERT( cost[1] == 1 );
        TS_ASSERT( cost[2] == -1 );
        TS_ASSERT( cost[3] == 0 );
        TS_ASSERT( cost[4] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu2.removeCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 4 );
        TS_ASSERT( cost[0] == 0 );
        TS_ASSERT( cost[1] == 1 );
        TS_ASSERT( cost[3] == -1 );
        TS_ASSERT( cost[4] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu1.removeCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 2 );
        TS_ASSERT( cost[3] == -1 );
        TS_ASSERT( cost[4] == 1 );

        TS_ASSERT_THROWS_NOTHING( relu3.removeCostFunctionComponent( cost ) );
        TS_ASSERT( cost.size() == 0 );
    }

    void test_get_reduced_heuristic_cost()
    {
        ReluConstraint relu1( 0, 1 );

        double large = 100;
        relu1.notifyLowerBound( 0, -large );
        relu1.notifyUpperBound( 0, large );
        relu1.notifyLowerBound( 1, -large );
        relu1.notifyUpperBound( 1, large );

        relu1.notifyVariableValue( 0, -1 );
        relu1.notifyVariableValue( 1, 1 );

        Map<unsigned, double> cost;
        double reducedCost = 0;
        PhaseStatus phase = PhaseStatus::PHASE_NOT_FIXED;

        TS_ASSERT_THROWS_NOTHING( relu1.addCostFunctionComponent( cost, PhaseStatus::RELU_PHASE_INACTIVE ) );
        TS_ASSERT_THROWS_NOTHING( relu1.getReducedHeuristicCost( reducedCost, phase ) );
        TS_ASSERT( reducedCost == -1 );
        TS_ASSERT( phase == RELU_PHASE_ACTIVE );

        TS_ASSERT_THROWS_NOTHING( relu1.addCostFunctionComponent( cost, PhaseStatus::RELU_PHASE_ACTIVE ) );
        TS_ASSERT_THROWS_NOTHING( relu1.getReducedHeuristicCost( reducedCost, phase ) );
        TS_ASSERT( reducedCost == 1 );
        TS_ASSERT( phase == RELU_PHASE_INACTIVE );
    }
};

//
// Local Variables:
// compile-command: "make -C ../../.. "
// tags-file-name: "../../../TAGS"
// c-basic-offset: 4
// End:
//
