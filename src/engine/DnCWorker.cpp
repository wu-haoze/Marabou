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
#include "LookAheadDivider.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"
#include "ReluLookAheadDivider.h"
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
{
    if ( divideStrategy == DivideStrategy::LargestInterval )
    {
        const List<unsigned> inputVariables = _engine->getInputVariables();
        _queryDivider = std::unique_ptr<LargestIntervalDivider>
            ( new LargestIntervalDivider( inputVariables ) );
    } else if (divideStrategy == DivideStrategy::ActivationVariance )
    {
        const List<unsigned> inputVariables = _engine->getInputVariables();
        NetworkLevelReasoner *networkLevelReasoner = _engine->getInputQuery()->
            getNetworkLevelReasoner();
        _queryDivider = std::unique_ptr<ActivationPatternDivider>
            ( new ActivationPatternDivider( inputVariables,
                                            networkLevelReasoner,
                                            numberOfSegments,
                                            pointsPerSegment ) );
    }
    else if ( divideStrategy == DivideStrategy::LookAhead )
    {
        const List<unsigned> inputVariables = _engine->getInputVariables();
        _queryDivider = std::unique_ptr<LookAheadDivider>
            ( new LookAheadDivider( inputVariables, _engine ) );
    }
    else// if ( _divideStrategy == DivideStrategy::ReluLookAhead )
    {
        _queryDivider = std::unique_ptr<ReluLookAheadDivider>
            ( new ReluLookAheadDivider( _engine ) );
    }

    // Obtain the current state of the engine
    _initialState = std::make_shared<EngineState>();
    _engine->storeState( *_initialState, true );
}

void DnCWorker::run( bool performTreeStateRecovery )
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
            std::unique_ptr<SmtState> smtState = nullptr;
            if ( performTreeStateRecovery && subQuery->_smtState )
                smtState = std::move( subQuery->_smtState );
            unsigned timeoutInSeconds = subQuery->_timeoutInSeconds;

            // Create a new statistics object for each subQuery
            Statistics *statistics = new Statistics();
            _engine->resetStatistics( *statistics );

            // Reset the engine state
            _engine->clearViolatedPLConstraints();
            _engine->resetBoundTighteners();
            _engine->resetExitCode();

            // Restore the smtCore to where the parent query left
            _engine->resetSmtCore();
            // Apply the split and solve
            _engine->applySplit( *split );

            bool fullSolveNeeded = true;
            if ( performTreeStateRecovery && smtState )
                fullSolveNeeded = _engine->restoreSmtState( *smtState );
            // TODO: each worker is going to keep a map from *CaseSplit to an
            // object of class DnCStatistics, which contains some basic
            // statistics. The maps are owned by the DnCManager.

            Engine::ExitCode result;
            if ( fullSolveNeeded )
            {
                _engine->solve( timeoutInSeconds );
                result = _engine->getExitCode();
            } else
            {
                // UNSAT is found when replaying stack-entries
                result = Engine::UNSAT;
            }
            _engine->restoreState( *_initialState );

            printProgress( queryId, result );
            // Switch on the result
            if ( result == Engine::UNSAT )
            {
                // If UNSAT, continue to solve
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
            }
            else if ( result == Engine::TIMEOUT )
            {
                // If TIMEOUT, split the current input region and add the
                // new subQueries to the current queue
                SubQueries subQueries;
                _engine->restoreState( *_initialState );
                if ( performTreeStateRecovery && subQuery->_smtState )
                {
                    // Step 1: all implied valid splits at root
                    for ( auto &validSplit : smtState->_impliedValidSplitsAtRoot )
                        _engine->applySplit( validSplit );
                    _engine->propagate();
                }

                _queryDivider->createSubQueries( pow( 2, _onlineDivides ),
                                                 queryId, *split,
                                                 (unsigned) timeoutInSeconds *
                                                 _timeoutFactor, subQueries );
                for ( auto &newSubQuery : subQueries )
                {
                    // Store the SmtCore state
                    if ( performTreeStateRecovery )
                    {
                        auto newSmtState = std::unique_ptr<SmtState>
                            ( new SmtState() );
                        _engine->storeSmtState( *newSmtState );
                        newSubQuery->_smtState = std::move( newSmtState );
                    }
                    if ( !_workload->push( std::move( newSubQuery ) ) )
                    {
                        ASSERT( false );
                    }

                    *_numUnsolvedSubQueries += 1;
                }
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
            }
            else if ( result == Engine::SAT )
            {
                // If SAT, set the shouldQuitSolving flag to true, so that the
                // DnCManager will kill all the DnCWorkers
                *_shouldQuitSolving = true;
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
                return;
            }
            else if ( result == Engine::QUIT_REQUESTED )
            {
                // If engine was asked to quit, quit
                std::cout << "Quit requested by manager!" << std::endl;
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
                return;
            }
            else if ( result == Engine::ERROR )
            {
                // If ERROR, set the shouldQuitSolving flag to true and quit
                std::cout << "Error!" << std::endl;
                *_shouldQuitSolving = true;
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
                return;
            }
            else if ( result == Engine::NOT_DONE )
            {
                // If NOT_DONE, set the shouldQuitSolving flag to true and quit
                ASSERT( false );
                std::cout << "Not done! This should not happen." << std::endl;
                *_shouldQuitSolving = true;
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
                return;
            }
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
