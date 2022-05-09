/*********************                                                        */
/*! \file Test_MILPEncoder.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Teruhiro Tagomori
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include <cxxtest/TestSuite.h>

#include "MILPEncoder.h"
#include "MarabouError.h"
#include "MockTableau.h"
#include "InputQuery.h"
#include "FloatUtils.h"

#include <string.h>

class MILPEncoderTestSuite : public CxxTest::TestSuite
{
public:

    void setUp()
    {
    }

    void tearDown()
    {
    }

    void test_eoncode_max_constraint()
    {
#ifdef ENABLE_GUROBI
        unsigned f;

        // elements = (x1, x2, x3)
        Set<unsigned> elements;
        for ( unsigned i = 1; i < 4; ++i )
            elements.insert( i );

        //
        // x1 = max(x1, x2, x3)
        // Bounds:
        //   100 <= x0 <= 100
        //   0 <= x1 <= 2
        //   1 <= x2 <= 2
        //   3 <= x3 <= 4
        //
        GurobiWrapper gurobi1;

        InputQuery inputQuery1 = InputQuery();
        inputQuery1.setNumberOfVariables( 4 );

        MockTableau tableau1 = MockTableau();
        tableau1.setDimensions( 2, 6 );

        // 100 <= x0 <= 100
        inputQuery1.setLowerBound( 0, 100 );
        inputQuery1.setUpperBound( 0, 100 );
        tableau1.setLowerBound( 0, 100 );
        tableau1.setUpperBound( 0, 100 );

        // 0 <= x1 <= 2
        inputQuery1.setLowerBound( 1, 0 );
        inputQuery1.setUpperBound( 1, 2 );
        tableau1.setLowerBound( 1, 0 );
        tableau1.setUpperBound( 1, 2 );

        // 1 <= x2 <= 2
        inputQuery1.setLowerBound( 2, 1 );
        inputQuery1.setUpperBound( 2, 2 );
        tableau1.setLowerBound( 2, 1 );
        tableau1.setUpperBound( 2, 2 );

        // 3 <= x3 <= 4
        inputQuery1.setLowerBound( 3, 3 );
        inputQuery1.setUpperBound( 3, 4 );
        tableau1.setLowerBound( 3, 3 );
        tableau1.setUpperBound( 3, 4 );

        // For auxiliary vars of max constraint
        tableau1.setLowerBound( 4, 0 );
        tableau1.setUpperBound( 4, FloatUtils::infinity() );
        tableau1.setLowerBound( 5, 0 );
        tableau1.setUpperBound( 5, FloatUtils::infinity() );

        f = 1;
        MaxConstraint *max1 = new MaxConstraint( f, elements );
        max1->transformToUseAuxVariables( inputQuery1 );
        inputQuery1.addPiecewiseLinearConstraint( max1 );
        MILPEncoder milp1( tableau1 );
        milp1.encodeInputQuery( gurobi1, inputQuery1 );
        gurobi1.solve();

        TS_ASSERT( !gurobi1.haveFeasibleSolution() );

        //
        // x2 = max(x1, x2, x3)
        // Bounds:
        //   100 <= x0 <= 100
        //   0 <= x1 <= 2
        //   1 <= x2 <= 2
        //   3 <= x3 <= 4
        //
        GurobiWrapper gurobi2;

        InputQuery inputQuery2 = InputQuery();
        inputQuery2.setNumberOfVariables( 4 );

        MockTableau tableau2 = MockTableau();
        tableau2.setDimensions( 2, 6 );

        // 100 <= x0 <= 100
        inputQuery2.setLowerBound( 0, 100 );
        inputQuery2.setUpperBound( 0, 100 );
        tableau2.setLowerBound( 0, 100 );
        tableau2.setUpperBound( 0, 100 );

        // 0 <= x1 <= 2
        inputQuery2.setLowerBound( 1, 0 );
        inputQuery2.setUpperBound( 1, 2 );
        tableau2.setLowerBound( 1, 0 );
        tableau2.setUpperBound( 1, 2 );

        // 1 <= x2 <= 2
        inputQuery2.setLowerBound( 2, 1 );
        inputQuery2.setUpperBound( 2, 2 );
        tableau2.setLowerBound( 2, 1 );
        tableau2.setUpperBound( 2, 2 );

        // 3 <= x3 <= 4
        inputQuery2.setLowerBound( 3, 3 );
        inputQuery2.setUpperBound( 3, 4 );
        tableau2.setLowerBound( 3, 3 );
        tableau2.setUpperBound( 3, 4 );

        // For auxiliary vars of max constraint
        tableau2.setLowerBound( 4, 0 );
        tableau2.setUpperBound( 4, FloatUtils::infinity() );
        tableau2.setLowerBound( 5, 0 );
        tableau2.setUpperBound( 5, FloatUtils::infinity() );
        tableau2.setLowerBound( 6, 0 );
        tableau2.setUpperBound( 6, FloatUtils::infinity() );

        f = 2;
        MaxConstraint *max2 = new MaxConstraint( f, elements );
        max2->transformToUseAuxVariables( inputQuery2 );
        inputQuery2.addPiecewiseLinearConstraint( max2 );
        MILPEncoder milp2( tableau2 );
        milp2.encodeInputQuery( gurobi2, inputQuery2 );
        gurobi2.solve();

        TS_ASSERT( !gurobi2.haveFeasibleSolution() );

        //
        // x3 = max(x1, x2, x3)
        // Bounds:
        //   100 <= x0 <= 100
        //   0 <= x1 <= 2
        //   1 <= x2 <= 2
        //   3 <= x3 <= 4
        //
        GurobiWrapper gurobi3;

        InputQuery inputQuery3 = InputQuery();
        inputQuery3.setNumberOfVariables( 4 );

        MockTableau tableau3 = MockTableau();
        tableau3.setDimensions( 2, 6 );

        // 100 <= x0 <= 100
        inputQuery3.setLowerBound( 0, 100 );
        inputQuery3.setUpperBound( 0, 100 );
        tableau3.setLowerBound( 0, 100 );
        tableau3.setUpperBound( 0, 100 );

        // 0 <= x1 <= 2
        inputQuery3.setLowerBound( 1, 0 );
        inputQuery3.setUpperBound( 1, 2 );
        tableau3.setLowerBound( 1, 0 );
        tableau3.setUpperBound( 1, 2 );

        // 1 <= x2 <= 2
        inputQuery3.setLowerBound( 2, 1 );
        inputQuery3.setUpperBound( 2, 2 );
        tableau3.setLowerBound( 2, 1 );
        tableau3.setUpperBound( 2, 2 );

        // 3 <= x3 <= 4
        inputQuery3.setLowerBound( 3, 3 );
        inputQuery3.setUpperBound( 3, 4 );
        tableau3.setLowerBound( 3, 3 );
        tableau3.setUpperBound( 3, 4 );

        // For auxiliary vars of max constraint
        tableau3.setLowerBound( 4, 0 );
        tableau3.setUpperBound( 4, FloatUtils::infinity() );
        tableau3.setLowerBound( 5, 0 );
        tableau3.setUpperBound( 5, FloatUtils::infinity() );
        tableau3.setLowerBound( 6, 0 );
        tableau3.setUpperBound( 6, FloatUtils::infinity() );

        f = 3;
        MaxConstraint *max3 = new MaxConstraint( f, elements );
        max3->transformToUseAuxVariables( inputQuery3 );
        inputQuery3.addPiecewiseLinearConstraint( max3 );
        MILPEncoder milp3( tableau3 );
        milp3.encodeInputQuery( gurobi3, inputQuery3 );
        gurobi3.solve();

        TS_ASSERT( gurobi3.haveFeasibleSolution() );

        Map<String, double> values3;
        double costOrObjective3;
        
        gurobi3.extractSolution(values3, costOrObjective3 );
        
        double x0_sol3 = values3["x0"];
        double x1_sol3 = values3["x1"];
        double x2_sol3 = values3["x2"];
        double x3_sol3 = values3["x3"];

        TS_ASSERT_LESS_THAN_EQUALS( x1_sol3, x3_sol3 );
        TS_ASSERT_LESS_THAN_EQUALS( x2_sol3, x3_sol3 );
        TS_ASSERT_LESS_THAN_EQUALS( x3_sol3, x0_sol3 );
        TS_ASSERT_EQUALS( x0_sol3, 100.0 );

        //
        // x0 = max(x1, x2, x3)
        // Bounds:
        //   4 <= x0 <= 4
        //   0 <= x1 <= 2
        //   1 <= x2 <= 2
        //   3 <= x3 <= 4
        //
        GurobiWrapper gurobi4;

        InputQuery inputQuery4 = InputQuery();
        inputQuery4.setNumberOfVariables( 4 );

        MockTableau tableau4 = MockTableau();
        tableau4.setDimensions( 2, 6 );

        // 4 <= x0 <= 4
        inputQuery4.setLowerBound( 0, 4 );
        inputQuery4.setUpperBound( 0, 4 );
        tableau4.setLowerBound( 0, 4 );
        tableau4.setUpperBound( 0, 4 );

        // 0 <= x1 <= 2
        inputQuery4.setLowerBound( 1, 0 );
        inputQuery4.setUpperBound( 1, 2 );
        tableau4.setLowerBound( 1, 0 );
        tableau4.setUpperBound( 1, 2 );

        // 1 <= x2 <= 2
        inputQuery4.setLowerBound( 2, 1 );
        inputQuery4.setUpperBound( 2, 2 );
        tableau4.setLowerBound( 2, 1 );
        tableau4.setUpperBound( 2, 2 );

        // 3 <= x3 <= 4
        inputQuery4.setLowerBound( 3, 3 );
        inputQuery4.setUpperBound( 3, 4 );
        tableau4.setLowerBound( 3, 3 );
        tableau4.setUpperBound( 3, 4 );

        // For auxiliary vars of max constraint
        tableau4.setLowerBound( 4, 0 );
        tableau4.setUpperBound( 4, FloatUtils::infinity() );
        tableau4.setLowerBound( 5, 0 );
        tableau4.setUpperBound( 5, FloatUtils::infinity() );
        tableau4.setLowerBound( 6, 0 );
        tableau4.setUpperBound( 6, FloatUtils::infinity() );

        f = 0;
        MaxConstraint *max4 = new MaxConstraint( f, elements );
        max4->transformToUseAuxVariables( inputQuery4 );
        inputQuery4.addPiecewiseLinearConstraint( max4 );
        MILPEncoder milp4( tableau4 );
        milp4.encodeInputQuery( gurobi4, inputQuery4 );
        gurobi4.solve();

        TS_ASSERT( gurobi4.haveFeasibleSolution() );

        Map<String, double> values4;
        double costOrObjective4;
        
        gurobi4.extractSolution(values4, costOrObjective4 );
        
        double x0_sol4 = values4["x0"];
        double x1_sol4 = values4["x1"];
        double x2_sol4 = values4["x2"];
        double x3_sol4 = values4["x3"];

        TS_ASSERT_LESS_THAN_EQUALS( x1_sol4, x0_sol4 );
        TS_ASSERT_LESS_THAN_EQUALS( x2_sol4, x0_sol4 );
        TS_ASSERT_LESS_THAN_EQUALS( x3_sol4, x0_sol4 );
        TS_ASSERT_EQUALS( x0_sol4, 4.0 );

        //
        // x0 = max(x1, x2, x3)
        // Bounds:
        //   0 <= x0 <= 0
        //   0 <= x1 <= 2
        //   1 <= x2 <= 2
        //   3 <= x3 <= 4
        //
        GurobiWrapper gurobi5;

        InputQuery inputQuery5 = InputQuery();
        inputQuery5.setNumberOfVariables( 4 );

        MockTableau tableau5 = MockTableau();
        tableau5.setDimensions( 2, 6 );

        // 0 <= x0 <= 0
        inputQuery5.setLowerBound( 0, 0 );
        inputQuery5.setUpperBound( 0, 0 );
        tableau5.setLowerBound( 0, 0 );
        tableau5.setUpperBound( 0, 0 );

        // 0 <= x1 <= 2
        inputQuery5.setLowerBound( 1, 0 );
        inputQuery5.setUpperBound( 1, 2 );
        tableau5.setLowerBound( 1, 0 );
        tableau5.setUpperBound( 1, 2 );

        // 1 <= x2 <= 2
        inputQuery5.setLowerBound( 2, 1 );
        inputQuery5.setUpperBound( 2, 2 );
        tableau5.setLowerBound( 2, 1 );
        tableau5.setUpperBound( 2, 2 );

        // 3 <= x3 <= 4
        inputQuery5.setLowerBound( 3, 3 );
        inputQuery5.setUpperBound( 3, 4 );
        tableau5.setLowerBound( 3, 3 );
        tableau5.setUpperBound( 3, 4 );

        // For auxiliary vars of max constraint
        tableau5.setLowerBound( 4, 0 );
        tableau5.setUpperBound( 4, FloatUtils::infinity() );
        tableau5.setLowerBound( 5, 0 );
        tableau5.setUpperBound( 5, FloatUtils::infinity() );
        tableau5.setLowerBound( 6, 0 );
        tableau5.setUpperBound( 6, FloatUtils::infinity() );

        f = 0;
        MaxConstraint *max5 = new MaxConstraint( f, elements );
        max5->transformToUseAuxVariables( inputQuery5 );
        inputQuery5.addPiecewiseLinearConstraint( max5 );
        MILPEncoder milp5( tableau5 );
        milp5.encodeInputQuery( gurobi5, inputQuery5 );
        gurobi5.solve();

        TS_ASSERT( !gurobi5.haveFeasibleSolution() );

#else
        TS_ASSERT( true );
#endif // ENABLE_GUROBI
	}

    void test_eoncode_sigmoid_constraint_sat()
    {
#ifdef ENABLE_GUROBI

        /*
         * x0_lb >= 0
        */
        GurobiWrapper gurobi1;

        InputQuery inputQuery1 = InputQuery();
        inputQuery1.setNumberOfVariables( 2 );

        MockTableau tableau1 = MockTableau();

        // 0 <= x0 <= 1
        inputQuery1.setLowerBound( 0, 0 );
        inputQuery1.setUpperBound( 0, 1 );
        tableau1.setLowerBound( 0, 0 );
        tableau1.setUpperBound( 0, 1 );

        // x1 = sigmoid( x0 )
        SigmoidConstraint *sigmoid1 = new SigmoidConstraint( 0, 1 );
        inputQuery1.addTranscendentalConstraint( sigmoid1 );
        inputQuery1.setLowerBound( 1, sigmoid1->sigmoid( 0 ) );
        inputQuery1.setUpperBound( 1, sigmoid1->sigmoid( 1 ) );
        tableau1.setLowerBound( 1, sigmoid1->sigmoid( 0 ) );
        tableau1.setUpperBound( 1, sigmoid1->sigmoid( 1 ) );

        sigmoid1->notifyLowerBound( 0, 0 );
        sigmoid1->notifyUpperBound( 0, 1 );
        sigmoid1->notifyLowerBound( 1, sigmoid1->sigmoid( 0 ) );
        sigmoid1->notifyUpperBound( 1, sigmoid1->sigmoid( 1 ) );

        MILPEncoder milp1( tableau1 );
        TS_ASSERT_THROWS_NOTHING( milp1.encodeInputQuery( gurobi1, inputQuery1 ) );

        // binVarName should not be set.
        TS_ASSERT_THROWS_NOTHING( gurobi1.solve() );

        TS_ASSERT( gurobi1.haveFeasibleSolution() );
    
        Map<String, double> solution1;
        double costValue1;

        TS_ASSERT_THROWS_NOTHING( gurobi1.extractSolution( solution1, costValue1 ) );

        TS_ASSERT( solution1.exists( "x0" ) );
        TS_ASSERT( solution1.exists( "x1" ) );
        TS_ASSERT( !solution1.exists( "a0" ) );

        /*
         * x0_ub < 0
        */
        GurobiWrapper gurobi2;

        InputQuery inputQuery2 = InputQuery();
        inputQuery2.setNumberOfVariables( 2 );

        MockTableau tableau2 = MockTableau();
        tableau2.setDimensions( 2, 2 );

        // -1 <= x0 < 0
        inputQuery2.setLowerBound( 0, -1 );
        inputQuery2.setUpperBound( 0, -0.1 );
        tableau2.setLowerBound( 0, -1 );
        tableau2.setUpperBound( 0, -0.1 );

        // x1 = sigmoid( x0 )
        SigmoidConstraint *sigmoid2 = new SigmoidConstraint( 0, 1 );
        inputQuery2.addTranscendentalConstraint( sigmoid2 );
        inputQuery2.setLowerBound( 1, sigmoid2->sigmoid( -1 ) );
        inputQuery2.setUpperBound( 1, sigmoid2->sigmoid( -0.1 )  );
        tableau2.setLowerBound( 1, sigmoid2->sigmoid( -1 )  );
        tableau2.setUpperBound( 1, sigmoid2->sigmoid( -0.1 )  );

        sigmoid2->notifyLowerBound( 0, -1 );
        sigmoid2->notifyUpperBound( 0, -0.1 );
        sigmoid2->notifyLowerBound( 1, sigmoid1->sigmoid( -1 ) );
        sigmoid2->notifyUpperBound( 1, sigmoid1->sigmoid( -0.1 ) );

        MILPEncoder milp2( tableau2 );
        milp2.encodeInputQuery( gurobi2, inputQuery2 );


        TS_ASSERT_THROWS_NOTHING( gurobi2.solve() );

        TS_ASSERT( gurobi2.haveFeasibleSolution() );
        Map<String, double> solution2;
        double costValue2;

        TS_ASSERT_THROWS_NOTHING( gurobi2.extractSolution( solution2, costValue2 ) );
