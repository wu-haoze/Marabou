/*********************                                                        */
/*! \file TheoryEngine.h
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

#pragma once

#include "DivideStrategy.h"
#include "ExitCode.h"
#include "SnCDivideStrategy.h"
#include "TableauStateStorageLevel.h"
#include "List.h"
#include "context/context.h"

#ifdef _WIN32
#undef ERROR
#endif

class EngineState;
class Equation;
class PiecewiseLinearCaseSplit;
class SmtState;
class String;
class PiecewiseLinearConstraint;

namespace marabou {

class TheoryEngine {
public:
    virtual ~TheoryEngine() {};

    /*
      Add equations and apply tightenings from a PL case split.
    */
    virtual void applySplit( const PiecewiseLinearCaseSplit &split ) = 0;

    /*
      Register initial SnC split
    */
    virtual void applySnCSplit( PiecewiseLinearCaseSplit split, String queryId ) = 0;

    /*
      Hooks invoked before/after context push/pop to store/restore/update context independent data.
    */
    virtual void preContextPushHook() = 0;
    virtual void postContextPopHook() = 0;

    /*
      Methods for storing and restoring the state of the engine.
    */
    virtual void storeState( EngineState &state, TableauStateStorageLevel level ) const = 0;
    virtual void restoreState( const EngineState &state ) = 0;
    virtual void setNumPlConstraintsDisabledByValidSplits( unsigned numConstraints ) = 0;

    /*
      Store the current stack of the smtCore into smtState
    */
    virtual void storeSmtState( SmtState &smtState ) = 0;

    /*
      Apply the stack to the newly created SmtCore, returns false if UNSAT is
      found in this process.
    */
    virtual bool restoreSmtState( SmtState &smtState ) = 0;

    /*
      Solve the encoded query.
    */
    virtual bool solve( unsigned timeoutInSeconds ) = 0;

    /*
      Retrieve the exit code.
    */
    virtual ExitCode getExitCode() const = 0;

    /*
      Methods for DnC: reset the engine state for re-use,
      get input variables.
    */
    virtual void reset() = 0;
    virtual List<unsigned> getInputVariables() const = 0;

    virtual void applyAllBoundTightenings() = 0;

    virtual bool applyAllValidConstraintCaseSplits() = 0;
    /*
      Get Context reference
     */
    virtual CVC4::context::Context &getContext() = 0;

    virtual bool consistentBounds() const = 0;
};

} // namespace marabou
