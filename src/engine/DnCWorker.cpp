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
#include "File.h"
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

static void dump( String queryId, PiecewiseLinearCaseSplit &split, const std::vector<bool> isActive, bool holds )
{
    String dumpFilePath;
    if ( holds )
        dumpFilePath = Stringf( "/home/haozewu/Projects/NASA/InductiveReasoning/dump/") + queryId + Stringf(".hold");
    else
        dumpFilePath = Stringf( "/home/haozewu/Projects/NASA/InductiveReasoning/dump/") + queryId + Stringf(".nothold");
    File summaryFile( dumpFilePath );
    summaryFile.open( File::MODE_WRITE_TRUNCATE );

    for ( const auto &b : isActive )
    {
        if ( b )
            summaryFile.write( "1" );
        else
            summaryFile.write( "0" );
    }
    summaryFile.write( "\n" );

    if ( holds )
        summaryFile.write( "Hold\n" );
    else
        summaryFile.write( "Not Hold\n" );
    summaryFile.write( "\tBounds are:\n" );
    for ( const auto &bound : split.getBoundTightenings() )
    {
        summaryFile.write( Stringf( "\t\tVariable: %u. New bound: %.2lf. Bound type: %s\n",
                                    bound._variable, bound._value, bound._type == Tightening::LB ? "lower" : "upper" ) );
    }
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
            auto activations = std::move( subQuery->_activations );

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
            if ( !checkInvariants( timeoutInSeconds, *activations  ) )
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
                    auto newActivations = std::unique_ptr<std::vector<bool>>( new std::vector<bool>() );
                    for ( const auto &b : *activations )
                        newActivations->push_back( b );
                    newSubQuery->_activations = std::move( newActivations );
                    if ( !_workload->push( std::move( newSubQuery ) ) )
                    {
                        ASSERT( false );
                    }

                    *_numUnsolvedSubQueries += 1;
                }
                *_numUnsolvedSubQueries -= 1;
                std::cout << "pre-condition might not hold" << std::endl;
                dump( queryId, *split, *activations, false );
                delete subQuery;
            } else
            {
                // pre-condition holds
                std::cout << "pre-condition holds" << std::endl;
                dump( queryId, *split, *activations, true );
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

bool DnCWorker::checkInvariants( unsigned timeoutInSeconds,
                                 std::vector<bool> &activations )
{
    for ( auto &invariant : _invariants )
        if ( checkInvariant( invariant, timeoutInSeconds, activations ) )
            return true;
    return false;
}

bool DnCWorker::checkInvariant( Invariant& invariant, unsigned timeoutInSeconds,
                                std::vector<bool> &isActive )
{
    EngineState *engineState = new EngineState();
    _engine->storeState( *engineState, true );
    List<PiecewiseLinearCaseSplit> activationPatterns =
        invariant.getActivationPatterns( _engine->_symbolicBoundTightener );
    unsigned i = 0;
    for ( auto& activation : activationPatterns )
    {
        if ( !(isActive[i]) )
        {
            Statistics *statistics = new Statistics();
            _engine->resetStatistics( *statistics );
            _engine->clearViolatedPLConstraints();
            _engine->resetSmtCore();
            _engine->resetExitCode();
            _engine->resetBoundTighteners();
            _engine->restoreState( *engineState );

            _engine->applySplit( activation );
            std::cout << "Checking a pattern\n";
            _engine->solve( timeoutInSeconds );
            Engine::ExitCode result = _engine->getExitCode();
            if ( result != Engine::UNSAT )
                return false;
            else {
                std::cout << "Fixed a relu!\n";
                isActive[i] = true;
            }
        }
        ++i;
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