#endif // ENABLE_GUROBI
    }

    void test_encode_sigmoid_with_cut_point()
    {
        GurobiWrapper gurobi1;

        InputQuery inputQuery1 = InputQuery();
        inputQuery1.setNumberOfVariables( 2 );

        MockTableau tableau1 = MockTableau();

        // -1 <= x0 <= 1
        inputQuery1.setLowerBound( 0, -1 );
        inputQuery1.setUpperBound( 0, 1 );
        tableau1.setLowerBound( 0, -1 );
        tableau1.setUpperBound( 0, 1 );

        // x1 = sigmoid( x0 )
        SigmoidConstraint *sigmoid1 = new SigmoidConstraint( 0, 1 );
        inputQuery1.addTranscendentalConstraint( sigmoid1 );
        inputQuery1.setLowerBound( 1, sigmoid1->sigmoid( -1 ) );
        inputQuery1.setUpperBound( 1, sigmoid1->sigmoid( 1 ) );
        tableau1.setLowerBound( 1, sigmoid1->sigmoid( -1 ) );
        tableau1.setUpperBound( 1, sigmoid1->sigmoid( 1 ) );

        sigmoid1->notifyLowerBound( 0, -1 );
        sigmoid1->notifyUpperBound( 0, 1 );
        sigmoid1->notifyLowerBound( 1, sigmoid1->sigmoid( -1 ) );
        sigmoid1->notifyUpperBound( 1, sigmoid1->sigmoid( 1 ) );

        // Above and below
        sigmoid1->addCutPoint( 0, true );
        sigmoid1->addCutPoint( 0, false );
        sigmoid1->addCutPoint( 1, false );
        sigmoid1->addCutPoint( -1, true );
        sigmoid1->addCutPoint( 0.1, true );
        sigmoid1->addCutPoint( 0.1, false );
        sigmoid1->addCutPoint( -0.1, true );
        sigmoid1->addCutPoint( -0.1, false );

        MILPEncoder milp1( tableau1 );
        TS_ASSERT_THROWS_NOTHING( milp1.encodeInputQuery( gurobi1, inputQuery1 ) );

        // binVarName should not be set.
        TS_ASSERT_THROWS_NOTHING( gurobi1.solve() );

        TS_ASSERT( gurobi1.haveFeasibleSolution() );

        Map<String, double> solution1;
        double costValue1;

        TS_ASSERT_THROWS_NOTHING( gurobi1.extractSolution( solution1, costValue1 ) );

        TS_ASSERT( solution1.exists( "x0" ) );
        TS_ASSERT( solution1.exists( "x1" ) );
        TS_ASSERT( solution1.exists( "a0" ) );
        TS_ASSERT( solution1.exists( "a1" ) );
    }
};

//
// Local Variables:
// compile-command: "make -C ../../.. "
// tags-file-name: "../../../TAGS"
// c-basic-offset: 4
// End:
//
