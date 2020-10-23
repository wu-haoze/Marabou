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

    // Obtain the current state of the engine
    _initialState = std::make_shared<EngineState>();
    _engine->storeState( *_initialState, true );
}

static void dump( String queryId, PiecewiseLinearCaseSplit &split, bool holds )
{
    String dumpFilePath;
    if ( holds )
        dumpFilePath = Stringf( "./dump/") + queryId + Stringf(".hold");
    else
        dumpFilePath = Stringf( "./dump/") + queryId + Stringf(".nothold");
    File summaryFile( dumpFilePath );
    summaryFile.open( File::MODE_WRITE_TRUNCATE );

    for ( const auto &bound : split.getBoundTightenings() )
    {
        summaryFile.write( Stringf( "x%u %s %f\n",
                                    bound._variable, bound._type == Tightening::LB ? ">=" : "<=", bound._value ) );
    }
}

static unsigned getDepth( String queryId )
{
    unsigned depth = 0;
    for ( unsigned i = 0; i < queryId.length(); i++ ){
        if ( queryId[i] == '-' ) {
            depth++;
        }
    }
    return depth;
}

void DnCWorker::run( unsigned depth )
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
            if ( !checkInvariant( *split, timeoutInSeconds ) )
            {
                if ( getDepth( queryId ) < depth )
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
                }
                *_numUnsolvedSubQueries -= 1;
                std::cout << queryId.ascii()<< " "<< "pre-condition might not hold" << std::endl;
                if ( timeoutInSeconds != 0 )
                    dump( queryId, *split, false );
                delete subQuery;
            } else
            {
                // pre-condition holds
                std::cout << queryId.ascii()<< " "<< "pre-condition holds" << std::endl;
                if ( timeoutInSeconds != 0 )
                    dump( queryId, *split, true );
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

bool DnCWorker::checkInvariant( const PiecewiseLinearCaseSplit &split, unsigned timeoutInSeconds )
{
    EngineState *engineState = new EngineState();
    _engine->storeState( *engineState, true );

    Vector<unsigned> nextStateVariables = _engine->getInputQuery()->getNextStateVariables();

    List<Tightening> bounds = split.getBoundTightenings();
    for ( auto const &bound : bounds )
    {
        std::cout << "\n\ninput variable range:" << std::endl;
        split.dump();
        unsigned var = bound._variable;
        double value = bound._value;
        auto type = bound._type;
        unsigned nextVar = nextStateVariables[var];

        for ( unsigned i = 0; i < _transitionSystems.size(); ++i )
        {
            PiecewiseLinearCaseSplit newSplit = _transitionSystems[i];
            newSplit.storeBoundTightening( Tightening( nextVar, value + ( type==Tightening::LB ? 0 : 0),
                                                       type==Tightening::LB ? Tightening::UB : Tightening::LB ) );

            Statistics *statistics = new Statistics();
            _engine->resetStatistics( *statistics );
            _engine->clearViolatedPLConstraints();
            _engine->resetSmtCore();
            _engine->resetExitCode();
            _engine->resetBoundTighteners();
            _engine->restoreState( *engineState );

            std::cout << "Next state variable and transition system:" << std::endl;
            newSplit.dump();

            _engine->applySplit( newSplit );

            //std::cout << "Checking a bound\n";

            _engine->solve( timeoutInSeconds );
            Engine::ExitCode result = _engine->getExitCode();

            if ( result != Engine::UNSAT )
            {
                if ( _engine->getExitCode() == Engine::SAT )
                    {
                double inputs[_engine->getInputQuery()->getNumInputVariables()];
                double outputs[_engine->getInputQuery()->getNumOutputVariables()];

                _engine->extractSolution( *(_engine->getInputQuery()) );
                printf( "Input assignment:\n" );
                for ( unsigned i = 0; i < _engine->getInputQuery()->getNumInputVariables(); ++i )
                {
                    printf( "\tx%u = %lf\n", i, _engine->getInputQuery()->getSolutionValue( _engine->getInputQuery()->inputVariableByIndex( i ) ) );
                    inputs[i] = _engine->getInputQuery()->getSolutionValue( _engine->getInputQuery()->inputVariableByIndex( i ) );
                }

                _engine->getInputQuery()->getNetworkLevelReasoner()->evaluate( inputs, outputs );

                printf( "\n" );
                printf( "Output:\n" );
                for ( unsigned i = 0; i < _engine->getInputQuery()->getNumOutputVariables(); ++i )
                    printf( "\ty%u = %lf\n", i, outputs[i] );

                printf( "Next state assignment:\n" );
                for ( unsigned i = 0; i < _engine->getInputQuery()->getNumNextStateVariables(); ++i )
                    printf( "\tx_next%u = %lf\n", i, _engine->getInputQuery()->getSolutionValue( _engine->getInputQuery()->nextStateVariableByIndex( i ) ) );

            }

                return false;
            }
            else{
                printf("UNSAT\n");
            }
        }
    }
    return true;
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
