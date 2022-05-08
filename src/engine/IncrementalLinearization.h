/*********************                                                        */
/*! \file IncrementalLinearization.h
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

#ifndef __IncrementalLinearization_h__
#define __IncrementalLinearization_h__

#include "GurobiWrapper.h"
#include "InputQuery.h"
#include "ITableau.h"
#include "MILPEncoder.h"
#include "MStringf.h"
#include "Map.h"

#define INCREMENTAL_LINEARIZATION_LOG(x, ...) LOG(GlobalConfiguration::INCREMENTAL_LINEARIZATION_LOGGING, "IncrementalLinearization: %s\n", x)

class IncrementalLinearization
{
public:    
    IncrementalLinearization( MILPEncoder &milpEncoder, InputQuery &inputQuery );

    /*
      Solve with incremental linarizations.
      Only for the purpose of TranscendetalConstarints
    */
    IEngine::ExitCode solveWithIncrementalLinearization( GurobiWrapper &gurobi,
                                                         double timeoutInSeconds,
                                                         unsigned threads = 1,
                                                         unsigned verbosity = 2 );

private:
    /*
      MILPEncoder
    */
    MILPEncoder &_milpEncoder;
    InputQuery &_inputQuery;

    /*
      add new constraints
    */
    void incrementLinearConstraint( TranscendentalConstraint *constraint,
                                    const Map<String, double> &assignment,
                                    unsigned &satisfied,
                                    unsigned &tangentAdded,
                                    unsigned &secantAdded,
                                    unsigned &skipped );
};

#endif // __IncrementalLinearization_h__
