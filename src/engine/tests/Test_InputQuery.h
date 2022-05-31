/*********************                                                        */
/*! \file Test_InputQuery.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Christopher Lazarus, Shantanu Thakoor
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
#include "FloatUtils.h"
#include "InputQuery.h"
#include "MockErrno.h"
#include "ReluConstraint.h"
#include "SoftmaxConstraint.h"
#include "MarabouError.h"

#include <string.h>

class MockForInputQuery
    : public MockErrno
{
public:
};

class InputQueryTestSuite : public CxxTest::TestSuite
{
public:
    MockForInputQuery *mock;

    void setUp()
    {
        TS_ASSERT( mock = new MockForInputQuery );
    }

    void tearDown()
    {
        TS_ASSERT_THROWS_NOTHING( delete mock );
    }

    void test_lower_bounds()
    {
        InputQuery inputQuery;

        TS_ASSERT_THROWS_NOTHING( inputQuery.setNumberOfVariables( 5 ) );
        TS_ASSERT_EQUALS( inputQuery.getLowerBound( 3 ), FloatUtils::negativeInfinity() );
        TS_ASSERT_THROWS_NOTHING( inputQuery.setLowerBound( 3, -3 ) );
        TS_ASSERT_EQUALS( inputQuery.getLowerBound( 3 ), -3 );
        TS_ASSERT_THROWS_NOTHING( inputQuery.setLowerBound( 3, 5 ) );
        TS_ASSERT_EQUALS( inputQuery.getLowerBound( 3 ), 5 );

        TS_ASSERT_EQUALS( inputQuery.getLowerBound( 2 ), FloatUtils::negativeInfinity() );
        TS_ASSERT_THROWS_NOTHING( inputQuery.setLowerBound( 2, 4 ) );
        TS_ASSERT_EQUALS( inputQuery.getLowerBound( 2 ), 4 );

        TS_ASSERT_THROWS_EQUALS( inputQuery.getLowerBound( 5 ),
                                 const MarabouError &e,
                                 e.getCode(),
                                 MarabouError::VARIABLE_INDEX_OUT_OF_RANGE );

        TS_ASSERT_THROWS_EQUALS( inputQuery.setLowerBound( 6, 1 ),
                                 const MarabouError &e,
                                 e.getCode(),
                                 MarabouError::VARIABLE_INDEX_OUT_OF_RANGE );
    }

    void test_upper_bounds()
    {
        InputQuery inputQuery;

        TS_ASSERT_THROWS_NOTHING( inputQuery.setNumberOfVariables( 5 ) );
        TS_ASSERT_EQUALS( inputQuery.getUpperBound( 2 ), FloatUtils::infinity() );
        TS_ASSERT_THROWS_NOTHING( inputQuery.setUpperBound( 2, -4 ) );
        TS_ASSERT_EQUALS( inputQuery.getUpperBound( 2 ), -4 );
        TS_ASSERT_THROWS_NOTHING( inputQuery.setUpperBound( 2, 55 ) );
        TS_ASSERT_EQUALS( inputQuery.getUpperBound( 2 ), 55 );

        TS_ASSERT_EQUALS( inputQuery.getUpperBound( 0 ), FloatUtils::infinity() );
        TS_ASSERT_THROWS_NOTHING( inputQuery.setUpperBound( 0, 1 ) );
        TS_ASSERT_EQUALS( inputQuery.getUpperBound( 0 ), 1 );

        TS_ASSERT_THROWS_EQUALS( inputQuery.getUpperBound( 5 ),
                                 const MarabouError &e,
                                 e.getCode(),
                                 MarabouError::VARIABLE_INDEX_OUT_OF_RANGE );

        TS_ASSERT_THROWS_EQUALS( inputQuery.setUpperBound( 6, 1 ),
                                 const MarabouError &e,
                                 e.getCode(),
                                 MarabouError::VARIABLE_INDEX_OUT_OF_RANGE );
    }

    void test_equality_operator()
    {
        ReluConstraint *relu1 = new ReluConstraint( 3, 5 );

        InputQuery *inputQuery = new InputQuery;

        inputQuery->setNumberOfVariables( 5 );
        inputQuery->setLowerBound( 2, -4 );
        inputQuery->setUpperBound( 2, 55 );
        inputQuery->addPiecewiseLinearConstraint( relu1 );

        InputQuery inputQuery2 = *inputQuery;

        TS_ASSERT_EQUALS( inputQuery2.getNumberOfVariables(), 5U );
        TS_ASSERT_EQUALS( inputQuery2.getLowerBound( 2 ), -4 );
        TS_ASSERT_EQUALS( inputQuery2.getUpperBound( 2 ), 55 );

        auto constraints = inputQuery2.getPiecewiseLinearConstraints();

        TS_ASSERT_EQUALS( constraints.size(), 1U );
        ReluConstraint *constraint = (ReluConstraint *)*constraints.begin();

        TS_ASSERT_DIFFERS( constraint, relu1 ); // Different pointers

        TS_ASSERT( constraint->participatingVariable( 3 ) );
        TS_ASSERT( constraint->participatingVariable( 5 ) );

        inputQuery2 = *inputQuery; // Repeat the assignment

        delete inputQuery;

        TS_ASSERT_EQUALS( inputQuery2.getNumberOfVariables(), 5U );
        TS_ASSERT_EQUALS( inputQuery2.getLowerBound( 2 ), -4 );
        TS_ASSERT_EQUALS( inputQuery2.getUpperBound( 2 ), 55 );

        constraints = inputQuery2.getPiecewiseLinearConstraints();

        TS_ASSERT_EQUALS( constraints.size(), 1U );
        constraint = (ReluConstraint *)*constraints.begin();

        TS_ASSERT_DIFFERS( constraint, relu1 ); // Different pointers

        TS_ASSERT( constraint->participatingVariable( 3 ) );
        TS_ASSERT( constraint->participatingVariable( 5 ) );
    }

    void test_equality_operator_with_sigmoid()
    {
        SigmoidConstraint *sigmoid1 = new SigmoidConstraint( 3, 5 );
        sigmoid1->notifyLowerBound( 3, 0.1 );
        sigmoid1->notifyLowerBound( 5, 0.2 );
        sigmoid1->notifyUpperBound( 3, 0.3 );
        sigmoid1->notifyUpperBound( 5, 0.4 );

        InputQuery *inputQuery = new InputQuery;

        inputQuery->setNumberOfVariables( 6 );
        inputQuery->setLowerBound( 3, 0.1 );
        inputQuery->setLowerBound( 5, 0.2 );
        inputQuery->setUpperBound( 3, 0.3 );
        inputQuery->setUpperBound( 5, 0.4 );
        inputQuery->addTranscendentalConstraint( sigmoid1 );

        InputQuery inputQuery2 = *inputQuery;

        TS_ASSERT_EQUALS( inputQuery2.getNumberOfVariables(), 6U );
        TS_ASSERT_EQUALS( inputQuery2.getLowerBound( 3 ), 0.1 );
        TS_ASSERT_EQUALS( inputQuery2.getLowerBound( 5 ), 0.2 );
        TS_ASSERT_EQUALS( inputQuery2.getUpperBound( 3 ), 0.3 );
        TS_ASSERT_EQUALS( inputQuery2.getUpperBound( 5 ), 0.4 );

        auto constraints = inputQuery2.getTranscendentalConstraints();

        TS_ASSERT_EQUALS( constraints.size(), 1U );
        SigmoidConstraint *constraint = (SigmoidConstraint *)*constraints.begin();

        TS_ASSERT_DIFFERS( constraint, sigmoid1 ); // Different pointers

        TS_ASSERT( constraint->participatingVariable( 3 ) );
        TS_ASSERT( constraint->participatingVariable( 5 ) );

        inputQuery2 = *inputQuery; // Repeat the assignment

        delete inputQuery;

        TS_ASSERT_EQUALS( inputQuery2.getNumberOfVariables(), 6U );
        TS_ASSERT_EQUALS( inputQuery2.getLowerBound( 3 ), 0.1 );
        TS_ASSERT_EQUALS( inputQuery2.getLowerBound( 5 ), 0.2 );
        TS_ASSERT_EQUALS( inputQuery2.getUpperBound( 3 ), 0.3 );
        TS_ASSERT_EQUALS( inputQuery2.getUpperBound( 5 ), 0.4 );

        constraints = inputQuery2.getTranscendentalConstraints();

        TS_ASSERT_EQUALS( constraints.size(), 1U );
        constraint = (SigmoidConstraint *)*constraints.begin();

        TS_ASSERT_DIFFERS( constraint, sigmoid1 ); // Different pointers

        TS_ASSERT( constraint->participatingVariable( 3 ) );
        TS_ASSERT( constraint->participatingVariable( 5 ) );
    }

    void test_infinite_bounds()
    {
        InputQuery *inputQuery = new InputQuery;

        inputQuery->setNumberOfVariables( 5 );
        inputQuery->setLowerBound( 2, -4 );
        inputQuery->setUpperBound( 2, 55 );
        inputQuery->setUpperBound( 3, FloatUtils::infinity() );

        TS_ASSERT_EQUALS( inputQuery->countInfiniteBounds(), 8U );

        delete inputQuery;
    }

    void test_save_query()
    {
    }

    void test_save_smt()
    {
        InputQuery inputQuery;
        inputQuery.setNumberOfVariables( 18 );
        for ( unsigned i = 0; i <= 2; ++i )
        {
            inputQuery.setLowerBound( i, -1 );
            inputQuery.setUpperBound( i, 1 );
        }
        for ( unsigned i = 6; i <= 8; ++i )
        {
            inputQuery.setLowerBound( i, -1 );
            inputQuery.setUpperBound( i, 1 );
        }

        SoftmaxConstraint *s1 = new SoftmaxConstraint( {0, 1, 2}, {3, 4, 5} );
        SoftmaxConstraint *s2 = new SoftmaxConstraint( {6, 7, 8}, {9, 10, 11} );
        // x12 = x3 * x9 + x4 * x10
        // x13 = x3 + x4 + x5
        // x14 = x9 + x10 + x11
        QuadraticEquation q1;
        q1.addAddend( 1, 12 );
        q1.addQuadraticAddend( -1, 3, 9 );
        q1.addQuadraticAddend( -1, 4, 10 );
        Equation e1;
        e1.addAddend( 1, 13 );
        e1.addAddend( -1, 3 );
        e1.addAddend( -1, 4 );
        e1.addAddend( -1, 5 );
        Equation e2;
        e2.addAddend( 1, 14 );
        e2.addAddend( -1, 9 );
        e2.addAddend( -1, 10 );
        e2.addAddend( -1, 11 );
        inputQuery.addTranscendentalConstraint( s1 );
        inputQuery.addTranscendentalConstraint( s2 );
        inputQuery.addQuadraticEquation( q1 );
        inputQuery.addEquation( e1 );
        inputQuery.addEquation( e2 );
        inputQuery.addPiecewiseLinearConstraint( new ReluConstraint( 12, 15 ) );
        inputQuery.addPiecewiseLinearConstraint( new ReluConstraint( 13, 16 ) );
        inputQuery.addPiecewiseLinearConstraint( new ReluConstraint( 14, 17 ) );

        //TS_ASSERT_THROWS_NOTHING( inputQuery.dumpSmtLibFile( "query.smt2" ) );
    }
};
