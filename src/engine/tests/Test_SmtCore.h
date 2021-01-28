/*********************                                                        */
/*! \file Test_SmtCore.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling, Derek Huang
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
#include "MockEngine.h"
#include "MockErrno.h"
#include "Options.h"
#include "PiecewiseLinearConstraint.h"
#include "ReluConstraint.h"
#include "SmtCore.h"

#include <string.h>

class MockForSmtCore
{
public:
};

class SmtCoreTestSuite : public CxxTest::TestSuite
{
public:
    MockForSmtCore *mock;
    MockEngine *engine;

    class MockConstraint : public PiecewiseLinearConstraint
    {
    public:
        MockConstraint()
            : setActiveWasCalled( false )
        {
            nextIsActive = true;
        }

        PiecewiseLinearFunctionType getType() const
        {
            return (PiecewiseLinearFunctionType)999;
        }

        PiecewiseLinearConstraint *duplicateConstraint() const
        {
            return NULL;
        }

        void restoreState( const PiecewiseLinearConstraint */* state */ )
        {
        }

        void registerAsWatcher( ITableau * )
        {
        }

        void unregisterAsWatcher( ITableau * )
        {
        }

        bool setActiveWasCalled;
        void setActiveConstraint( bool active )
        {
            TS_ASSERT( active == false );
            setActiveWasCalled = true;
        }

        bool nextIsActive;
        bool isActive() const
        {
            return nextIsActive;
        }

        bool participatingVariable( unsigned ) const
        {
            return true;
        }

        List<unsigned> getParticipatingVariables() const
        {
            return List<unsigned>();
        }


        bool satisfied() const
        {
            return true;
        }

        List<PiecewiseLinearConstraint::Fix> getPossibleFixes() const
        {
            return List<PiecewiseLinearConstraint::Fix>();
        }

        List<PiecewiseLinearConstraint::Fix> getSmartFixes( ITableau * ) const
        {
            return List<PiecewiseLinearConstraint::Fix>();
        }

        List<PiecewiseLinearCaseSplit> nextSplits;
        List<PiecewiseLinearCaseSplit> getCaseSplits() const
        {
            return nextSplits;
        }

        bool phaseFixed() const
        {
            return true;
        }

        PiecewiseLinearCaseSplit getValidCaseSplit() const
        {
            PiecewiseLinearCaseSplit dontCare;
            return dontCare;
        }

		void updateVariableIndex( unsigned, unsigned )
		{
		}

		void eliminateVariable( unsigned, double )
		{
		}

        bool constraintObsolete() const
        {
            return false;
        }

		void preprocessBounds( unsigned, double, Tightening::BoundType )
		{
		}

        void getEntailedTightenings( List<Tightening> & ) const
        {
        }

        void getAuxiliaryEquations( List<Equation> &/* newEquations */ ) const
        {
        }

        String serializeToString() const
        {
            return "";
        }
    };

    void setUp()
    {
        TS_ASSERT( mock = new MockForSmtCore );
        TS_ASSERT( engine = new MockEngine );
    }

    void tearDown()
    {
        TS_ASSERT_THROWS_NOTHING( delete engine );
        TS_ASSERT_THROWS_NOTHING( delete mock );
    }
    void test_todo()
    {
        // Reason: the inefficiency in resizing the tableau mutliple times
        TS_TRACE( "add support for adding multiple equations at once, not one-by-one" );
    }
};

//
// Local Variables:
// compile-command: "make -C ../../.. "
// tags-file-name: "../../../TAGS"
// c-basic-offset: 4
// End:
//
