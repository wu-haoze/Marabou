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

#include <thread>

namespace NLR {

BackwardAnalysis::BackwardAnalysis( LayerOwner *layerOwner,
                                    std::vector<LPFormulator *> &lpFormulators )
    : _layerOwner( layerOwner )
    , _lpFormulator( layerOwner )
    , _lpFormulators( lpFormulators )
{
}

double BackwardAnalysis::optimizeWithGurobi( GurobiWrapper &gurobi,
                                             MinOrMax minOrMax,
                                             String variableName,
                                             std::atomic_bool *infeasible )
{
    List<GurobiWrapper::Term> terms;
    terms.append( GurobiWrapper::Term( 1, variableName ) );

    if ( minOrMax == MAX )
        gurobi.setObjective( terms );
    else
        gurobi.setCost( terms );

    gurobi.solve();

    if ( gurobi.infeasbile() )
    {
        if ( infeasible )
        {
            *infeasible = true;
            return FloatUtils::infinity();
        }
        else
            throw InfeasibleQueryException();
    }

    if ( gurobi.optimal() )
    {
        Map<String, double> dontCare;
        double result = 0;
        gurobi.extractSolution( dontCare, result );
        return result;
    }

    throw NLRError( NLRError::UNEXPECTED_RETURN_STATUS_FROM_GUROBI );
}


void BackwardAnalysis::optimizeBounds( TighteningQueryQueue *workload,
                                       GurobiWrapper *gurobi,
                                       LPFormulator *formulator,
                                       unsigned layerIndex,
                                       std::atomic_bool &shouldQuitSolving,
                                       std::atomic_uint &numUnsolved,
                                       std::atomic_bool &infeasible,
                                       TighteningQueue *tighteningQueue,
                                       unsigned  )
{
    gurobi->resetModel();
    formulator->createLPRelaxationAfter
        ( formulator->_layerOwner->getLayerIndexToLayer(), *gurobi,
          layerIndex );
    //printf( "Worker %u started...", threadId );
    while ( numUnsolved.load() > 0 && infeasible.load() == false )
    {
        TighteningQuery *q = NULL;
        if ( workload->pop( q ) )
        {
            unsigned variable = q->_variable;
            double currentLb = q->_currentLb;
            double currentUb = q->_currentUb;
            delete q;

            Stringf variableName( "x%u", variable );

            BackwardAnalysis_LOG( Stringf( "Computing upperbound..." ).ascii() );
            gurobi->reset();
            gurobi->setNumberOfThreads( 1 );
            double ub = optimizeWithGurobi( *gurobi, MinOrMax::MAX,
                                            variableName, &infeasible );
            BackwardAnalysis_LOG( Stringf( "Upperbound computed %f -> %f",
                                       currentUb, ub ).ascii() );
            if ( FloatUtils::lt( ub, currentLb ) )
            {
                BackwardAnalysis_LOG( Stringf( "Found invalid bound! lb: %u, ub: %u",
                                               currentLb, ub ).ascii() );
                infeasible = true;
                return;
            }
            if ( FloatUtils::lt( ub, currentUb ) )
            {
                Tightening *t = new Tightening( variable, ub, Tightening::UB );
                tighteningQueue->push( t );
            }

            BackwardAnalysis_LOG( Stringf( "Computing lowerbound..." ).ascii() );
            gurobi->reset();
            gurobi->setNumberOfThreads( 1 );
            double lb = optimizeWithGurobi( *gurobi, MinOrMax::MIN,
                                            variableName, &infeasible );
            BackwardAnalysis_LOG( Stringf( "Lowerbound computed %f -> %f",
                                       currentLb, lb ).ascii() );
            if ( FloatUtils::gt( lb, currentUb ) )
            {
                BackwardAnalysis_LOG( Stringf( "Found invalid bound! lb: %u, ub: %u",
                                               lb, currentUb ).ascii() );
                infeasible = true;
                return;
            }
            if ( FloatUtils::gt( lb, currentLb ) )
            {
                Tightening *t = new Tightening( variable, lb, Tightening::LB );
                tighteningQueue->push( t );
            }

            numUnsolved -= 1;
            if ( numUnsolved.load() == 0 )
                shouldQuitSolving = true;
        }
        else
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        }
    }
}

