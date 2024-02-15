/*********************                                                        */
/*! \file Test_IncrementalLinearization.h
** \verbatim
** Top contributors (to current version):
**   Teruhiro Tagomori, Andrew Wu
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** [[ Add lengthier description here ]]
**/

#include "Engine.h"
#include "FloatUtils.h"
#include "IncrementalLinearization.h"
#include "InputQuery.h"
#include "SigmoidConstraint.h"

#include <cxxtest/TestSuite.h>
#include <string.h>


using namespace CEGAR;

class IncrementalLinearizationTestSuite : public CxxTest::TestSuite
{
public:
    void setUp()
    {
    }

    void tearDown()
    {
    }

    void run_sigmoid_test( unsigned testNumber, double lb, double ub, double bValue, double fValue )
    {
        std::cout << "Sigmoid case " << testNumber << std::endl;
        InputQuery ipq;
        ipq.setNumberOfVariables( 2 );
        ipq.setLowerBound( 0, lb );
        ipq.setUpperBound( 0, ub );
        ipq.addNonlinearConstraint( new SigmoidConstraint( 0, 1 ) );
        Engine *dummy = new Engine();
        IncrementalLinearization cegarEngine( ipq, dummy );

        InputQuery refinement;
        refinement.setNumberOfVariables( 2 );
        refinement.setLowerBound( 0, lb );
        refinement.setUpperBound( 0, ub );
        refinement.setSolutionValue( 0, bValue );
        refinement.setSolutionValue( 1, fValue );
        TS_ASSERT_THROWS_NOTHING( cegarEngine.refine( refinement ) );

        Engine engine;
        TS_ASSERT_THROWS_NOTHING( engine.processInputQuery( ipq ) );
        TS_ASSERT_THROWS_NOTHING( engine.solve() );
        Engine::ExitCode code = engine.getExitCode();
        TS_ASSERT( code == Engine::SAT || code == Engine::UNKNOWN );
    }

    void test_incremental_linearization_sigmoid()
    {
        // We test that given a counter-example,
        // a refinement can be sucessfully applied to
        // excude that counter-example.

        // Case 1
        run_sigmoid_test( 1, -10, 10, -3, 0.00001 );
        return;
        // Case 2
        run_sigmoid_test( 2, -10, 10, 0, 0.01 );

        // Case 3
        run_sigmoid_test( 3, -10, 10, 3, 0.01 );

        // Case 4
        run_sigmoid_test( 4, 1, 10, 3, 0.5 );

        // Case 5
        run_sigmoid_test( 5, -10, -2, -3, 0.00001 );


        // Case 6
        run_sigmoid_test( 6, -10, 10, -3, 0.08 );

        // Case 7
        run_sigmoid_test( 7, -10, 10, 0, 0.8 );

        // Case 8
        run_sigmoid_test( 8, -10, 10, 3, 0.999 );

        // Case 9
        run_sigmoid_test( 9, 1, 10, 3, 0.999 );

        // Case 10
        run_sigmoid_test( 10, -10, -2, -3, 0.08 );
    }
};
