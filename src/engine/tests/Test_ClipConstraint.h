/*********************                                                        */
/*! \file Test_ClipConstraint.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include <cxxtest/TestSuite.h>

#include "ClipConstraint.h"
#include "InputQuery.h"
#include "MarabouError.h"
#include "MockErrno.h"
#include "MockTableau.h"
#include "PiecewiseLinearCaseSplit.h"
#include <iostream>
#include <string.h>

class MockForClipConstraint : public MockErrno
{
public:
};

/*
   Exposes protected members of ClipConstraint for testing.
 */
class TestClipConstraint : public ClipConstraint
{
public:
    TestClipConstraint( unsigned b, unsigned f )
        : ClipConstraint( b, f, 1, 5 )
    {}

    using ClipConstraint::getPhaseStatus;
};

using namespace CVC4::context;

class ClipConstraintTestSuite : public CxxTest::TestSuite
{
public:
    MockForClipConstraint *mock;

    void setUp()
    {
        TS_ASSERT( mock = new MockForClipConstraint );
    }

    void tearDown()
    {
        TS_ASSERT_THROWS_NOTHING( delete mock );
    }

    void test_clip_duplicate_and_restore()
    {
        TestClipConstraint clip1( 4, 6 );
        MockTableau tableau;
        clip1.registerTableau( &tableau );

        clip1.setActiveConstraint( false );
        tableau.setValue( 4, 1.0 );
        tableau.setValue( 6, 1.0 );

        clip1.notifyLowerBound( 4, -8.0 );
        clip1.notifyUpperBound( 4, 8.0 );

        clip1.notifyLowerBound( 6, 0.0 );
        clip1.notifyUpperBound( 6, 4.0 );

        PiecewiseLinearConstraint *clip2 = clip1.duplicateConstraint();

        TS_ASSERT( clip1.satisfied() );
        TS_ASSERT( !clip1.isActive() );
        TS_ASSERT( !clip2->isActive() );
        TS_ASSERT( clip2->satisfied() );

        clip1.setActiveConstraint( true );
        clip2->restoreState( &clip1 );
        TS_ASSERT( clip2->isActive() );
        TS_ASSERT( clip2->satisfied() );

        clip1.setActiveConstraint( false );
        TS_ASSERT( !clip1.isActive() );
        TS_ASSERT( clip2->isActive() );

        TS_ASSERT_THROWS_NOTHING( delete clip2 );
    }

    void test_register_and_unregister_as_watcher()
    {
        unsigned b = 1;
        unsigned f = 4;

        MockTableau tableau;

        TestClipConstraint clip( b, f );

        TS_ASSERT_THROWS_NOTHING( clip.registerAsWatcher( &tableau ) );

        TS_ASSERT_EQUALS( tableau.lastRegisteredVariableToWatcher.size(), 2U );
        TS_ASSERT( tableau.lastUnregisteredVariableToWatcher.empty() );
        TS_ASSERT_EQUALS( tableau.lastRegisteredVariableToWatcher[b].size(), 1U );
        TS_ASSERT( tableau.lastRegisteredVariableToWatcher[b].exists( &clip ) );
        TS_ASSERT_EQUALS( tableau.lastRegisteredVariableToWatcher[f].size(), 1U );
        TS_ASSERT( tableau.lastRegisteredVariableToWatcher[f].exists( &clip ) );

        tableau.lastRegisteredVariableToWatcher.clear();

        TS_ASSERT_THROWS_NOTHING( clip.unregisterAsWatcher( &tableau ) );

        TS_ASSERT( tableau.lastRegisteredVariableToWatcher.empty() );
        TS_ASSERT_EQUALS( tableau.lastUnregisteredVariableToWatcher.size(), 2U );
        TS_ASSERT_EQUALS( tableau.lastUnregisteredVariableToWatcher[b].size(), 1U );
        TS_ASSERT( tableau.lastUnregisteredVariableToWatcher[b].exists( &clip ) );
        TS_ASSERT_EQUALS( tableau.lastUnregisteredVariableToWatcher[f].size(), 1U );
        TS_ASSERT( tableau.lastUnregisteredVariableToWatcher[f].exists( &clip ) );
    }