void BackwardAnalysis::run( const Map<unsigned, Layer *> &layers )
{
    unsigned numberOfWorkers = Options::get()->getInt( Options::NUM_WORKERS );

    // Create a queue of free workers
    // When a worker is working, it is popped off the queue, when it is done, it
    // is added back to the queue.
    std::vector<GurobiWrapper *> freeSolvers;
    for ( unsigned i = 0; i < numberOfWorkers; ++i )
    {
        GurobiWrapper *gurobi = new GurobiWrapper();
        freeSolvers.push_back( gurobi );
    }

    TighteningQueryQueue *workload = new TighteningQueryQueue( 0 );
    std::atomic_bool shouldQuitSolving( false );
    std::atomic_uint numUnsolved( 0 );
    std::atomic_bool infeasible( false );
    TighteningQueue *tighteningQueue = new TighteningQueue( 0 );

    unsigned tighterBoundCounter = 0;

    struct timespec gurobiStart;
    (void) gurobiStart;
    struct timespec gurobiEnd;
    (void) gurobiEnd;

    gurobiStart = TimeUtils::sampleMicro();

    unsigned numberOfLayers = _layerOwner->getNumberOfLayers();
    for ( unsigned i = numberOfLayers - 1; i != 0; --i )
    {
        //BackwardAnalysis_LOG( Stringf( "Handling layer %u", i ).ascii() );
        shouldQuitSolving = false;

        ASSERT( numUnsolved.load() == 0 );
        ASSERT( infeasible.load() == false );

        Layer *layer = layers[i];
        layer->updateVariableToNeuron();

        unsigned numberOfSubProblems = 0;
        for ( unsigned i = 0; i < layer->getSize(); ++i )
        {
            if ( layer->neuronEliminated( i ) )
                continue;
            else
            {
                TighteningQuery *q = new TighteningQuery
                    ( i, layer->neuronToVariable( i ),
                      layer->getLb( i ),layer->getUb( i ) );
                workload->push( q );
                ++numberOfSubProblems;
            }
        }

        numUnsolved = numberOfSubProblems;
        unsigned numberOfWorkersThisLayer = std::min( numberOfWorkers,
                                                      numberOfSubProblems );

        //if ( layer->getLayerType() == Layer::LEAKY_RELU )
        //{
        //    printf("leaky relu layer\n" );
        //}

        // Spawn threads and start solving
        std::list<std::thread> threads;
        for ( unsigned threadId = 0; threadId < numberOfWorkersThisLayer;
              ++threadId )
        {
            _layerOwner->storeBoundsIntoOther( _lpFormulators[threadId]->_layerOwner );
            threads.push_back( std::thread( optimizeBounds, workload,
                                            freeSolvers[threadId],
                                            _lpFormulators[threadId],
                                            layer->getLayerIndex(),
                                            std::ref( shouldQuitSolving ),
                                            std::ref( numUnsolved ),
                                            std::ref( infeasible ),
                                            tighteningQueue,
                                            threadId ) );
        }

        for ( auto &thread : threads )
            thread.join();

        if ( infeasible )
        {
            throw InfeasibleQueryException();
        }

        Tightening *t = NULL;
        while ( tighteningQueue->pop( t ) )
        {
            unsigned index = layer->variableToNeuron( t->_variable );
            if ( t->_type == Tightening::LB )
                layer->setLb( index, t->_value );
            else
                layer->setUb( index, t->_value );
            _layerOwner->receiveTighterBound( *t );
            ++tighterBoundCounter;
            delete t;
        }

        //printf( "Number of tighter bounds found by Gurobi: %u.\n",
        //        tighterBoundCounter );
    }

    gurobiEnd = TimeUtils::sampleMicro();

    //printf( "Number of tighter bounds found by Gurobi: %u.\n",
    //      tighterBoundCounter );
    printf( "Seconds spent Gurobiing: %llu\n", TimeUtils::timePassed( gurobiStart, gurobiEnd ) / 1000000 );

    for ( unsigned i = 0; i < numberOfWorkers; ++i )
    {
        delete freeSolvers[i];
    }

    delete workload;
    delete tighteningQueue;
}

} // namespace NLR
