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

#include "Debug.h"
#include "DivideStrategy.h"
#include "DnCWorker.h"
#include "IEngine.h"
#include "EngineState.h"
#include "GlobalConfiguration.h"
#include "LargestIntervalDivider.h"
#include "MarabouError.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"
#include "SubQuery.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

DnCWorker::DnCWorker( WorkerQueue *workload, std::shared_ptr<IEngine> engine,
                      std::atomic_uint &numUnsolvedSubQueries,
                      std::atomic_bool &shouldQuitSolving,
                      unsigned threadId, unsigned onlineDivides,
                      float timeoutFactor, DivideStrategy divideStrategy )
    : _workload( workload )
    , _engine( engine )
    , _numUnsolvedSubQueries( &numUnsolvedSubQueries )
    , _shouldQuitSolving( &shouldQuitSolving )
    , _threadId( threadId )
    , _onlineDivides( onlineDivides )
    , _timeoutFactor( timeoutFactor )
{
    setQueryDivider( divideStrategy );

    // Obtain the current state of the engine
    _initialState = std::make_shared<EngineState>();
    _engine->storeState( *_initialState, true );
}

void DnCWorker::setQueryDivider( DivideStrategy divideStrategy )
{
    // For now, there is only one strategy
    ASSERT( divideStrategy == DivideStrategy::LargestInterval );
    if ( divideStrategy == DivideStrategy::LargestInterval )
    {
        const List<unsigned> &inputVariables = _engine->getInputVariables();
        _queryDivider = std::unique_ptr<LargestIntervalDivider>
            ( new LargestIntervalDivider( inputVariables ) );
    }
}

