/*********************                                                        */
/*! \file SmtCore.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Parth Shah
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#ifndef __SmtCore_h__
#define __SmtCore_h__

#include "context/context.h"
#include "context/cdlist.h"
#include "PiecewiseLinearCaseSplit.h"
#include "PiecewiseLinearConstraint.h"
#include "SmtState.h"
#include "Stack.h"
#include "SmtStackEntry.h"
#include "Statistics.h"

#include "Vector.h"
#include <memory>

#define SMT_LOG( x, ... ) LOG( GlobalConfiguration::SMT_CORE_LOGGING, "SmtCore: %s\n", x )

class EngineState;
class IEngine;
class String;

class SmtCore
{
public:
    SmtCore( IEngine *engine, CVC4::context::Context &context );
    ~SmtCore();

    /*
      Clear the stack.
    */
    void freeMemory();

    /*
      Reset the SmtCore
    */
    void reset();

    /*
      Inform the SMT core that a random flip happened.
    */
    void reportRandomFlip();

    /*
      Reset all reported violation counts.
    */
    void resetReportedViolations();

    /*
      Returns true iff the SMT core wants to perform a case split.
    */
    bool needToSplit() const;

    /*
      Perform the split according to the constraint marked for
      splitting. Update bounds, add equations and update the stack.
    */
    void performSplit();

    /*
      Pop an old split from the stack, and perform a new split as
      needed. Return true if successful, false if the stack is empty.
    */
    bool popSplit();

    /*
      The current stack depth.
    */
    unsigned getStackDepth() const;

    /*
      Let the smt core know of an implied valid case split that was discovered.
    */
    void recordImpliedValidSplit( PiecewiseLinearCaseSplit &validSplit );

    /*
      Return a list of all splits performed so far, both SMT-originating and valid ones,
      in the correct order.
    */
    void allSplitsSoFar( List<PiecewiseLinearCaseSplit> &result ) const;

    /*
      Have the SMT core start reporting statistics.
    */
    void setStatistics( Statistics *statistics );

    void setConstraintViolationThreshold( unsigned threshold );

    /*
      Pick the piecewise linear constraint for splitting, returns true
      if a constraint for splitting is successfully picked
    */
    bool pickSplitPLConstraint();

private:
    /*
      Valid splits that were implied by level 0 of the stack.
    */
    List<PiecewiseLinearCaseSplit> _impliedValidSplitsAtRoot;

    /*
      CVC4 Context, constructed in Engine
    */
    CVC4::context::Context& _context;

    /*
      Collect and print various statistics.
    */
    Statistics *_statistics;

    /*
      The case-split stack.
    */
    List<SmtStackEntry *> _stack;

    /*
      The engine.
    */
    IEngine *_engine;

    /*
      Do we need to perform a split and on which constraint.
    */
    bool _needToSplit;
    PiecewiseLinearConstraint *_constraintForSplitting;

    /*
      Split when some relu has been violated for this many times
    */
    unsigned _constraintViolationThreshold;

    bool _localSearch;

    unsigned _numberOfRandomFlips;
};

#endif // __SmtCore_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
