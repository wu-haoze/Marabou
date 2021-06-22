/*********************                                                        */
/*! \file Test_Engine.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Shantanu Thakoor, Derek Huang
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
#include "InputQuery.h"
#include "MockConstraintBoundTightenerFactory.h"
#include "MockConstraintMatrixAnalyzerFactory.h"
#include "MockCostFunctionManagerFactory.h"
#include "MockErrno.h"
#include "MockProjectedSteepestEdgeFactory.h"
#include "MockRowBoundTightenerFactory.h"
#include "MockTableauFactory.h"
#include "Options.h"
#include "PiecewiseLinearCaseSplit.h"
#include "ReluConstraint.h"

#include <string.h>

class MockForEngine :
    public MockTableauFactory,
    public MockProjectedSteepestEdgeRuleFactory,
    public MockRowBoundTightenerFactory,
    public MockConstraintBoundTightenerFactory,
    public MockCostFunctionManagerFactory,
    public MockConstraintMatrixAnalyzerFactory
{
public:
};

class EngineTestSuite : public CxxTest::TestSuite
{
public:
    MockForEngine *mock;
    MockTableau *tableau;
    MockCostFunctionManager *costFunctionManager;
    MockRowBoundTightener *rowTightener;
    MockConstraintBoundTightener *constraintTightener;
    MockConstraintMatrixAnalyzer *constraintMatrixAnalyzer;

    void setUp()
    {
        TS_ASSERT( mock = new MockForEngine );

        tableau = &( mock->mockTableau );
        costFunctionManager = &( mock->mockCostFunctionManager );
        rowTightener = &( mock->mockRowBoundTightener );
        constraintTightener = &( mock->mockConstraintBoundTightener );
        constraintMatrixAnalyzer = &( mock->mockConstraintMatrixAnalyzer );
    }

    void tearDown()
    {
        TS_ASSERT_THROWS_NOTHING( delete mock );
    }

    void test_constructor_destructor()
    {
        Engine *engine = NULL;

        TS_ASSERT_THROWS_NOTHING( engine = new Engine() );

        TS_ASSERT( tableau->wasCreated );
        TS_ASSERT( costFunctionManager->wasCreated );

        TS_ASSERT_THROWS_NOTHING( delete engine );

        TS_ASSERT( tableau->wasDiscarded );
        TS_ASSERT( costFunctionManager->wasDiscarded );
    }

    void test_pick_split_pl_constraint_polarity()
    {
        // x0 --> relu(x2,x4) --> relu(x6,x8)
        //    x               x
        // x1 --> relu(x3,x5) --> relu(x7,x9)

        // x0 \in [-1, 1]
        // x1 \in [-1, 2]
        // x2 = 2 x0 - 3 x1    ( x2 \in [-8, 5] )
        // x3 = 2 x0 - x1    ( x2 \in [-4, 3] )
        // x4 \in [0, 5]
        // x5 \in [0, 3]
        // x6 = -x4 + x5  ( x6 \in [-2, 8] )
        // x7 = -x4 + x5 - 3 ( x7 \in [-5, 5] )
        // Based on the polarity, relu(x7, x9) should be picked
        // And the picked ReLU must have |polarity| not larger than the other unfixed ReLUs

        InputQuery inputQuery;
        inputQuery.setNumberOfVariables( 10 );
        inputQuery.markInputVariable( 0, 0 );
        inputQuery.markInputVariable( 1, 1 );

        Equation equation;
        equation.addAddend( 2, 0 );
        equation.addAddend( -3, 1 );
        equation.addAddend( -1, 2 );
        equation.setScalar( 0 );
        inputQuery.addEquation( equation );

        Equation equation1;
        equation1.addAddend( 2, 0 );
        equation1.addAddend( -1, 1 );
        equation1.addAddend( -1, 3 );
        equation1.setScalar( 0 );
        inputQuery.addEquation( equation1 );

        Equation equation3;
        equation3.addAddend( -1, 4 );
        equation3.addAddend( 1, 5 );
        equation3.addAddend( -1, 6 );
        equation3.setScalar( 0 );
        inputQuery.addEquation( equation3 );

        Equation equation4;
        equation4.addAddend( -1, 4 );
        equation4.addAddend( 1, 5 );
        equation4.addAddend( -1, 7 );
        equation4.setScalar( 0 );
        inputQuery.addEquation( equation4 );

        ReluConstraint *relu1 = new ReluConstraint( 2, 4 );
        ReluConstraint *relu2 = new ReluConstraint( 3, 5 );
        ReluConstraint *relu3 = new ReluConstraint( 6, 8 );
        ReluConstraint *relu4 = new ReluConstraint( 7, 9 );

        inputQuery.addPiecewiseLinearConstraint( relu1 );
        inputQuery.addPiecewiseLinearConstraint( relu2 );
        inputQuery.addPiecewiseLinearConstraint( relu3 );
        inputQuery.addPiecewiseLinearConstraint( relu4 );

        inputQuery.setLowerBound( 0, -1 );
        inputQuery.setUpperBound( 0, 1 );
        inputQuery.setLowerBound( 1, -1 );
        inputQuery.setUpperBound( 1, 2 );
        inputQuery.setLowerBound( 2, -8 );
        inputQuery.setUpperBound( 2, 5 );
        inputQuery.setLowerBound( 3, -4 );
        inputQuery.setUpperBound( 3, 3 );
        inputQuery.setLowerBound( 4, 0 );
        inputQuery.setUpperBound( 4, 5 );
        inputQuery.setLowerBound( 5, 0 );
        inputQuery.setUpperBound( 5, 3 );
        inputQuery.setLowerBound( 6, -2 );
        inputQuery.setUpperBound( 6, 8 );
        inputQuery.setLowerBound( 7, -5 );
        inputQuery.setUpperBound( 7, 5 );
        inputQuery.setLowerBound( 8, 0 );
        inputQuery.setUpperBound( 8, 8 );
        inputQuery.setLowerBound( 9, 0 );
        inputQuery.setUpperBound( 9, 5 );

        Options::get()->setString( Options::SPLITTING_STRATEGY, "polarity" );
        Engine engine;
        TS_ASSERT( inputQuery.constructNetworkLevelReasoner() );
        engine.processInputQuery( inputQuery, false );
        PiecewiseLinearConstraint *constraintToSplit;
        PiecewiseLinearConstraint *constraintToSplitSnC;
        constraintToSplit = engine.pickSplitPLConstraint();
        constraintToSplitSnC = engine.pickSplitPLConstraintSnC( SnCDivideStrategy::Polarity );
        TS_ASSERT_EQUALS( constraintToSplitSnC, constraintToSplit );

        TS_ASSERT( constraintToSplit );
        TS_ASSERT( !constraintToSplit->phaseFixed() );
        TS_ASSERT( constraintToSplit->isActive() );
        for ( const auto &constraint : engine.getInputQuery()->
                  getNetworkLevelReasoner()->getConstraintsInTopologicalOrder() )
        {
            TS_ASSERT( std::abs( ( ( ReluConstraint * ) constraintToSplit )->computePolarity() ) <=
                       std::abs( ( ( ReluConstraint * ) constraint )->computePolarity() ) );
        }

        TS_ASSERT_EQUALS( 0, ( ( ReluConstraint * ) constraintToSplit )->computePolarity() );
    }

    void test_pick_split_pl_constraint_earliest_relu()
    {
        // x0 --> relu(x2,x4) --> relu(x6,x8)
        //    x               x
        // x1 --> relu(x3,x5) --> relu(x7,x9)

        // x0 \in [-1, 1]
        // x1 \in [-1, 2]
        // x2 = 2 x0 - 3 x1    ( x2 \in [-8, 5] )
        // x3 = 2 x0 - x1    ( x2 \in [-4, 3] )
        // x4 \in [0, 5]
        // x5 \in [0, 3]
        // x6 = -x4 + x5  ( x6 \in [-2, 8] )
        // x7 = -x4 + x5 - 3 ( x7 \in [-5, 5] )

        InputQuery inputQuery;
        inputQuery.setNumberOfVariables( 10 );
        inputQuery.markInputVariable( 0, 0 );
        inputQuery.markInputVariable( 1, 1 );

        Equation equation;
        equation.addAddend( 2, 0 );
        equation.addAddend( -3, 1 );
        equation.addAddend( -1, 2 );
        equation.setScalar( 0 );
        inputQuery.addEquation( equation );

        Equation equation1;
        equation1.addAddend( 2, 0 );
        equation1.addAddend( -1, 1 );
        equation1.addAddend( -1, 3 );
        equation1.setScalar( 0 );
        inputQuery.addEquation( equation1 );

        Equation equation3;
        equation3.addAddend( -1, 4 );
        equation3.addAddend( 1, 5 );
        equation3.addAddend( -1, 6 );
        equation3.setScalar( 0 );
        inputQuery.addEquation( equation3 );

        Equation equation4;
        equation4.addAddend( -1, 4 );
        equation4.addAddend( 1, 5 );
        equation4.addAddend( -1, 7 );
        equation4.setScalar( 0 );
        inputQuery.addEquation( equation4 );

        ReluConstraint *relu1 = new ReluConstraint( 2, 4 );
        ReluConstraint *relu2 = new ReluConstraint( 3, 5 );
        ReluConstraint *relu3 = new ReluConstraint( 6, 8 );
        ReluConstraint *relu4 = new ReluConstraint( 7, 9 );

        inputQuery.addPiecewiseLinearConstraint( relu1 );
        inputQuery.addPiecewiseLinearConstraint( relu2 );
        inputQuery.addPiecewiseLinearConstraint( relu3 );
        inputQuery.addPiecewiseLinearConstraint( relu4 );

        inputQuery.setLowerBound( 0, -1 );
        inputQuery.setUpperBound( 0, 1 );
        inputQuery.setLowerBound( 1, -1 );
        inputQuery.setUpperBound( 1, 2 );
        inputQuery.setLowerBound( 2, -8 );
        inputQuery.setUpperBound( 2, 5 );
        inputQuery.setLowerBound( 3, -4 );
        inputQuery.setUpperBound( 3, 3 );
        inputQuery.setLowerBound( 4, 0 );
        inputQuery.setUpperBound( 4, 5 );
        inputQuery.setLowerBound( 5, 0 );
        inputQuery.setUpperBound( 5, 3 );
        inputQuery.setLowerBound( 6, -2 );
        inputQuery.setUpperBound( 6, 8 );
        inputQuery.setLowerBound( 7, -5 );
        inputQuery.setUpperBound( 7, 5 );
        inputQuery.setLowerBound( 8, 0 );
        inputQuery.setUpperBound( 8, 8 );
        inputQuery.setLowerBound( 9, 0 );
        inputQuery.setUpperBound( 9, 5 );

        Options::get()->setString( Options::SPLITTING_STRATEGY, "earliest-relu" );
        Engine engine;
        TS_ASSERT( inputQuery.constructNetworkLevelReasoner() );
        engine.processInputQuery( inputQuery, false );
        PiecewiseLinearConstraint *constraintToSplit;
        PiecewiseLinearConstraint *constraintToSplitSnC;
        constraintToSplit = engine.pickSplitPLConstraint();
        constraintToSplitSnC = engine.pickSplitPLConstraintSnC( SnCDivideStrategy::EarliestReLU );
        TS_ASSERT_EQUALS( constraintToSplitSnC, constraintToSplit );

        PiecewiseLinearConstraint *firstConstraintInTopologicalOrder;
        firstConstraintInTopologicalOrder = *(engine.getInputQuery()->
                                              getNetworkLevelReasoner()->
                                              getConstraintsInTopologicalOrder().begin());

        TS_ASSERT( firstConstraintInTopologicalOrder );
        TS_ASSERT( !firstConstraintInTopologicalOrder->phaseFixed() );
        TS_ASSERT( firstConstraintInTopologicalOrder->isActive() );
        TS_ASSERT_EQUALS( firstConstraintInTopologicalOrder, constraintToSplit );
    }

    void test_pick_split_pl_constraint_largest_interval()
    {
        // x0 --> relu(x2,x4) --> relu(x6,x8)
        //    x               x
        // x1 --> relu(x3,x5) --> relu(x7,x9)

        // x0 \in [-1, 1]
        // x1 \in [-1, 2]
        // x2 = 2 x0 - 3 x1    ( x2 \in [-8, 5] )
        // x3 = 2 x0 - x1    ( x2 \in [-4, 3] )
        // x4 \in [0, 5]
        // x5 \in [0, 3]
        // x6 = -x4 + x5  ( x6 \in [-2, 8] )
        // x7 = -x4 + x5 - 3 ( x7 \in [-5, 5] )

        InputQuery inputQuery;
        inputQuery.setNumberOfVariables( 10 );
        inputQuery.markInputVariable( 0, 0 );
        inputQuery.markInputVariable( 1, 1 );

        Equation equation;
        equation.addAddend( 2, 0 );
        equation.addAddend( -3, 1 );
        equation.addAddend( -1, 2 );
        equation.setScalar( 0 );
        inputQuery.addEquation( equation );

        Equation equation1;
        equation1.addAddend( 2, 0 );
        equation1.addAddend( -1, 1 );
        equation1.addAddend( -1, 3 );
        equation1.setScalar( 0 );
        inputQuery.addEquation( equation1 );

        Equation equation3;
        equation3.addAddend( -1, 4 );
        equation3.addAddend( 1, 5 );
        equation3.addAddend( -1, 6 );
        equation3.setScalar( 0 );
        inputQuery.addEquation( equation3 );

        Equation equation4;
        equation4.addAddend( -1, 4 );
        equation4.addAddend( 1, 5 );
        equation4.addAddend( -1, 7 );
        equation4.setScalar( 0 );
        inputQuery.addEquation( equation4 );

        ReluConstraint *relu1 = new ReluConstraint( 2, 4 );
        ReluConstraint *relu2 = new ReluConstraint( 3, 5 );
        ReluConstraint *relu3 = new ReluConstraint( 6, 8 );
        ReluConstraint *relu4 = new ReluConstraint( 7, 9 );

        inputQuery.addPiecewiseLinearConstraint( relu1 );
        inputQuery.addPiecewiseLinearConstraint( relu2 );
        inputQuery.addPiecewiseLinearConstraint( relu3 );
        inputQuery.addPiecewiseLinearConstraint( relu4 );

        inputQuery.setLowerBound( 0, -1 );
        inputQuery.setUpperBound( 0, 1 );
        inputQuery.setLowerBound( 1, -1 );
        inputQuery.setUpperBound( 1, 2 );
        inputQuery.setLowerBound( 2, -8 );
        inputQuery.setUpperBound( 2, 5 );
        inputQuery.setLowerBound( 3, -4 );
        inputQuery.setUpperBound( 3, 3 );
        inputQuery.setLowerBound( 4, 0 );
        inputQuery.setUpperBound( 4, 5 );
        inputQuery.setLowerBound( 5, 0 );
        inputQuery.setUpperBound( 5, 3 );
        inputQuery.setLowerBound( 6, -2 );
        inputQuery.setUpperBound( 6, 8 );
        inputQuery.setLowerBound( 7, -5 );
        inputQuery.setUpperBound( 7, 5 );
        inputQuery.setLowerBound( 8, 0 );
        inputQuery.setUpperBound( 8, 8 );
        inputQuery.setLowerBound( 9, 0 );
        inputQuery.setUpperBound( 9, 5 );

        Options::get()->setString( Options::SPLITTING_STRATEGY,
                                   "largest-interval" );
        Engine engine;
        TS_ASSERT( inputQuery.constructNetworkLevelReasoner() );
        engine.processInputQuery( inputQuery, false );
        PiecewiseLinearConstraint *constraintToSplit;
        constraintToSplit = engine.pickSplitPLConstraint();

        PiecewiseLinearCaseSplit interval1;
        interval1.storeBoundTightening( Tightening( 1, 0.5, Tightening::UB ) );

        PiecewiseLinearCaseSplit interval2;
        interval2.storeBoundTightening( Tightening( 1, 0.5, Tightening::LB ) );

        List<PiecewiseLinearCaseSplit> caseSplits = constraintToSplit->getCaseSplits();

        TS_ASSERT_EQUALS( caseSplits.size(), 2u );
        TS_ASSERT_EQUALS( *caseSplits.begin(), interval1 );
        TS_ASSERT_EQUALS( *( ++caseSplits.begin() ), interval2 );
    }

    void test_todo()
    {
        TS_TRACE( "Future work: Guarantee correct behavior even when some variable is unbounded\n" );
    }
};

//
// Local Variables:
// compile-command: "make -C ../../.. "
// tags-file-name: "../../../TAGS"
// c-basic-offset: 4
// End:
//
