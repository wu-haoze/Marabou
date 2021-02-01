/**********************/
/*! \file Test_ReluConstraint.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze (Andrew) Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include <cxxtest/TestSuite.h>
#include "PiecewiseLinearConstraint.h"
#include "ReluConstraint.h"

class ReluConstraintTestSuite : public CxxTest::TestSuite
{
public:

    void setUp()
    {
    }

    void tearDown()
    {
    }

    void test_cdos()
    {
        PiecewiseLinearConstraint *relu = new ReluConstraint( 0, 1 );
        CVC4::context::Context context;
        TS_ASSERT_THROWS_NOTHING( relu->initializeCDOs( &context ) );
        TS_ASSERT( !relu->phaseFixed() );
        context.push();
        relu->notifyLowerBound( 0, 1 );
        TS_ASSERT( relu->phaseFixed() );
        context.pop();
        TS_ASSERT( !relu->phaseFixed() );
        context.push();
        TS_ASSERT( !relu->phaseFixed() );
        delete relu;
    }
};
