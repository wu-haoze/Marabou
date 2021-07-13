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

namespace NLR {

#define BackwardAnalysis_LOG(x, ...) LOG(GlobalConfiguration::MILP_BASED_BOUND_TIGHTENING_LOGGING, "Backward Analysis: %s\n", x)

class BackwardAnalysis : public ParallelSolver
{
public:
    enum MinOrMax {
        MIN = 0,
        MAX = 1,
    };

    BackwardAnalysis( LayerOwner *layerOwner,
                      std::vector<LPFormulator *> &lpFormulators );

    void run( const Map<unsigned, Layer *> &layers );

private:
    LayerOwner *_layerOwner;
    LPFormulator _lpFormulator;
    std::vector<LPFormulator *> _lpFormulators;

    void handleLeakyReLULayer( Layer *layer );

    static double optimizeWithGurobi( GurobiWrapper &gurobi,
                                      MinOrMax minOrMax,
                                      String variableName,
                                      std::atomic_bool *infeasible );

    static void optimizeBounds( TighteningQueryQueue *workload,
                                GurobiWrapper *gurobi,
                                LPFormulator *formulator,
                                unsigned layerIndex,
                                std::atomic_bool &shouldQuitSolving,
                                std::atomic_uint &numUnsolved,
                                std::atomic_bool &infeasible,
                                TighteningQueue *tighteningQueue,
                                unsigned threadId );

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
