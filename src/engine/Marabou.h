/*********************                                                        */
/*! \file Marabou.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __Marabou_h__
#define __Marabou_h__

#include "AcasParser.h"
#include "Engine.h"
#include "InputQuery.h"
#include "SubQuery.h"

class Marabou
{
public:
    Marabou( unsigned verbosity = 2, bool ggOutput = false );
    ~Marabou();

    /*
      Entry point of this class
    */
    void run();

private:
    InputQuery _inputQuery;

    /*
      Extract the input files: network and property, and use them
      to generate the input query
    */
    void prepareInputQuery();

    /*
      Invoke the engine to solve the input query
    */
    void solveQuery();

    bool lookAheadPreprocessing();

    /*
      Display the results
    */
    void displayResults( unsigned long long microSecondsElapsed );


    /**
     * Resets this solver, and creates subqueries.
     */
    SubQueries split( unsigned divides );

    /**
     * Creates output files containing thunks for these SubQueries, as well as
     * a merge thunk.
     */
    void dumpSubQueriesAsThunks( const SubQueries &subQueries ) const;

    /**
     * Creates empty output files for the subproblems of this problem.
     */
    void createEmptySubproblemOutputs() const;

    BiasStrategy setBiasStrategyFromOptions( const String strategy );

    /*
      ACAS network parser
    */
    AcasParser *_acasParser;

    /*
      The solver
    */
    Engine _engine;

    /*
     * Whether the output should be GG-compatible.
     * In the case of TIMEOUT, this means that the summary file will contain a
     * continuation thunk dependent on subproblem thunks.
     * In the case of SAT/UNSAT, this just means that empty subproblem thunk
     * files will be created.
     */
    bool _ggOutput;
};

#endif // __Marabou_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
