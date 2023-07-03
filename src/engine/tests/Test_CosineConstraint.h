/*********************                                                        */
/*! \file Test_CosineConstraint.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Teruhiro Tagomori, Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include <cxxtest/TestSuite.h>

#include "InputQuery.h"
#include "MarabouError.h"
#include "CosineConstraint.h"
#include "MockTableau.h"
#include <string.h>

class MockForCosineConstraint
{
public:
};

/*
   Exposes protected members of CosineConstraint for testing.
 */
class TestCosineConstraint : public CosineConstraint
{
public:
    TestCosineConstraint( unsigned b, unsigned f  )
        : CosineConstraint( b, f )
    {}
};

class MaxConstraintTestSuite : public CxxTest::TestSuite
{
public:
    MockForCosineConstraint *mock;

    void setUp()
    {
        TS_ASSERT( mock = new MockForCosineConstraint );
    }

    void tearDown()
    {
        TS_ASSERT_THROWS_NOTHING( delete mock );
    }

    void test_cosine_constraint()
    {
        unsigned b = 1;
        unsigned f = 4;

        CosineConstraint cosine( b, f );

        List<unsigned> participatingVariables;
        TS_ASSERT_THROWS_NOTHING( participatingVariables = cosine.getParticipatingVariables() );
        TS_ASSERT_EQUALS( participatingVariables.size(), 2U );
        auto it = participatingVariables.begin();
        TS_ASSERT_EQUALS( *it, b );
        ++it;
        TS_ASSERT_EQUALS( *it, f );

        TS_ASSERT( cosine.participatingVariable( b ) );
        TS_ASSERT( cosine.participatingVariable( f ) );
        TS_ASSERT( !cosine.participatingVariable( 0 ) );
        TS_ASSERT( !cosine.participatingVariable( 2 ) );
        TS_ASSERT( !cosine.participatingVariable( 3 ) );
        TS_ASSERT( !cosine.participatingVariable( 5 ) );

        // not obsolete yet
        TS_ASSERT( !cosine.constraintObsolete() );

        // eliminate variable b
        cosine.eliminateVariable( b, 0 );  // 0 is dummy for the argument of fixedValue

        // cosine is obsolete now
        TS_ASSERT( cosine.constraintObsolete() );
    }

    void test_cosine_duplicate()
    {
        unsigned b = 1;
        unsigned f = 4;

        CosineConstraint *cosine = new CosineConstraint( b, f );

        List<unsigned> participatingVariables;
        TS_ASSERT_THROWS_NOTHING( participatingVariables = cosine->getParticipatingVariables() );

        // not obsolete yet
        TS_ASSERT( !cosine->constraintObsolete() );

        // duplicate constraint
        CosineConstraint *cosine2 = dynamic_cast<CosineConstraint *>( cosine->duplicateConstraint() );
        TS_ASSERT_THROWS_NOTHING( participatingVariables = cosine2->getParticipatingVariables() );
        TS_ASSERT_EQUALS( participatingVariables.size(), 2U );
        auto it = participatingVariables.begin();
        TS_ASSERT_EQUALS( *it, b );
        ++it;
        TS_ASSERT_EQUALS( *it, f );
        TS_ASSERT( cosine2->participatingVariable( b ) );
        TS_ASSERT( cosine2->participatingVariable( f ) );
        TS_ASSERT( !cosine2->participatingVariable( 0 ) );
        TS_ASSERT( !cosine2->participatingVariable( 2 ) );
        TS_ASSERT( !cosine2->participatingVariable( 3 ) );
        TS_ASSERT( !cosine2->participatingVariable( 5 ) );

        // eliminate variable b
        cosine->eliminateVariable( b, 0 );  // 0 is dummy for the argument of fixedValue

        // cosine is obsolete now
        TS_ASSERT( cosine->constraintObsolete() );

        // cosine2 is not obsolete
        TS_ASSERT( !cosine2->constraintObsolete() );

        TS_ASSERT_THROWS_NOTHING( delete cosine );
        TS_ASSERT_THROWS_NOTHING( delete cosine2 );
    }

    void test_cosine_notify_bounds_b_1()
    {
        unsigned b = 1;
        unsigned f = 4;

        CosineConstraint cosine( b, f );


        cosine.notifyLowerBound( f, -1.0 );
        cosine.notifyUpperBound( f, 1.0 );

        cosine.notifyLowerBound( b, -1.0 );
        cosine.notifyUpperBound( b, 1.0 );

        List<Tightening> tightenings;
        cosine.getEntailedTightenings( tightenings );

        auto it = tightenings.begin();
        TS_ASSERT_EQUALS( it->_type, Tightening::LB );
        TS_ASSERT_EQUALS( it->_variable, b );
        TS_ASSERT( FloatUtils::areEqual( it->_value, -1.0 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::LB );
        TS_ASSERT_EQUALS( it->_variable, f );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 0.5403, 0.001 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::UB );
        TS_ASSERT_EQUALS( it->_variable, b );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 1 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::UB );
        TS_ASSERT_EQUALS( it->_variable, f );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 1, 0.001 ) );
    }

    void test_cosine_notify_bounds_b_2()
    {
        unsigned b = 1;
        unsigned f = 4;

        CosineConstraint cosine( b, f );


        cosine.notifyLowerBound( f, -1.0 );
        cosine.notifyUpperBound( f, 1.0 );

        cosine.notifyLowerBound( b, 1 );
        cosine.notifyUpperBound( b, 6 );

        List<Tightening> tightenings;
        cosine.getEntailedTightenings( tightenings );

        auto it = tightenings.begin();
        TS_ASSERT_EQUALS( it->_type, Tightening::LB );
        TS_ASSERT_EQUALS( it->_variable, b );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 1 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::LB );
        TS_ASSERT_EQUALS( it->_variable, f );
        TS_ASSERT( FloatUtils::areEqual( it->_value, -1, 0.001 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::UB );
        TS_ASSERT_EQUALS( it->_variable, b );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 6 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::UB );
        TS_ASSERT_EQUALS( it->_variable, f );
        std::cout << it->_value << std::endl;
        TS_ASSERT( FloatUtils::areEqual( it->_value, 0.96017, 0.001 ) );
    }

    void test_cosine_notify_bounds_f()
    {
        unsigned b = 1;
        unsigned f = 4;

        CosineConstraint cosine( b, f );

        cosine.notifyLowerBound( f, -1 );
        cosine.notifyUpperBound( f, 1 );

        cosine.notifyLowerBound( f, 0.2 );
        cosine.notifyUpperBound( f, 0.5 );
        List<Tightening> tightenings;
        cosine.getEntailedTightenings( tightenings );

        auto it = tightenings.begin();
        TS_ASSERT_EQUALS( it->_type, Tightening::LB );
        TS_ASSERT_EQUALS( it->_variable, f );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 0.2 ) );

        ++it;
        TS_ASSERT_EQUALS( it->_type, Tightening::UB );
        TS_ASSERT_EQUALS( it->_variable, f );
        TS_ASSERT( FloatUtils::areEqual( it->_value, 0.5 ) );
    }

    void test_cosine_serialize()
    {
        unsigned b = 0;
        unsigned f = 1;

        CosineConstraint cosine( b, f );

        String serializedString = cosine.serializeToString();

        CosineConstraint serializedCos( serializedString.ascii() );

        TS_ASSERT_EQUALS( serializedCos.getB(), b );
        TS_ASSERT_EQUALS( serializedCos.getF(), f );

    }
};
