/*********************                                                        */
/*! \file Test_PseudoCostTracker.h
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
#include "PseudoCostTracker.h"
#include "PiecewiseLinearConstraint.h"
#include "ReluConstraint.h"

class PseudoCostTrackerTestSuite : public CxxTest::TestSuite
{
public:

    PseudoCostTracker *_tracker;
    List<PiecewiseLinearConstraint *> _constraints;

    void setUp()
    {
        _tracker = new PseudoCostTracker();
    }

    void tearDown()
    {
        delete _tracker;
        for ( const auto &ele : _constraints )
            delete ele;
    }

    void test_updateScore()
    {
        CVC4::context::Context context;
        PiecewiseLinearConstraint *r1 = new ReluConstraint( 0, 1 );
        PiecewiseLinearConstraint *r2 = new ReluConstraint( 2, 3 );
        PiecewiseLinearConstraint *r3 = new ReluConstraint( 4, 5 );
        PiecewiseLinearConstraint *r4 = new ReluConstraint( 6, 7 );
        _constraints = {r1, r2, r3, r4};
        for ( const auto &constraint : _constraints )
            constraint->initializeCDOs( &context );

        TS_ASSERT_THROWS_NOTHING( _tracker->initialize( _constraints ) );
        TS_ASSERT_EQUALS( _tracker->_plConstraintToScore.size(), 4u );
        TS_ASSERT_EQUALS( _tracker->_scores.size(), 4u );
        TS_ASSERT_THROWS_NOTHING( _tracker->updateScore( r1, 2 ) );
        TS_ASSERT_THROWS_NOTHING( _tracker->updateScore( r2, 4 ) );
        TS_ASSERT_THROWS_NOTHING( _tracker->updateScore( r3, 5 ) );
        TS_ASSERT_THROWS_NOTHING( _tracker->updateScore( r3, 6 ) );
        TS_ASSERT_THROWS_NOTHING( _tracker->updateScore( r4, 3 ) );

        double alpha = GlobalConfiguration::EXPONENTIAL_MOVING_AVERAGE_ALPHA;
        TS_ASSERT_EQUALS( _tracker->_plConstraintToScore[r1], alpha * 2 );
        TS_ASSERT_EQUALS( _tracker->_plConstraintToScore[r3], (1 - alpha) * (alpha * 5) + alpha * 6 );

        r3->setActiveConstraint( false );
        r1->setActiveConstraint( false );
        TS_ASSERT( _tracker->top() == r3 );
        TS_ASSERT( _tracker->topUnfixed() == r2 );
        r3->setActiveConstraint( true );
        TS_ASSERT_THROWS_NOTHING( _tracker->updateScore( r3, 5 ) );
        TS_ASSERT( _tracker->topUnfixed() == r3 );
        r2->setActiveConstraint( false );
        TS_ASSERT( _tracker->topUnfixed() == r3 );
    }
};