void DnCWorker::popOneSubQueryAndSolve()
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

        // Reset the engine state
        _engine->restoreState( *_initialState );
        _engine->reset();

        // TODO: each worker is going to keep a map from *CaseSplit to an
        // object of class DnCStatistics, which contains some basic
        // statistics. The maps are owned by the DnCManager.

        // Apply the split and solve
        _engine->applySplit( *split );
        _engine->solve( timeoutInSeconds );

        IEngine::ExitCode result = _engine->getExitCode();
        printProgress( queryId, result );
        // Switch on the result
        if ( result == IEngine::UNSAT )
        {
            // If UNSAT, continue to solve
            *_numUnsolvedSubQueries -= 1;
            if ( _numUnsolvedSubQueries->load() == 0 )
                *_shouldQuitSolving = true;
            delete subQuery;
        }
        else if ( result == IEngine::TIMEOUT )
        {
            // If TIMEOUT, split the current input region and add the
            // new subQueries to the current queue
            SubQueries subQueries;
            _queryDivider->createSubQueries( pow( 2, _onlineDivides ),
                                             queryId, *split,
                                             (unsigned)timeoutInSeconds *
                                             _timeoutFactor, subQueries );
            for ( auto &newSubQuery : subQueries )
            {
                if ( !_workload->push( std::move( newSubQuery ) ) )
                {
                    throw MarabouError( MarabouError::UNSUCCESSFUL_QUEUE_PUSH );
                }

                *_numUnsolvedSubQueries += 1;
            }
            *_numUnsolvedSubQueries -= 1;
            delete subQuery;
        }
        else if ( result == IEngine::QUIT_REQUESTED )
        {
            // If engine was asked to quit, quit
            std::cout << "Quit requested by manager!" << std::endl;
            delete subQuery;
            ASSERT( _shouldQuitSolving->load() );
        }
        else
        {
            // We must set the quit flag to true  if the result is not UNSAT or
            // TIMEOUT. This way, the DnCManager will kill all the DnCWorkers.

            *_shouldQuitSolving = true;
            if ( result == IEngine::SAT )
            {
                // case SAT
                *_numUnsolvedSubQueries -= 1;
                delete subQuery;
            }
            else if ( result == IEngine::ERROR )
            {
                // case ERROR
                std::cout << "Error!" << std::endl;
                delete subQuery;
            }
            else // result == IEngine::NOT_DONE
            {
                // case NOT_DONE
                ASSERT( false );
                std::cout << "Not done! This should not happen." << std::endl;
                delete subQuery;
            }
        }
    }
    else
    {
        // If the queue is empty but the pop fails, wait and retry
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

bool DnCWorker::volumeThresholdReached( PiecewiseLinearCaseSplit &split )
{
    Map<unsigned, double> lbs;
    Map<unsigned, double> ubs;
    for ( const auto&bound : split.getBoundTightenings() )
        if ( bound._type == Tightening::LB )
            lbs[bound._variable] = bound._value;
        else
            ubs[bound._variable] = bound._value;
    for ( unsigned i = 0; i < _engine->getInputQuery()->getInputVariables().size(); ++i )
        if ( ( ubs[i] - lbs[i] ) * GlobalConfiguration::INTERVAL_WIDTH_THRESHOLD >
             _engine->getFullInputRanges( i ) )
            return false;
    return true;
}

void DnCWorker::popOneHypercubeAndCheckRobustness( unsigned label, Hypercubes *unrobustRegions )
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
        auto targetsToCheck = subQuery->_targetsToCheck;

        List<unsigned> targetsYetToCheck = targetsToCheck;
        IEngine::ExitCode result = IEngine::NOT_DONE;

        for ( const auto& target : targetsToCheck )
        {
            // Reset the engine state
            _engine->restoreState( *_initialState );
            _engine->reset();

            // Apply the split and solve
            _engine->applySplit( *split );

            // Apply the output relation
            PiecewiseLinearCaseSplit outputRelation;
            unsigned targetVar = _engine->getInputQuery()->outputVariableByIndex( target );
            unsigned labelVar = _engine->getInputQuery()->outputVariableByIndex( label );
            Equation eq( Equation::EQ );
            eq.addAddend( 1, targetVar );
            eq.addAddend( -1, labelVar );
            eq.setScalar( 0 );
            outputRelation.addEquation( eq );
            _engine->applySplit( outputRelation );

            _engine->solve( timeoutInSeconds );
            result = _engine->getExitCode();
            if ( result == IEngine::UNSAT )
            {
                targetsYetToCheck.erase( result );
                continue;
            }
            else
            {
                break;
            }
        }

        printProgress( queryId, result );
        // Switch on the result
        if ( result == IEngine::UNSAT  && targetsYetToCheck.empty() )
        {
            // If UNSAT, continue to solve
            *_numUnsolvedSubQueries -= 1;
            if ( _numUnsolvedSubQueries->load() == 0 )
                *_shouldQuitSolving = true;
            delete subQuery;
        }
        else if ( result == IEngine::SAT && volumeThresholdReached( *split ) )
        {
            // case SAT and volumeThreshold is reached
            // we add the hypercube to unrobustRegions
            auto splitDup = new PiecewiseLinearCaseSplit();
            *splitDup = *split;
            if ( !unrobustRegions->push( splitDup ) )
                throw MarabouError( MarabouError::UNSUCCESSFUL_QUEUE_PUSH,
                                    "DnCWorker::unrobustRegions" );
            *_numUnsolvedSubQueries -= 1;
            if ( _numUnsolvedSubQueries->load() == 0 )
                *_shouldQuitSolving = true;
            delete subQuery;
        }

        else if ( result == IEngine::TIMEOUT ||
		  result == IEngine::SAT )
        {
            // If TIMEOUT, split the current input region and add the
            // new subQueries to the current queue
            SubQueries subQueries;
            _queryDivider->createSubQueries( pow( 2, _onlineDivides ),
                                             queryId, *split,
                                             (unsigned)timeoutInSeconds *
                                             _timeoutFactor, subQueries );
            for ( auto &newSubQuery : subQueries )
            {
                newSubQuery->_targetsToCheck = targetsYetToCheck;
                if ( !_workload->push( std::move( newSubQuery ) ) )
                {
                    throw MarabouError( MarabouError::UNSUCCESSFUL_QUEUE_PUSH );
                }

                *_numUnsolvedSubQueries += 1;
            }
            *_numUnsolvedSubQueries -= 1;
            delete subQuery;
        }
        else if ( result == IEngine::QUIT_REQUESTED )
        {
            // If engine was asked to quit, quit
            std::cout << "Quit requested by manager!" << std::endl;
            delete subQuery;
            ASSERT( _shouldQuitSolving->load() );
        }
        else
        {
            // We must set the quit flag to true  if the result is not UNSAT or
            // TIMEOUT. This way, the DnCManager will kill all the DnCWorkers.

            *_shouldQuitSolving = true;
            if ( result == IEngine::ERROR )
            {
                // case ERROR
                std::cout << "Error!" << std::endl;
                delete subQuery;
            }
            else // result == IEngine::NOT_DONE
            {
                // case NOT_DONE
                ASSERT( false );
                std::cout << "Not done! This should not happen." << std::endl;
                delete subQuery;
            }
        }
    }
    else
    {
        // If the queue is empty but the pop fails, wait and retry
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void DnCWorker::printProgress( String queryId, IEngine::ExitCode result ) const
{
    printf( "Worker %d: Query %s %s, %d tasks remaining\n", _threadId,
            queryId.ascii(), exitCodeToString( result ).ascii(),
            _numUnsolvedSubQueries->load() );
}

String DnCWorker::exitCodeToString( IEngine::ExitCode result )
{
    switch ( result )
    {
    case IEngine::UNSAT:
        return "unsat";
    case IEngine::SAT:
        return "sat";
    case IEngine::ERROR:
        return "ERROR";
    case IEngine::TIMEOUT:
        return "TIMEOUT";
    case IEngine::QUIT_REQUESTED:
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