    void test_participating_variables()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );

        List<unsigned> participatingVariables;

        TS_ASSERT_THROWS_NOTHING( participatingVariables = clip.getParticipatingVariables() );
        TS_ASSERT_EQUALS( participatingVariables.size(), 2U );
        auto it = participatingVariables.begin();
        TS_ASSERT_EQUALS( *it, b );
        ++it;
        TS_ASSERT_EQUALS( *it, f );

        TS_ASSERT_EQUALS( clip.getParticipatingVariables(), List<unsigned>( { 1, 4 } ) );

        TS_ASSERT( clip.participatingVariable( b ) );
        TS_ASSERT( clip.participatingVariable( f ) );
        TS_ASSERT( !clip.participatingVariable( 2 ) );
        TS_ASSERT( !clip.participatingVariable( 0 ) );
        TS_ASSERT( !clip.participatingVariable( 5 ) );
    }

    void test_clip_notify_variable_value_and_satisfied()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );
        MockTableau tableau;
        clip.registerTableau( &tableau );

        tableau.setValue( b, 5 );
        tableau.setValue( f, 5 );
        TS_ASSERT( clip.satisfied() );

        tableau.setValue( b, 8 );
        tableau.setValue( f, 5 );
        TS_ASSERT( clip.satisfied() );

        tableau.setValue( b, 3 );
        tableau.setValue( f, 5 );
        TS_ASSERT( !clip.satisfied() );

        tableau.setValue( b, -1 );
        tableau.setValue( f, -1 );
        TS_ASSERT( !clip.satisfied() );

        tableau.setValue( b, -1 );
        tableau.setValue( f, 1 );
        TS_ASSERT( clip.satisfied() );

        tableau.setValue( b, 1 );
        tableau.setValue( f, 1 );
        TS_ASSERT( clip.satisfied() );

        tableau.setValue( b, 5 );
        tableau.setValue( f, 5 );
        TS_ASSERT( clip.satisfied() );
    }

    void test_clip_update_variable_index()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );
        MockTableau tableau;
        clip.registerTableau( &tableau );

        // Changing variable indices
        tableau.setValue( b, 1 );
        tableau.setValue( f, 1 );
        TS_ASSERT( clip.satisfied() );

        unsigned newB = 12;
        unsigned newF = 14;

        TS_ASSERT_THROWS_NOTHING( clip.updateVariableIndex( b, newB ) );
        TS_ASSERT_THROWS_NOTHING( clip.updateVariableIndex( f, newF ) );

        tableau.setValue( newB, 1 );
        tableau.setValue( newF, 1 );

        TS_ASSERT( clip.satisfied() );

        tableau.setValue( newF, 2 );

        TS_ASSERT( !clip.satisfied() );

        tableau.setValue( newB, 2 );

        TS_ASSERT( clip.satisfied() );
    }

    void test_eliminate_variable_b()
    {
        unsigned b = 1;
        unsigned f = 4;

        MockTableau tableau;

        TestClipConstraint clip( b, f );

        clip.registerAsWatcher( &tableau );

        TS_ASSERT( !clip.constraintObsolete() );
        TS_ASSERT_THROWS_NOTHING( clip.eliminateVariable( b, 5 ) );
        TS_ASSERT( clip.constraintObsolete() );
    }

    void test_eliminate_variable_f()
    {
        unsigned b = 1;
        unsigned f = 4;

        MockTableau tableau;

        TestClipConstraint clip( b, f );

        clip.registerAsWatcher( &tableau );

        TS_ASSERT( !clip.constraintObsolete() );
        TS_ASSERT_THROWS_NOTHING( clip.eliminateVariable( f, 5 ) );
        TS_ASSERT( clip.constraintObsolete() );
    }

    /*
    void test_clip_entailed_tightenings_positive_phase_1()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;

            clip.notifyLowerBound( b, 1 );
            clip.notifyLowerBound( f, 2 );
            clip.notifyUpperBound( b, 7 );
            clip.notifyUpperBound( f, 7 );

            // 1 < x_b < 7 , 2 < x_f < 7 -> 2 < x_b
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 1, 2, 7, 7, entailedTightenings );

            clip.notifyLowerBound( b, 3 );
            // 3 < x_b < 7 , 2 < x_f < 7
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 2, 7, 7, entailedTightenings );

            clip.notifyLowerBound( f, 3 );
            clip.notifyUpperBound( b, 6 );
            // 3 < x_b < 6 , 3 < x_f < 7
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 3, 6, 7, entailedTightenings );

            clip.notifyUpperBound( f, 6 );
            clip.notifyUpperBound( b, 7 );
            // 3 < x_b < 6 , 3 < x_f < 6
            //  --> x_b < 6
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 3, 6, 6, entailedTightenings );

            clip.notifyLowerBound( f, -3 );
            // 3 < x_b < 6 , 3 < x_f < 6
            // --> 3 < x_f
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 3, 6, 6, entailedTightenings );

            clip.notifyLowerBound( b, 5 );
            clip.notifyUpperBound( f, 5 );
            // 5 < x_b < 6 , 3 < x_f < 5
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 5, 3, 6, 5, entailedTightenings );
        }

        {   // With BoundManager
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );
            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;

            clip.notifyLowerBound( b, 1 );
            clip.notifyLowerBound( f, 2 );
            clip.notifyUpperBound( b, 7 );
            clip.notifyUpperBound( f, 7 );

            // 1 < x_b < 7 , 2 < x_f < 7 -> 2 < x_b
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 1, 2, 7, 7, entailedTightenings );

            clip.notifyLowerBound( b, 3 );
            // 3 < x_b < 7 , 2 < x_f < 7
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 2, 7, 7, entailedTightenings );

            clip.notifyLowerBound( f, 3 );
            clip.notifyUpperBound( b, 6 );
            // 3 < x_b < 6 , 3 < x_f < 7
            // --> x_f < 6
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 3, 6, 6, entailedTightenings );

            clip.notifyUpperBound( f, 6 );
            clip.notifyUpperBound( b, 7 );
            // 3 < x_b < 6 , 3 < x_f < 6
            //  --> x_b < 6
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 3, 6, 6, entailedTightenings );

            clip.notifyLowerBound( f, -3 );
            // 3 < x_b < 6 , 3 < x_f < 6
            // --> 3 < x_f
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 3, 6, 6, entailedTightenings );

            clip.notifyLowerBound( b, 5 );
            clip.notifyUpperBound( f, 5 );
            // 5 < x_b < 6 , 3 < x_f < 5
            // --> x_b < 5
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 5, 3, 5, 5, entailedTightenings );
        }
    }

    void test_clip_entailed_tightenings_positive_phase_2()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;

            // 8 < b < 18, 48 < f < 64
            clip.notifyUpperBound( b, 18 );
            clip.notifyUpperBound( f, 64 );
            clip.notifyLowerBound( b, 8 );
            clip.notifyLowerBound( f, 48 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 8, 48, 18, 64, entailedTightenings );
        }

        {   //With BoundManage
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );

            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;

            // 8 < b < 18, 48 < f < 64
            clip.notifyUpperBound( b, 18 );
            clip.notifyUpperBound( f, 64 );
            clip.notifyLowerBound( b, 8 );
            clip.notifyLowerBound( f, 48 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 8, 48, 18, 64, entailedTightenings );
        }
    }

    void test_clip_entailed_tightenings_positive_phase_3()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;

            // 3 < b < 4, 1 < f < 2
            clip.notifyUpperBound( b, 4 );
            clip.notifyUpperBound( f, 2 );
            clip.notifyLowerBound( b, 3 );
            clip.notifyLowerBound( f, 1 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 1, 4, 2, entailedTightenings );
        }

        {   // With Bound Manager
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );

            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;

            // 3 < b < 4, 1 < f < 2
            // --> b < 2
            clip.notifyUpperBound( b, 4 );
            clip.notifyUpperBound( f, 2 );
            clip.notifyLowerBound( b, 3 );
            clip.notifyLowerBound( f, 1 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 3, 1, 2, 2, entailedTightenings );
        }
    }

    void test_clip_entailed_tightenings_positive_phase_4()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyUpperBound( b, 7 );
            clip.notifyUpperBound( f, 6 );
            clip.notifyLowerBound( b, 0 );
            clip.notifyLowerBound( f, 0 );

            // 0 < x_b < 7 ,0 < x_f < 6
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 0, 0, 7, 6, entailedTightenings );

            clip.notifyUpperBound( b, 5 );
            // 0 < x_b < 5 ,0 < x_f < 6
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 0, 0, 5, 6, entailedTightenings );

            clip.notifyLowerBound( b, 1 );
            // 1 < x_b < 5 ,0 < x_f < 6
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 1, 0, 5, 6, entailedTightenings );

            clip.notifyUpperBound( f, 4 );
            // 1 < x_b < 5 ,0 < x_f < 4
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 1, 0, 5, 4, entailedTightenings );

            // Non overlap
            clip.notifyUpperBound( f, 2 );
            clip.notifyLowerBound( b, 3 );

            // 3 < x_b < 5 ,0 < x_f < 2
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            TS_ASSERT_EQUALS( entailedTightenings.size(), 4U );
            assert_lower_upper_bound( f, b, 3, 0, 5, 2, entailedTightenings );
        }

        {   // With Bound Manager
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );
            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyUpperBound( b, 7 );
            clip.notifyUpperBound( f, 6 );
            clip.notifyLowerBound( b, 0 );
            clip.notifyLowerBound( f, 0 );

            // 0 < x_b < 7 ,0 < x_f < 6
            // --> x_b < 6
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 0, 0, 6, 6, entailedTightenings );

            clip.notifyUpperBound( b, 5 );
            // 0 < x_b < 5 ,0 < x_f < 6
            // --> x_f < 5
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 0, 0, 5, 5, entailedTightenings );

            clip.notifyLowerBound( b, 1 );
            // 1 < x_b < 5 ,0 < x_f < 5
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 1, 0, 5, 5, entailedTightenings );

            clip.notifyUpperBound( f, 4 );
            // 1 < x_b < 5 ,0 < x_f < 4
            // --> x_b < 4
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 1, 0, 4, 4, entailedTightenings );

            // Non overlap
            clip.notifyUpperBound( f, 2 );
            clip.notifyLowerBound( b, 3 );

            // 3 < x_b < 5 ,0 < x_f < 2
            // --> x_b < 2 ( i.e. bounds not valid )
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            TS_ASSERT_EQUALS( entailedTightenings.size(), 4U );
            assert_lower_upper_bound( f, b, 3, 0, 2, 2, entailedTightenings );
        }
    }

    void test_clip_entailed_tightenings_positive_phase_5()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;

            clip.notifyUpperBound( b, 6 );
            clip.notifyUpperBound( f, 5 );
            clip.notifyLowerBound( b, 4 );
            clip.notifyLowerBound( f, 3 );

            // 4 < x_b < 6 ,3 < x_f < 5
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 4, 3, 6, 5, entailedTightenings );
        }

        {   // With Bound Manager
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );
            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;

            clip.notifyUpperBound( b, 6 );
            clip.notifyUpperBound( f, 5 );
            clip.notifyLowerBound( b, 4 );
            clip.notifyLowerBound( f, 3 );

            // 4 < x_b < 6 ,3 < x_f < 5
            // --> x_b < 5
            entailedTightenings.clear();
            clip.getEntailedTightenings( entailedTightenings );
            assert_lower_upper_bound( f, b, 4, 3, 5, 5, entailedTightenings );
        }
    }

    void test_clip_entailed_tightenings_positive_phase_6()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, 5 );
            clip.notifyUpperBound( b, 10 );
            clip.notifyLowerBound( f, -1 );
            clip.notifyUpperBound( f, 10 );

            TS_ASSERT( clip.phaseFixed() );
            clip.getEntailedTightenings( entailedTightenings );

            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( f, 0, Tightening::LB ),

                                          Tightening( b, 0, Tightening::LB ),
                                          Tightening( b, 10, Tightening::UB ),
                                          Tightening( f, 5, Tightening::LB ),
                                          Tightening( f, 10, Tightening::UB ),
                                      } ) );
        }

        {   // With Bound Manager
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );
            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, 5 );
            clip.notifyUpperBound( b, 10 );
            clip.notifyLowerBound( f, -1 );
            clip.notifyUpperBound( f, 10 );

            TS_ASSERT( clip.phaseFixed() );
            clip.getEntailedTightenings( entailedTightenings );

            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, 0, Tightening::LB ),
                                          Tightening( b, 10, Tightening::UB ),
                                          Tightening( f, 5, Tightening::LB ),
                                          Tightening( f, 10, Tightening::UB ),
                                      } ) );
        }
    }

    void test_clip_entailed_tightenings_phase_not_fixed_f_strictly_positive()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );
        List<Tightening> entailedTightenings;
        List<Tightening>::iterator it;

        clip.notifyLowerBound( b, -6 );
        clip.notifyUpperBound( b, 3 );
        clip.notifyLowerBound( f, 2 );
        clip.notifyUpperBound( f, 4 );

        // -6 < x_b < 3 ,2 < x_f < 4
        clip.getEntailedTightenings( entailedTightenings );

        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -4, Tightening::LB ),
                                      Tightening( b, 4, Tightening::UB ),
                                      Tightening( f, 6, Tightening::UB ),
                                  } ) );

        entailedTightenings.clear();

        // -6 < x_b < 2 ,2 < x_f < 4
        clip.notifyUpperBound( b, 2 );
        clip.getEntailedTightenings( entailedTightenings );

        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -4, Tightening::LB ),
                                      Tightening( b, 4, Tightening::UB ),
                                      Tightening( f, 6, Tightening::UB ),
                                  } ) );

        entailedTightenings.clear();

        // -6 < x_b < 1 ,2 < x_f < 4, now stuck in negative phase
        clip.notifyUpperBound( b, 1 );
        clip.getEntailedTightenings( entailedTightenings );

        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -4, Tightening::LB ),
                                      Tightening( b, 4, Tightening::UB ),
                                      Tightening( f, 6, Tightening::UB ),
                                      Tightening( b, -2, Tightening::UB ),
                                  } ) );
    }

    void test_clip_entailed_tightenings_phase_not_fixed_f_strictly_positive_2()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );
        List<Tightening> entailedTightenings;
        List<Tightening>::iterator it;

        clip.notifyLowerBound( b, -5 );
        clip.notifyUpperBound( b, 10 );
        clip.notifyLowerBound( f, 3 );
        clip.notifyUpperBound( f, 7 );

        // -5 < x_b < 10 ,3 < x_f < 7
        clip.getEntailedTightenings( entailedTightenings );
        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -7, Tightening::LB ),
                                      Tightening( b, 7, Tightening::UB ),
                                      Tightening( f, 10, Tightening::UB ),
                                  } ) );

        entailedTightenings.clear();

        // -5 < x_b < 10 ,6 < x_f < 7, positive phase
        clip.notifyLowerBound( f, 6 );

        clip.getEntailedTightenings( entailedTightenings );
        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -7, Tightening::LB ),
                                      Tightening( b, 7, Tightening::UB ),
                                      Tightening( f, 10, Tightening::UB ),
                                      Tightening( b, 6, Tightening::LB ),
                                  } ) );

        entailedTightenings.clear();

        // -5 < x_b < 3 ,6 < x_f < 7

        // Extreme case, disjoint ranges

        clip.notifyUpperBound( b, 3 );

        clip.getEntailedTightenings( entailedTightenings );
        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -7, Tightening::LB ),
                                      Tightening( b, 7, Tightening::UB ),
                                      Tightening( f, 5, Tightening::UB ),
                                      Tightening( b, 6, Tightening::LB ),
                                      Tightening( b, -6, Tightening::UB ),
                                  } ) );
    }

    void test_clip_entailed_tightenings_phase_not_fixed_f_strictly_positive_3()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );
        List<Tightening> entailedTightenings;
        List<Tightening>::iterator it;

        clip.notifyLowerBound( b, -1 );
        clip.notifyUpperBound( b, 1 );
        clip.notifyLowerBound( f, 2 );
        clip.notifyUpperBound( f, 4 );

        // -1 < x_b < 1 ,2 < x_f < 4
        clip.getEntailedTightenings( entailedTightenings );
        assert_tightenings_match( entailedTightenings,
                                  List<Tightening>( {
                                      Tightening( b, -4, Tightening::LB ),
                                      Tightening( b, 4, Tightening::UB ),
                                      Tightening( f, 1, Tightening::UB ),
                                      Tightening( b, 2, Tightening::LB ),
                                      Tightening( b, -2, Tightening::UB ),
                                  } ) );
    }

    void test_clip_entailed_tightenings_phase_not_fixed_f_non_negative()
    {
      {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, -7 );
            clip.notifyUpperBound( b, 7 );
            clip.notifyLowerBound( f, 0 );
            clip.notifyUpperBound( f, 6 );

            // -7 < x_b < 7 ,0 < x_f < 6
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -6, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 7, Tightening::UB ),
                                      } ) );


            entailedTightenings.clear();

            // -7 < x_b < 5 ,0 < x_f < 6
            clip.notifyUpperBound( b, 5 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -6, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 7, Tightening::UB ),
                                      } ) );

            entailedTightenings.clear();
            // 0 < x_b < 5 ,0 < x_f < 6
            clip.notifyLowerBound( b, 0 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, 0, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 0, Tightening::LB ),
                                          Tightening( f, 5, Tightening::UB ),
                                      } ) );


            entailedTightenings.clear();

            // 3 < x_b < 5 ,0 < x_f < 6
            clip.notifyLowerBound( b, 3 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, 0, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 3, Tightening::LB ),
                                          Tightening( f, 5, Tightening::UB ),
                                      } ) );
        }
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, -7 );
            clip.notifyUpperBound( b, 7 );
            clip.notifyLowerBound( f, 0 );
            clip.notifyUpperBound( f, 6 );

            // -7 < x_b < 7 ,0 < x_f < 6
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -6, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 7, Tightening::UB ),
                                      } ) );


            entailedTightenings.clear();

            // -7 < x_b < 5 ,0 < x_f < 6
            clip.notifyUpperBound( b, 5 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -6, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 7, Tightening::UB ),
                                      } ) );

            entailedTightenings.clear();
            // 0 < x_b < 5 ,0 < x_f < 6
            clip.notifyLowerBound( b, 0 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, 0, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 0, Tightening::LB ),
                                          Tightening( f, 5, Tightening::UB ),
                                      } ) );


            entailedTightenings.clear();

            // 3 < x_b < 5 ,0 < x_f < 6
            clip.notifyLowerBound( b, 3 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, 0, Tightening::LB ),
                                          Tightening( b, 6, Tightening::UB ),
                                          Tightening( f, 3, Tightening::LB ),
                                          Tightening( f, 5, Tightening::UB ),
                                      } ) );
        }
    }

    void test_clip_entailed_tightenings_negative_phase()
    {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, -20 );
            clip.notifyUpperBound( b, -2 );
            clip.notifyLowerBound( f, 0 );
            clip.notifyUpperBound( f, 15 );

            // -20 < x_b < -2 ,0 < x_f < 15
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -15, Tightening::LB ),
                                          Tightening( b, 0, Tightening::UB ),
                                          Tightening( f, 2, Tightening::LB ),
                                          Tightening( f, 20, Tightening::UB ),
                                      } ) );


            entailedTightenings.clear();

            // -20 < x_b < -2 ,7 < x_f < 15
            clip.notifyLowerBound( f, 7 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -15, Tightening::LB ),
                                          Tightening( b, -7, Tightening::UB ),
                                          Tightening( f, 2, Tightening::LB ),
                                          Tightening( f, 20, Tightening::UB ),
                                      } ) );

            entailedTightenings.clear();

            // -12 < x_b < -2 ,7 < x_f < 15
            clip.notifyLowerBound( b, -12 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -15, Tightening::LB ),
                                          Tightening( b, -7, Tightening::UB ),
                                          Tightening( f, 2, Tightening::LB ),
                                          Tightening( f, 12, Tightening::UB ),
                                      } ) );

            entailedTightenings.clear();

            // -12 < x_b < -8 ,7 < x_f < 15
            clip.notifyUpperBound( b, -8 );
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -15, Tightening::LB ),
                                          Tightening( b, -7, Tightening::UB ),
                                          Tightening( f, 8, Tightening::LB ),
                                          Tightening( f, 12, Tightening::UB ),
                                      } ) );
    }

    void test_clip_entailed_tightenings_negative_phase_2()
    {
        {
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, -20 );
            clip.notifyUpperBound( b, -2 );
            clip.notifyLowerBound( f, 25 );
            clip.notifyUpperBound( f, 30 );

            // -20 < x_b < -2 ,25 < x_f < 30
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -30, Tightening::LB ),
                                          Tightening( b, -25, Tightening::UB ),
                                          Tightening( f, 2, Tightening::LB ),
                                          Tightening( f, 20, Tightening::UB ),
                                      } ) );
        }

        {   // With Bound Manager
            unsigned b = 1;
            unsigned f = 4;

            TestClipConstraint clip( b, f );
            Context context;
            BoundManager boundManager( context );
            boundManager.initialize( 5 );
            clip.registerBoundManager( &boundManager );

            List<Tightening> entailedTightenings;
            List<Tightening>::iterator it;

            clip.notifyLowerBound( b, -20 );
            clip.notifyUpperBound( b, -2 );
            clip.notifyLowerBound( f, 25 );
            clip.notifyUpperBound( f, 30 );

            // -20 < x_b < -2 ,25 < x_f < 30
            clip.getEntailedTightenings( entailedTightenings );
            assert_tightenings_match( entailedTightenings,
                                      List<Tightening>( {
                                          Tightening( b, -30, Tightening::LB ),
                                          Tightening( b, -25, Tightening::UB ),
                                          Tightening( f, 2, Tightening::LB ),
                                          Tightening( f, 20, Tightening::UB ),
                                      } ) );
        }
    }

    void test_clip_case_splits()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );

        List<PiecewiseLinearConstraint::Fix> fixes;
        List<PiecewiseLinearConstraint::Fix>::iterator it;

        List<PiecewiseLinearCaseSplit> splits = clip.getCaseSplits();

        Equation positiveEquation, negativeEquation;

        TS_ASSERT_EQUALS( splits.size(), 2U );

        List<PiecewiseLinearCaseSplit>::iterator split1 = splits.begin();
        List<PiecewiseLinearCaseSplit>::iterator split2 = split1;
        ++split2;

        TS_ASSERT( isPositiveSplit( b, f, split1 ) || isPositiveSplit( b, f, split2 ) );
        TS_ASSERT( isNegativeSplit( b, f, split1 ) || isNegativeSplit( b, f, split2 ) );
    }

    bool isPositiveSplit( unsigned b, unsigned f, List<PiecewiseLinearCaseSplit>::iterator &split )
    {
        List<Tightening> bounds = split->getBoundTightenings();

        auto bound = bounds.begin();
        Tightening bound1 = *bound;

        TS_ASSERT_EQUALS( bound1._variable, b );
        TS_ASSERT_EQUALS( bound1._value, 0.0 );

        if ( bound1._type != Tightening::LB )
            return false;

        TS_ASSERT_EQUALS( bounds.size(), 1U );

        Equation positiveEquation;
        auto equations = split->getEquations();
        TS_ASSERT_EQUALS( equations.size(), 1U );
        positiveEquation = split->getEquations().front();
        TS_ASSERT_EQUALS( positiveEquation._addends.size(), 2U );
        TS_ASSERT_EQUALS( positiveEquation._scalar, 0.0 );

        auto addend = positiveEquation._addends.begin();
        TS_ASSERT_EQUALS( addend->_coefficient, 1.0 );
        TS_ASSERT_EQUALS( addend->_variable, b );

        ++addend;
        TS_ASSERT_EQUALS( addend->_coefficient, -1.0 );
        TS_ASSERT_EQUALS( addend->_variable, f );

        TS_ASSERT_EQUALS( positiveEquation._type, Equation::EQ );

        return true;
    }

    bool isNegativeSplit( unsigned b, unsigned f, List<PiecewiseLinearCaseSplit>::iterator &split )
    {
        List<Tightening> bounds = split->getBoundTightenings();

        auto bound = bounds.begin();
        Tightening bound1 = *bound;

        TS_ASSERT_EQUALS( bound1._variable, b );
        TS_ASSERT_EQUALS( bound1._value, 0.0 );

        if ( bound1._type != Tightening::UB )
            return false;

        Equation negativeEquation;
        auto equations = split->getEquations();
        TS_ASSERT_EQUALS( equations.size(), 1U );
        negativeEquation = split->getEquations().front();
        TS_ASSERT_EQUALS( negativeEquation._addends.size(), 2U );
        TS_ASSERT_EQUALS( negativeEquation._scalar, 0.0 );

        auto addend = negativeEquation._addends.begin();
        TS_ASSERT_EQUALS( addend->_coefficient, 1.0 );
        TS_ASSERT_EQUALS( addend->_variable, b );

        ++addend;
        TS_ASSERT_EQUALS( addend->_coefficient, 1.0 );
        TS_ASSERT_EQUALS( addend->_variable, f );

        TS_ASSERT_EQUALS( negativeEquation._type, Equation::EQ );
        return true;
    }

    void test_constraint_phase_gets_fixed()
    {
        unsigned b = 1;
        unsigned f = 4;

        MockTableau tableau;

        // Upper bounds
        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyUpperBound( b, -1.0 );
            TS_ASSERT( clip.phaseFixed() );
        }

        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyUpperBound( b, 0.0 );
            TS_ASSERT( clip.phaseFixed() );
        }

        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyUpperBound( f, 5 );
            TS_ASSERT( !clip.phaseFixed() );
        }

        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyUpperBound( b, 3.0 );
            TS_ASSERT( !clip.phaseFixed() );
        }

        // Lower bounds
        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyLowerBound( b, 3.0 );
            TS_ASSERT( clip.phaseFixed() );
        }

        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyLowerBound( b, 0.0 );
            TS_ASSERT( clip.phaseFixed() );
        }

        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyLowerBound( f, 6.0 );
            TS_ASSERT( !clip.phaseFixed() );
        }

        {
            TestClipConstraint clip( b, f );
            TS_ASSERT( !clip.phaseFixed() );
            clip.notifyLowerBound( b, -2.5 );
            TS_ASSERT( !clip.phaseFixed() );
        }
    }

    void test_valid_split_clip_phase_fixed_to_active()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );

        List<PiecewiseLinearConstraint::Fix> fixes;
        List<PiecewiseLinearConstraint::Fix>::iterator it;

        TS_ASSERT( !clip.phaseFixed() );
        TS_ASSERT_THROWS_NOTHING( clip.notifyLowerBound( b, 5 ) );
        TS_ASSERT( clip.phaseFixed() );

        PiecewiseLinearCaseSplit split;
        TS_ASSERT_THROWS_NOTHING( split = clip.getValidCaseSplit() );

        Equation activeEquation;

        List<Tightening> bounds = split.getBoundTightenings();

        TS_ASSERT_EQUALS( bounds.size(), 1U );
        auto bound = bounds.begin();
        Tightening bound1 = *bound;

        TS_ASSERT_EQUALS( bound1._variable, b );
        TS_ASSERT_EQUALS( bound1._type, Tightening::LB );
        TS_ASSERT_EQUALS( bound1._value, 0.0 );

        auto equations = split.getEquations();
        TS_ASSERT_EQUALS( equations.size(), 1U );
        activeEquation = split.getEquations().front();
        TS_ASSERT_EQUALS( activeEquation._addends.size(), 2U );
        TS_ASSERT_EQUALS( activeEquation._scalar, 0.0 );

        auto addend = activeEquation._addends.begin();
        TS_ASSERT_EQUALS( addend->_coefficient, 1.0 );
        TS_ASSERT_EQUALS( addend->_variable, b );

        ++addend;
        TS_ASSERT_EQUALS( addend->_coefficient, -1.0 );
        TS_ASSERT_EQUALS( addend->_variable, f );

        TS_ASSERT_EQUALS( activeEquation._type, Equation::EQ );
    }

    void test_valid_split_clip_phase_fixed_to_inactive()
    {
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );

        List<PiecewiseLinearConstraint::Fix> fixes;
        List<PiecewiseLinearConstraint::Fix>::iterator it;

        TS_ASSERT( !clip.phaseFixed() );
        TS_ASSERT_THROWS_NOTHING( clip.notifyUpperBound( b, -2 ) );
        TS_ASSERT( clip.phaseFixed() );

        PiecewiseLinearCaseSplit split;
        TS_ASSERT_THROWS_NOTHING( split = clip.getValidCaseSplit() );

        Equation activeEquation;

        List<Tightening> bounds = split.getBoundTightenings();

        TS_ASSERT_EQUALS( bounds.size(), 1U );
        auto bound = bounds.begin();
        Tightening bound1 = *bound;

        TS_ASSERT_EQUALS( bound1._variable, b );
        TS_ASSERT_EQUALS( bound1._type, Tightening::UB );
        TS_ASSERT_EQUALS( bound1._value, 0.0 );

        auto equations = split.getEquations();
        TS_ASSERT_EQUALS( equations.size(), 1U );
        activeEquation = split.getEquations().front();
        TS_ASSERT_EQUALS( activeEquation._addends.size(), 2U );
        TS_ASSERT_EQUALS( activeEquation._scalar, 0.0 );

        auto addend = activeEquation._addends.begin();
        TS_ASSERT_EQUALS( addend->_coefficient, 1.0 );
        TS_ASSERT_EQUALS( addend->_variable, b );

        ++addend;
        TS_ASSERT_EQUALS( addend->_coefficient, 1.0 );
        TS_ASSERT_EQUALS( addend->_variable, f );

        TS_ASSERT_EQUALS( activeEquation._type, Equation::EQ );
    }
    */
    void assert_lower_upper_bound( unsigned f,
                                   unsigned b,
                                   double fLower,
                                   double bLower,
                                   double fUpper,
                                   double bUpper,
                                   List<Tightening> entailedTightenings )
    {
        TS_ASSERT_EQUALS( entailedTightenings.size(), 4U );

        Tightening f_lower( f, fLower, Tightening::LB );
        Tightening f_upper( f, fUpper, Tightening::UB );
        Tightening b_lower( b, bLower, Tightening::LB );
        Tightening b_upper( b, bUpper, Tightening::UB );
        for ( const auto &t : { f_lower, f_upper, b_lower, b_upper} )
        {
            TS_ASSERT( entailedTightenings.exists( t ) );
            if ( !entailedTightenings.exists(t) )
            {
              std::cout << " Cannot find tightening ("<< fLower << bLower << fUpper << bUpper << ") : " << std::endl;
              t.dump();

              std::cout << "Entailed tightenings: " << std::endl;
              for ( auto ent : entailedTightenings)
                ent.dump();
            }
        }
    }

    void assert_tightenings_match( List<Tightening> a, List<Tightening> b )
    {
        TS_ASSERT_EQUALS( a.size(), b.size() );

        for ( const auto &it : a )
        {
            TS_ASSERT( b.exists( it ) );
            if ( !b.exists( it ) )
            {
                std::cout << " Cannot find tightening "<< std::endl;
                it.dump();

                std::cout << "Entailed tightenings: " << std::endl;
                for ( auto ent : a )
                    ent.dump();

                std::cout << "Expected tightenings: " << std::endl;
                for ( auto ent : b )
                  ent.dump();
            }
        }
    }

    void test_serialize_and_unserialize()
    {
        unsigned b = 42;
        unsigned f = 7;

        TestClipConstraint originalClip( b, f );
        originalClip.notifyLowerBound( b, -10 );
        originalClip.notifyUpperBound( f, 5 );
        originalClip.notifyUpperBound( f, 5 );

        String originalSerialized = originalClip.serializeToString();
        TestClipConstraint recoveredClip( originalSerialized );

        TS_ASSERT_EQUALS( originalClip.serializeToString(),
                          recoveredClip.serializeToString() );
    }

    void test_initialization_of_CDOs()
    {
        Context context;
        TestClipConstraint *clip1 = new TestClipConstraint( 4, 6 );

        TS_ASSERT_EQUALS( clip1->getContext(), static_cast<Context*>( nullptr ) );
        TS_ASSERT_EQUALS( clip1->getActiveStatusCDO(), static_cast<CDO<bool>*>( nullptr ) );
        TS_ASSERT_EQUALS( clip1->getPhaseStatusCDO(), static_cast<CDO<PhaseStatus>*>( nullptr ) );
        TS_ASSERT_EQUALS( clip1->getInfeasibleCasesCDList(), static_cast<CDList<PhaseStatus>*>( nullptr ) );
        TS_ASSERT_THROWS_NOTHING( clip1->initializeCDOs( &context ) );
        TS_ASSERT_EQUALS( clip1->getContext(), &context );
        TS_ASSERT_DIFFERS( clip1->getActiveStatusCDO(), static_cast<CDO<bool>*>( nullptr ) );
        TS_ASSERT_DIFFERS( clip1->getPhaseStatusCDO(), static_cast<CDO<PhaseStatus>*>( nullptr ) );
        TS_ASSERT_DIFFERS( clip1->getInfeasibleCasesCDList(), static_cast<CDList<PhaseStatus>*>( nullptr ) );

        bool active = false;
        TS_ASSERT_THROWS_NOTHING( active = clip1->isActive() );
        TS_ASSERT_EQUALS( active, true );

        bool phaseFixed = true;
        TS_ASSERT_THROWS_NOTHING( phaseFixed = clip1->phaseFixed() );
        TS_ASSERT_EQUALS( phaseFixed, PHASE_NOT_FIXED );
        TS_ASSERT_EQUALS( clip1->numFeasibleCases(), 2u );


        TS_ASSERT_THROWS_NOTHING( delete clip1 );
    }

    void test_clip_get_cases()
    {
        TestClipConstraint clip( 4, 6 );

        List<PhaseStatus> cases = clip.getAllCases();

        TS_ASSERT_EQUALS( cases.size(), 2u );

        TS_ASSERT( cases.exists( CLIP_PHASE_FLOOR ) );
        TS_ASSERT( cases.exists( CLIP_PHASE_CEILING ) );
        TS_ASSERT( cases.exists( CLIP_PHASE_MIDDLE ) );
    }

    /*
      Test context-dependent Clip state behavior.
     */
    void test_clip_context_dependent_state()
    {
        Context context;
        unsigned b = 1;
        unsigned f = 4;

        TestClipConstraint clip( b, f );

        clip.initializeCDOs( &context );

        TS_ASSERT_EQUALS( clip.getPhaseStatus(), PHASE_NOT_FIXED );

        context.push();
        clip.notifyUpperBound( b, -1 );
        TS_ASSERT_EQUALS( clip.getPhaseStatus(), CLIP_PHASE_FLOOR );

        context.pop();
        TS_ASSERT_EQUALS( clip.getPhaseStatus(), PHASE_NOT_FIXED );
        context.push();

        clip.notifyLowerBound( b, 8 );
        TS_ASSERT_EQUALS( clip.getPhaseStatus(), CLIP_PHASE_CEILING );

        context.pop();
        TS_ASSERT_EQUALS( clip.getPhaseStatus(), PHASE_NOT_FIXED );
    }

    void test_get_cost_function_component()
    {
        /* Test the add cost function component methods */

        unsigned b = 0;
        unsigned f = 1;

        // The clip is fixed, do not add cost term.
        TestClipConstraint clip1 = TestClipConstraint( b, f );
        MockTableau tableau;
        clip1.registerTableau( &tableau );

        clip1.notifyLowerBound( b, 1 );
        clip1.notifyUpperBound( b, 2 );
        tableau.setValue( b, 1.5 );

        TS_ASSERT( clip1.phaseFixed() );
        LinearExpression cost1;
        TS_ASSERT_THROWS_NOTHING( clip1.getCostFunctionComponent( cost1, CLIP_PHASE_CEILING ) );
        TS_ASSERT_EQUALS( cost1._addends.size(), 0u );
        TS_ASSERT_EQUALS( cost1._constant, 0 );


        // The clip is not fixed and add active cost term
        TestClipConstraint clip2 = TestClipConstraint( b, f );
        clip2.registerTableau( &tableau );

        LinearExpression cost2;
        clip2.notifyLowerBound( b, -1 );
        clip2.notifyUpperBound( b, 4 );
        TS_ASSERT( !clip2.phaseFixed() );
        TS_ASSERT_THROWS_NOTHING( clip2.getCostFunctionComponent( cost2, CLIP_PHASE_FLOOR ) );
        TS_ASSERT_EQUALS( cost2._addends.size(), 1u );
        TS_ASSERT_EQUALS( cost2._addends[f], 1 );
        TS_ASSERT_EQUALS( cost2._constant, -1 );

        // The clip is not fixed and add inactive cost term
        TestClipConstraint clip3 = TestClipConstraint( b, f );
        clip3.registerTableau( &tableau );

        LinearExpression cost3;
        clip3.notifyLowerBound( b, -1 );
        clip3.notifyUpperBound( b, 6 );
        TS_ASSERT_THROWS_NOTHING( clip3.getCostFunctionComponent( cost3, CLIP_PHASE_CEILING ) );
        TS_ASSERT_EQUALS( cost3._addends.size(), 1u );
        TS_ASSERT_EQUALS( cost3._addends[f], -1 );
        TS_ASSERT_EQUALS( cost3._constant, 5 );
    }

    void test_get_phase_in_assignment()
    {
        unsigned b = 0;
        unsigned f = 1;

        // The clip is fixed, do not add cost term.
        TestClipConstraint clip = TestClipConstraint( b, f );
        MockTableau tableau;
        clip.registerTableau( &tableau );
        tableau.setValue( b, 1.5 );
        tableau.setValue( f, 2 );

        Map<unsigned, double> assignment;
        assignment[b] = -1;
        TS_ASSERT_EQUALS( clip.getPhaseStatusInAssignment( assignment ),
                          CLIP_PHASE_MIDDLE );

        assignment[b] = 15;
        TS_ASSERT_EQUALS( clip.getPhaseStatusInAssignment( assignment ),
                          CLIP_PHASE_CEILING );
    }

};
