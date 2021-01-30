/*********************                                                        */
/*! \file MarabouError.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Christopher Lazarus
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __MarabouError_h__
#define __MarabouError_h__

#include "Error.h"

class MarabouError : public Error
{
public:
	enum Code {
        ALLOCATION_FAILED = 0,
        VARIABLE_INDEX_OUT_OF_RANGE = 1,
        VARIABLE_DOESNT_EXIST_IN_SOLUTION = 2,
        PARTICIPATING_VARIABLES_ABSENT = 3,
        NON_EQUALITY_INPUT_EQUATION_DISCOVERED = 4,
        REQUESTED_CASE_SPLITS_FROM_FIXED_CONSTRAINT = 5,
        UNBOUNDED_VARIABLES_NOT_YET_SUPPORTED = 6,
        FILE_DOESNT_EXIST = 7,
        MERGED_INPUT_VARIABLE = 8,
        MERGED_OUTPUT_VARIABLE = 9,
        UNSUCCESSFUL_QUEUE_PUSH = 10,
        NETWORK_LEVEL_REASONER_ACTIVATION_NOT_SUPPORTED = 11,
        NETWORK_LEVEL_REASONER_NOT_AVAILABLE = 12,
        GUROBI_NOT_AVAILABLE = 13,
        UNKNOWN_LOCAL_SEARCH_STRATEGY = 14,

        // Error codes for Query Loader
        FILE_DOES_NOT_EXIST = 100,
        INVALID_EQUATION_TYPE = 101,
        UNSUPPORTED_PIECEWISE_LINEAR_CONSTRAINT = 102,

        FEATURE_NOT_YET_SUPPORTED = 900,

        DEBUGGING_ERROR = 999,
    };

    MarabouError( MarabouError::Code code ) : Error( "MarabouError", (int)code )
	{
	}

    MarabouError( MarabouError::Code code, const char *userMessage ) :
        Error( "MarabouError", (int)code, userMessage )
    {
    }
};

#endif // __MarabouError_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
