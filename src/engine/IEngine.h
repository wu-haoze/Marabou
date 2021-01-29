/*********************                                                        */
/*! \file IEngine.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#ifndef __IEngine_h__
#define __IEngine_h__

#include "DivideStrategy.h"
#include "SnCDivideStrategy.h"
#include "List.h"

#ifdef _WIN32
#undef ERROR
#endif

class EngineState;
class Equation;
class PiecewiseLinearCaseSplit;
class SmtState;
class PiecewiseLinearConstraint;

class IEngine
{
public:
    virtual ~IEngine() {};

    enum ExitCode {
        UNSAT = 0,
        SAT = 1,
        ERROR = 2,
        TIMEOUT = 3,
        QUIT_REQUESTED = 4,

        NOT_DONE = 999,
    };

    /*
      Add equations and apply tightenings from a PL case split.
    */
    virtual void applySplit( const PiecewiseLinearCaseSplit &split ) = 0;

    virtual void setNumPlConstraintsDisabledByValidSplits( unsigned numConstraints ) = 0;

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

    /*
      Pick the piecewise linear constraint for internal splitting
    */
    virtual PiecewiseLinearConstraint *pickSplitPLConstraint() = 0;

    /*
      Pick the piecewise linear constraint for SnC splitting
    */
    virtual PiecewiseLinearConstraint *pickSplitPLConstraintSnC( SnCDivideStrategy
                                                                 strategy ) = 0;

    virtual void pushContext();
    virtual void popContext();
};

#endif // __IEngine_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
