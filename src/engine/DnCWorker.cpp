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
#include "LargestIntervalDivider.h"
#include "MarabouError.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"
#include "QueryDivider.h"
#include "ReluDivider.h"
#include "SubQuery.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <thread>

DnCWorker::DnCWorker( WorkerQueue *workload, std::shared_ptr<IEngine> engine,
                      std::atomic_uint &numUnsolvedSubQueries,
                      std::atomic_bool &shouldQuitSolving,
                      unsigned threadId, unsigned onlineDivides,
                      float timeoutFactor, DivideStrategy divideStrategy,
                      unsigned maxDepth, String summaryFile )
    : _workload( workload )
    , _engine( engine )
    , _numUnsolvedSubQueries( &numUnsolvedSubQueries )
    , _shouldQuitSolving( &shouldQuitSolving )
    , _threadId( threadId )
    , _onlineDivides( onlineDivides )
    , _timeoutFactor( timeoutFactor )
    , _maxDepth( maxDepth )
{
    _summaryFile = Stringf( "%s.log.%u", summaryFile.ascii(), threadId );
    std::ofstream ofs (_summaryFile.ascii(), std::ofstream::out);
    ofs << Stringf("Worker %u initiated!\n", threadId).ascii();
    ofs.close();

    setQueryDivider( divideStrategy );

    // Obtain the current state of the engine
    _initialState = std::make_shared<EngineState>();
    _engine->storeState( *_initialState, true );

}

void DnCWorker::setQueryDivider( DivideStrategy divideStrategy )
{
    // For now, there is only one strategy
    if ( divideStrategy == DivideStrategy::LargestInterval )
    {
        const List<unsigned> &inputVariables = _engine->getInputVariables();
        _queryDivider = std::unique_ptr<LargestIntervalDivider>
            ( new LargestIntervalDivider( inputVariables ) );
    }
    else
    {
        _queryDivider = std::unique_ptr<QueryDivider>
            ( new ReluDivider( _engine, _summaryFile ) );
    }
}

void DnCWorker::popOneSubQueryAndSolve( bool restoreTreeStates )
{
    std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
    ofs << "\nPoping one subquery and solve\n";
    ofs.close();

    SubQuery *subQuery = NULL;
    // Boost queue stores the next element into the passed-in pointer
    // and returns true if the pop is successful (aka, the queue is not empty
    // in most cases)
    if ( _workload->pop( subQuery ) )
    {
        std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
        ofs << "\nSubquery poped!\n";

        String queryId = subQuery->_queryId;

        ofs << Stringf( "Id: %s\n", queryId.ascii() ).ascii();

        auto split = std::move( subQuery->_split );
        std::unique_ptr<SmtState> smtState = nullptr;
        if ( restoreTreeStates && subQuery->_smtState )
            smtState = std::move( subQuery->_smtState );
        unsigned timeoutInSeconds = subQuery->_timeoutInSeconds;
        if ( queryId.tokenize( "-" ).size() >= _maxDepth )
            timeoutInSeconds = 0;

        // Reset the engine state
        _engine->restoreState( *_initialState );
        _engine->reset();

        // TODO: each worker is going to keep a map from *CaseSplit to an
        // object of class DnCStatistics, which contains some basic
        // statistics. The maps are owned by the DnCManager.

        ofs << "Applying split\n";

        String outSplit = "";
        split->dump( outSplit );
        ofs << outSplit.ascii() << "\n";

        // Apply the split and solve
        _engine->applySplit( *split );
        ofs << "Split applied, start propagating\n";
        _engine->propagate();
        ofs << "Propagated\n";

        bool fullSolveNeeded = true;
        if ( restoreTreeStates && smtState )
            fullSolveNeeded = _engine->restoreSmtState( *smtState );

        Engine::ExitCode result;
        if ( fullSolveNeeded )
        {
            ofs << "Start solving\n";
            _engine->solve( timeoutInSeconds );
            result = _engine->getExitCode();
        } else
        {
            // UNSAT is found when replaying stack-entries
            result = Engine::UNSAT;
        }
        ofs.close();

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


            std::vector<std::unique_ptr<SmtState>> newSmtStates;
            if ( restoreTreeStates )
            {
                for ( unsigned i = 0; i < pow( 2, _onlineDivides ); ++i )
                {
                    newSmtStates.push_back( std::unique_ptr<SmtState>( new SmtState() ) );
                    _engine->storeSmtState( *( newSmtStates[i] ) );
                }
            }

            _engine->reset();
            _engine->restoreState( *_initialState );
            _queryDivider->createSubQueries( pow( 2, _onlineDivides ),
                                             queryId, *split,
                                             (unsigned)timeoutInSeconds *
                                             _timeoutFactor, subQueries );

            std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
            ofs << "\nQuit from queryDivider!\n";
            ofs.close();

            unsigned i = 0;
            for ( auto &newSubQuery : subQueries )
            {
                // Store the SmtCore state
                if ( restoreTreeStates )
                {
                    newSubQuery->_smtState = std::move( newSmtStates[i] );
                }
                if ( !_workload->push( std::move( newSubQuery ) ) )
                {
                    throw MarabouError( MarabouError::UNSUCCESSFUL_QUEUE_PUSH );
                }
                std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
                ofs << "newSubquery Pushed!\n";
                ofs.close();

                *_numUnsolvedSubQueries += 1;
            }
            std::ofstream ofs1 (_summaryFile.ascii(), std::ofstream::app);
            ofs1 << "\nall newSubquery Pushed!\n";

            *_numUnsolvedSubQueries -= 1;
            delete subQuery;
            std::ofstream ofs2 (_summaryFile.ascii(), std::ofstream::app);
            ofs2 << "old subQuery deleted!\n";
            ofs2.close();

            _engine->reset();
            _engine->restoreState( *_initialState );

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

void DnCWorker::printProgress( String queryId, IEngine::ExitCode result ) const
{
    std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
    ofs << Stringf( "Worker %d: Query %s %s, %d tasks remaining\n", _threadId,
                    queryId.ascii(), exitCodeToString( result ).ascii(),
                    _numUnsolvedSubQueries->load() ).ascii();
    ofs.close();

    printf( "Worker %d: Query %s %s, %d tasks remaining\n", _threadId,
            queryId.ascii(), exitCodeToString( result ).ascii(),
            _numUnsolvedSubQueries->load() );
}

String DnCWorker::exitCodeToString( IEngine::ExitCode result )
{
    switch ( result )
    {
    case IEngine::UNSAT:
        return "UNSAT";
    case IEngine::SAT:
        return "SAT";
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
