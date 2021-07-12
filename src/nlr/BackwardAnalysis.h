/*********************                                                        */
/*! \file BackwardAnalysis.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#ifndef __BackwardAnalysis_h__
#define __BackwardAnalysis_h__

#include "LPFormulator.h"
#include "GurobiWrapper.h"
#include "LayerOwner.h"
#include "ParallelSolver.h"
#include <climits>

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/chrono.hpp>
#include <mutex>

namespace NLR {

#define BackwardAnalysis_LOG(x, ...) LOG(GlobalConfiguration::PREPROCESSOR_LOGGING, "Backward Analysis: %s\n", x)

class BackwardAnalysis : public ParallelSolver
{
public:
    enum MinOrMax {
        MIN = 0,
        MAX = 1,
    };

    BackwardAnalysis( LayerOwner *layerOwner );

    void run();

private:
    LayerOwner *_layerOwner;
    LPFormulator _lpFormulator;

    void handleLeakyReLULayer( Layer *layer );
};

} // namespace NLR

#endif // __BackwardAnalysis_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
