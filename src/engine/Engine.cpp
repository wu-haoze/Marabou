/*********************                                                        */
/*! \file Engine.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling, Andrew Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief [[ Add one-line brief description here ]]
 **
 ** [[ Add lengthier description here ]]
 **/

#include "Engine.h"

#include "DeepSoIEngine.h"
#include "minisat.h"
#include "Statistics.h"
#include "theory_proxy.h"

#include <random>

using namespace prop;

namespace marabou {

Engine::Engine()
    : _statistics()
    , _theoryEngine(new DeepSoIEngine())
    , _satSolver(NULL)
    , _theoryProxy(NULL)
{
    _satSolver = new MinisatSatSolver(&_statistics);
    _theoryProxy = new TheoryProxy(_theoryEngine);
}

Engine::~Engine()
{
  if ( _satSolver ) {
    delete _satSolver;
    _satSolver = NULL;
  }

  if ( _theoryProxy ) {
    delete _theoryProxy;
    _theoryProxy = NULL;
  }

  if ( _theoryEngine ) {
    delete _theoryEngine;
    _theoryEngine = NULL;
  }
}

bool Engine::solve( unsigned timeoutInSeconds )
{
  return true;
}

bool Engine::processInputQuery( InputQuery &inputQuery )
{
  return true;
}

bool Engine::processInputQuery( InputQuery &inputQuery, bool preprocess )
{
  return true;
}

void Engine::extractSolution( InputQuery &inputQuery )
{
}



ExitCode Engine::getExitCode() const {
  return _theoryEngine->getExitCode();
}

const Statistics *Engine::getStatistics() const
{
  return &_statistics;
}


} // namespace marabou
