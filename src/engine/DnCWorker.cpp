/*********************                                                        */
/*! \file DnCWorker.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "ActivationPatternDivider.h"
#include "Debug.h"
#include "DivideStrategy.h"
#include "DnCWorker.h"
#include "Engine.h"
#include "EngineState.h"
#include "LargestIntervalDivider.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"
#include "SubQuery.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

DnCWorker::DnCWorker( WorkerQueue *workload, std::shared_ptr<Engine> engine,
                      std::atomic_uint &numUnsolvedSubQueries,
                      std::atomic_bool &shouldQuitSolving,
                      unsigned threadId, unsigned onlineDivides,
                      float timeoutFactor, DivideStrategy divideStrategy,
                      unsigned pointsPerSegment, unsigned numberOfSegments )
    : _workload( workload )
    , _engine( engine )
    , _numUnsolvedSubQueries( &numUnsolvedSubQueries )
    , _shouldQuitSolving( &shouldQuitSolving )
    , _threadId( threadId )
    , _onlineDivides( onlineDivides )
    , _timeoutFactor( timeoutFactor )
    , _invariants ( )
    , _postCondition ( NULL )
{
    if ( divideStrategy == DivideStrategy::LargestInterval )
    {
        const List<unsigned> &inputVariables = _engine->getInputVariables();
        _queryDivider = std::unique_ptr<LargestIntervalDivider>
            ( new LargestIntervalDivider( inputVariables ) );
    } else if (divideStrategy == DivideStrategy::ActivationVariance )
    {
        const List<unsigned> &inputVariables = _engine->getInputVariables();
        NetworkLevelReasoner *networkLevelReasoner = _engine->getInputQuery()->
            getNetworkLevelReasoner();
        _queryDivider = std::unique_ptr<ActivationPatternDivider>
            ( new ActivationPatternDivider( inputVariables,
                                            networkLevelReasoner,
                                            numberOfSegments,
                                            pointsPerSegment ) );
    }

    // Obtain the current state of the engine
    _initialState = std::make_shared<EngineState>();
    _engine->storeState( *_initialState, true );
}

void DnCWorker::run()
{
    while ( _numUnsolvedSubQueries->load() > 0 )
    {
        SubQuery *subQuery = NULL;
        // Boost queue stores the next element into the passed-in pointer
        // and returns true if the pop is successful (aka, the queue is not empty
        // in most cases)
        if ( _workload->pop( subQuery ) )
        {
            String queryId = subQuery->_queryId;
            auto split = std::move( subQuery->_split );
            unsigned timeoutInSeconds = subQuery->_timeoutInSeconds;

            // Create a new statistics object for each subQuery
            Statistics *statistics = new Statistics();
            _engine->resetStatistics( *statistics );
            // Reset the engine state
            _engine->restoreState( *_initialState );
            _engine->clearViolatedPLConstraints();
            _engine->resetSmtCore();
            _engine->resetBoundTighteners();
            _engine->resetExitCode();
            // TODO: each worker is going to keep a map from *CaseSplit to an
            // object of class DnCStatistics, which contains some basic
            // statistics. The maps are owned by the DnCManager.

            // Apply the split and solve
            _engine->applySplit( *split );
            if ( !checkInvariants() )
            {
                // Split the current input region and add the
                // new subQueries to the current queue
                SubQueries subQueries;
                _queryDivider->createSubQueries( pow( 2, _onlineDivides ),
                                                 queryId, *split,
                                                 (unsigned) timeoutInSeconds *
                                                 _timeoutFactor, subQueries );
                for ( auto &newSubQuery : subQueries )
                    {
                        if ( !_workload->push( std::move( newSubQuery ) ) )
                            {
                                ASSERT( false );
                            }

                        *_numUnsolvedSubQueries += 1;
                    }
                *_numUnsolvedSubQueries -= 1;
                std::cout << "pre-condition might not hold" << std::endl;
                split->dump();
                delete subQuery;
            } else
            {
                // pre-condition holds
                std::cout << "pre-condition holds" << std::endl;
                split->dump();
                *_numUnsolvedSubQueries -= 1;
            }
            if ( _engine->getExitCode() == Engine::QUIT_REQUESTED )
                return;
        }
        else
        {
            // If the queue is empty but the pop fails, wait and retry
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        }
    }
}

void DnCWorker::printProgress( String queryId, Engine::ExitCode result ) const
{
    printf( "Worker %d: Query %s %s, %d tasks remaining\n", _threadId,
            queryId.ascii(), exitCodeToString( result ).ascii(),
            _numUnsolvedSubQueries->load() );
}

bool DnCWorker::checkInvariants()
{
    for ( auto &invariant : _invariants )
        if ( checkInvariant( invariant ) )
            return true;
    return false;
}

bool DnCWorker::checkInvariant( Invariant& invariant )
{
    EngineState *engineState = new EngineState();
    _engine->storeState( *engineState, true );
    List<PiecewiseLinearCaseSplit> activationPatterns =
        invariant.getActivationPatterns( _engine->_symbolicBoundTightener );
    for ( auto& activation : activationPatterns )
    {
        Statistics *statistics = new Statistics();
        _engine->resetStatistics( *statistics );
        _engine->clearViolatedPLConstraints();
        _engine->resetSmtCore();
        _engine->resetExitCode();
        _engine->resetBoundTighteners();
        _engine->restoreState( *engineState );

        _engine->applySplit( activation );
        _engine->solve( );
        std::cout << "Checked a pattern" << std::endl;
        Engine::ExitCode result = _engine->getExitCode();
        if ( result != Engine::UNSAT )
            return false;
    }
    return true;
}

void DnCWorker::setPostCondition( PiecewiseLinearCaseSplit *postCondition )
{
    _postCondition = postCondition;
}

void DnCWorker::addInvariant( Invariant &invariant )
{
    _invariants.append( invariant );
}


String DnCWorker::exitCodeToString( Engine::ExitCode result )
{
    switch ( result )
    {
    case Engine::UNSAT:
        return "UNSAT";
    case Engine::SAT:
        return "SAT";
    case Engine::ERROR:
        return "ERROR";
    case Engine::TIMEOUT:
        return "TIMEOUT";
    case Engine::QUIT_REQUESTED:
        return "QUIT_REQUESTED";
    default:
        ASSERT( false );
        return "UNKNOWN (this should never happen)";
    }
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
