/*********************                                                        */
/*! \file Engine.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling, Andrew Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#pragma once

#ifdef _WIN32
#undef ERROR
#endif

#include "IEngine.h"

#include "InputQuery.h"
#include "Statistics.h"

namespace prop {
class TheoryProxy;
class MinisatSatSolver;
}

namespace marabou {

class SmtCore;
class TheoryEngine;

class Engine : public IEngine
{
public:
    enum {
          MICROSECONDS_TO_SECONDS = 1000000,
    };

    Engine();
    ~Engine();


    /*
      Attempt to find a feasible solution for the input within a time limit
      (a timeout of 0 means no time limit). Returns true if found, false if infeasible.
    */
    bool solve( unsigned timeoutInSeconds = 0 );
    bool processInputQuery( InputQuery &inputQuery );
    bool processInputQuery( InputQuery &inputQuery, bool preprocess );

    void extractSolution( InputQuery &inputQuery );

    /*
      Get the exit code
    */
    ExitCode getExitCode() const;

    const Statistics *getStatistics() const;

private:

  Statistics _statistics;

  SmtCore *_smtCore;
  TheoryEngine *_theoryEngine;
  prop::MinisatSatSolver *_satSolver;
  prop::TheoryProxy *_theoryProxy;
};

} // namespace marabou
