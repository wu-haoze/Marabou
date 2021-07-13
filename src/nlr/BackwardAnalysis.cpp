/*********************                                                        */
/*! \file BackwardAnalysis.cpp
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

#include "BackwardAnalysis.h"
#include "GurobiWrapper.h"
#include "InfeasibleQueryException.h"
#include "Layer.h"
#include "MStringf.h"
#include "NLRError.h"
#include "Options.h"
#include "TimeUtils.h"
#include "Vector.h"

#include <boost/thread.hpp>

namespace NLR {

BackwardAnalysis::BackwardAnalysis( LayerOwner *layerOwner )
    : _layerOwner( layerOwner )
    , _lpFormulator( layerOwner )
{
}

void BackwardAnalysis::run( const Map<unsigned, Layer *> &layers )
{
    unsigned numberOfWorkers = Options::get()->getInt( Options::NUM_WORKERS );

    // Time to wait if no idle worker is availble
    boost::chrono::milliseconds waitTime( numberOfWorkers - 1 );

    Map<GurobiWrapper *, unsigned> solverToIndex;
    // Create a queue of free workers
    // When a worker is working, it is popped off the queue, when it is done, it
    // is added back to the queue.
    SolverQueue freeSolvers( numberOfWorkers );
    for ( unsigned i = 0; i < numberOfWorkers; ++i )
    {
        GurobiWrapper *gurobi = new GurobiWrapper();
        solverToIndex[gurobi] = i;
        enqueueSolver( freeSolvers, gurobi );
    }

    boost::thread *threads = new boost::thread[numberOfWorkers];
    std::mutex mtx;
    std::atomic_bool infeasible( false );

    double currentLb;
    double currentUb;

    std::atomic_uint tighterBoundCounter( 0 );
    std::atomic_uint signChanges( 0 );
    std::atomic_uint cutoffs( 0 );

    struct timespec gurobiStart;
    (void) gurobiStart;
    struct timespec gurobiEnd;
    (void) gurobiEnd;

    double dontCare = 0;
    gurobiStart = TimeUtils::sampleMicro();

    bool skipTightenLb = false; // If true, skip lower bound tightening
    bool skipTightenUb = false; // If true, skip upper bound tightening

    unsigned numberOfTighterBoundsInPreviousLayer = 0;
    unsigned numberOfLayers = _layerOwner->getNumberOfLayers();
    for ( unsigned i = numberOfLayers - 1; i != 0; --i )
    {
        BackwardAnalysis_LOG( Stringf( "Handling layer %u", i ).ascii() );
        Layer *layer = layers[i];

        if ( layer->getLayerType() == Layer::LEAKY_RELU )
        {
            printf("leaky relu layer\n" );
        }

        for ( unsigned i = 0; i < layer->getSize(); ++i )
        {
            if ( layer->neuronEliminated( i ) )
                continue;

            currentLb = layer->getLb( i );
            currentUb = layer->getUb( i );

            if ( infeasible )
            {
                // infeasibility is derived, interupt all active threads
                for ( unsigned i = 0; i < numberOfWorkers; ++i )
                {
                    threads[i].interrupt();
                    threads[i].join();
                }
                clearSolverQueue( freeSolvers );
                throw InfeasibleQueryException();
            }

            // Wait until there is an idle solver
            GurobiWrapper *freeSolver;
            while ( !freeSolvers.pop( freeSolver ) )
                boost::this_thread::sleep_for( waitTime );

            freeSolver->resetModel();

            mtx.lock();
            _lpFormulator.createLPRelaxationAfter( layers, *freeSolver,
                                                  layer->getLayerIndex() );
            mtx.unlock();

            // spawn a thread to tighten the bounds for the current variable
            ThreadArgument argument( freeSolver, layer,
                                     i, currentLb, currentUb,
                                     false, dontCare,
                                     _layerOwner, std::ref( freeSolvers ),
                                     std::ref( mtx ), std::ref( infeasible ),
                                     std::ref( tighterBoundCounter ),
                                     std::ref( signChanges ),
                                     std::ref( cutoffs ),
                                     skipTightenLb,
                                     skipTightenUb );

            if ( numberOfWorkers == 1 )
                _lpFormulator.tightenSingleVariableBoundsWithLPRelaxation( argument );
            else
                threads[solverToIndex[freeSolver]] = boost::thread
                    ( _lpFormulator.tightenSingleVariableBoundsWithLPRelaxation, argument );
        }
        for ( unsigned i = 0; i < numberOfWorkers; ++i )
        {
            threads[i].join();
        }
        numberOfTighterBoundsInPreviousLayer = tighterBoundCounter.load() -
            numberOfTighterBoundsInPreviousLayer;
        printf( "Handling layer %u done, number of "
                         "tightened bounds found: %u\n", i,
                         numberOfTighterBoundsInPreviousLayer );
        if ( numberOfTighterBoundsInPreviousLayer == 0 )
            break;
    }

    gurobiEnd = TimeUtils::sampleMicro();

    printf( "Number of tighter bounds found by Gurobi: %u. Sign changes: %u. Cutoffs: %u\n",
                               tighterBoundCounter.load(), signChanges.load(), cutoffs.load() );
    printf( "Seconds spent Gurobiing: %llu\n", TimeUtils::timePassed( gurobiStart, gurobiEnd ) / 1000000 );

    clearSolverQueue( freeSolvers );

    if ( infeasible )
        throw InfeasibleQueryException();
}

} // namespace NLR
