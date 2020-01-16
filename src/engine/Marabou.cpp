/*********************                                                        */
/*! \file Marabou.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief [[ Add one-line brief description here ]]
 **
 ** [[ Add lengthier description here ]]
 **/

#include "AcasParser.h"
#include "File.h"
#include "LargestIntervalDivider.h"
#include "FixedReluParser.h"
#include "MStringf.h"
#include "LookAheadPreprocessor.h"
#include "Marabou.h"
#include "MarabouError.h"
#include "Options.h"
#include "PropertyParser.h"
#include "MarabouError.h"
#include "QueryLoader.h"
#include "QueryDivider.h"
#include "ReluDivider.h"

#include "thunk/thunk.hh"
#include "thunk/thunk_writer.hh"
#include "thunk/ggutils.hh"
#include "util/util.hh"

#include <fstream>

#ifdef _WIN32
#undef ERROR
#endif
#define PROP_SUFFIX ".prop"
#define THUNK_SUFFIX  ".thunk"


Marabou::Marabou( unsigned verbosity, bool ggOutput )
    : _acasParser( NULL )
    , _engine( verbosity )
    , _ggOutput( ggOutput )
{
}

Marabou::~Marabou()
{
    if ( _acasParser )
    {
        delete _acasParser;
        _acasParser = NULL;
    }
}

void Marabou::run()
{
    struct timespec start = TimeUtils::sampleMicro();

    prepareInputQuery();

    unsigned initialDivides = Options::get()->getInt( Options::NUM_INITIAL_DIVIDES );
    if ( initialDivides > 0 )
    {
        std::cerr << "Divide initially" << std::endl;
        _engine.processInputQuery( _inputQuery );
        SubQueries splits = split( initialDivides );
        dumpSubQueriesAsThunks( splits );
    }
    else
    {
        solveQuery();

        struct timespec end = TimeUtils::sampleMicro();

        unsigned long long totalElapsed = TimeUtils::timePassed( start, end );
        displayResults( totalElapsed );
    }
}

void Marabou::prepareInputQuery()
{
    String inputQueryFilePath = Options::get()->getString( Options::INPUT_QUERY_FILE_PATH );
    if ( inputQueryFilePath.length() > 0 )
    {
        /*
          Step 1: extract the query
        */
        if ( !File::exists( inputQueryFilePath ) )
        {
            printf( "Error: the specified inputQuery file (%s) doesn't exist!\n", inputQueryFilePath.ascii() );
            throw MarabouError( MarabouError::FILE_DOESNT_EXIST, inputQueryFilePath.ascii() );
        }

        //printf( "InputQuery: %s\n", inputQueryFilePath.ascii() );
        _inputQuery = QueryLoader::loadQuery(inputQueryFilePath);
    }
    else
    {
        /*
          Step 1: extract the network
        */
        String networkFilePath = Options::get()->getString( Options::INPUT_FILE_PATH );
        if ( !File::exists( networkFilePath ) )
        {
            printf( "Error: the specified network file (%s) doesn't exist!\n", networkFilePath.ascii() );
            throw MarabouError( MarabouError::FILE_DOESNT_EXIST, networkFilePath.ascii() );
        }
        //printf( "Network: %s\n", networkFilePath.ascii() );

        // For now, assume the network is given in ACAS format
        _acasParser = new AcasParser( networkFilePath );
        _acasParser->generateQuery( _inputQuery );

        /*
          Step 2: extract the property in question
        */
        String propertyFilePath = Options::get()->getString( Options::PROPERTY_FILE_PATH );
        if ( propertyFilePath != "" )
        {
            //printf( "Property: %s\n", propertyFilePath.ascii() );
            PropertyParser().parse( propertyFilePath, _inputQuery );
        }
        else
        {
            //printf( "Property: None\n" );
        }

        //printf( "\n" );
    }
}

void Marabou::solveQuery()
{
    if ( _engine.processInputQuery( _inputQuery ) &&
         lookAheadPreprocessing() && !Options::get()->
         getBool( Options::PREPROCESS_ONLY ) )
    {
        String fixedReluFilePath = Options::get()->getString( Options::PROPERTY_FILE_PATH );
        if ( fixedReluFilePath != "" )
        {
            Map<unsigned, unsigned> idToPhase;
            //printf( "Fixed Relus: %s\n", fixedReluFilePath.ascii() );
            FixedReluParser().parse( fixedReluFilePath, idToPhase );
            _engine.applySplits( idToPhase );
        }

        _engine.storeState(_postReluFixState, true);

        BiasStrategy biasStrategy = setBiasStrategyFromOptions
            ( Options::get()->getString( Options::BIAS_STRATEGY ) );
        _engine.setBiasedPhases( Options::get()->getInt( Options::FOCUS_LAYER ),
                                 biasStrategy );
        _engine.solve( Options::get()->getInt( Options::TIMEOUT ) );
    }
    if ( _engine.getExitCode() == Engine::SAT )
        _engine.extractSolution( _inputQuery );
}

bool Marabou::lookAheadPreprocessing()
{
    bool feasible = true;
    if ( Options::get()->getBool( Options::LOOK_AHEAD_PREPROCESSING ) )
    {
        Map<unsigned, unsigned> idToPhase;

        struct timespec start = TimeUtils::sampleMicro();
        auto lookAheadPreprocessor = new LookAheadPreprocessor
            ( Options::get()->getInt( Options::NUM_WORKERS ),
              *_engine.getInputQuery() );
        feasible = lookAheadPreprocessor->run( idToPhase );
        struct timespec end = TimeUtils::sampleMicro();
        unsigned long long totalElapsed = TimeUtils::timePassed( start, end );
        String summaryFilePath = Options::get()->getString( Options::SUMMARY_FILE );
        if ( summaryFilePath != "" )
        {
            File summaryFile( summaryFilePath + ".preprocess" );
            summaryFile.open( File::MODE_WRITE_TRUNCATE );

            // Field #1: result
            summaryFile.write( ( feasible ? "UNKNOWN" : "UNSAT" ) );

            // Field #2: total elapsed time
            summaryFile.write( Stringf( " %u ", totalElapsed / 1000000 ) ); // In seconds

            // Field #3: number of fixed relus by look ahead preprocessing
            summaryFile.write( Stringf( "%u ", idToPhase.size() ) );
            summaryFile.write( "\n" );
        }
        if ( summaryFilePath != "" )
        {
            File fixedFile( summaryFilePath + ".fixed" );
            fixedFile.open( File::MODE_WRITE_TRUNCATE );

            for ( const auto entry : idToPhase )
            {
                fixedFile.write( Stringf( "%u %u\n", entry.first, entry.second ) );
            }
        }

        if ( feasible )
            _engine.applySplits( idToPhase );
        else
            // Solved by preprocessing, we are done!
            _engine._exitCode = Engine::UNSAT;
    }
    return feasible;
}

void Marabou::displayResults( unsigned long long microSecondsElapsed )
{
    Engine::ExitCode result = _engine.getExitCode();
    String resultString;
    std::ostringstream assignment;

    if ( result == Engine::UNSAT )
    {
        resultString = "UNSAT";
        printf( "UNSAT\n" );
        if ( _ggOutput )
            createEmptySubproblemOutputs();
    }
    else if ( result == Engine::SAT )
    {
        resultString = "SAT";
        printf( "SAT\n" );

        assignment << "Input assignment:\n";
        for ( unsigned i = 0; i < _inputQuery.getNumInputVariables(); ++i )
        {
            assignment << "\tx" << i << " = " << _inputQuery.getSolutionValue( _inputQuery.inputVariableByIndex( i ) ) << "\n";
        }

        assignment << "\nOutput assignment:\n";
        for ( unsigned i = 0; i < _inputQuery.getNumOutputVariables(); ++i )
        {
            assignment << "\ty" << i << " = " << _inputQuery.getSolutionValue( _inputQuery.outputVariableByIndex( i ) ) << "\n";
        }
        assignment << "\n";
        std::cout << assignment.str();
        if ( _ggOutput )
            createEmptySubproblemOutputs();
    }
    else if ( result == Engine::TIMEOUT )
    {
        resultString = "TIMEOUT";
        printf( "Timeout\n" );
        if ( _ggOutput )
        {
            _engine.reset();
            _engine.restoreState(_postReluFixState);
            SubQueries splits = split( Options::get()->getInt( Options::NUM_ONLINE_DIVIDES ) );
            dumpSubQueriesAsThunks( splits );
            return;
        }
    }
    else if ( result == Engine::ERROR )
    {
        resultString = "ERROR";
        printf( "Error\n" );
        if ( _ggOutput )
            createEmptySubproblemOutputs();
    }
    else
    {
        resultString = "UNKNOWN";
        printf( "UNKNOWN EXIT CODE! (this should not happen)" );
        if ( _ggOutput )
            createEmptySubproblemOutputs();
    }

    // Create a summary file, if requested
    String summaryFilePath = Options::get()->getString( Options::SUMMARY_FILE );
    if ( summaryFilePath != "" )
    {
        File summaryFile( summaryFilePath );
        summaryFile.open( File::MODE_WRITE_TRUNCATE );

        // Field #1: result
        summaryFile.write( resultString );

        // Field #2: total elapsed time
        summaryFile.write( Stringf( " %u ", microSecondsElapsed / 1000000 ) ); // In seconds

        // Field #3: number of visited tree states
        summaryFile.write( Stringf( "%u ",
                                    _engine.getStatistics()->getNumVisitedTreeStates() ) );

        summaryFile.write( "\n" );
        if ( assignment.str().size() > 0 )
        {
            summaryFile.write( assignment.str() );
        }
    }
}

SubQueries Marabou::split( unsigned divides )
{
    std::unique_ptr<QueryDivider> queryDivider = nullptr;
    const List<unsigned> inputVariables(_engine.getInputVariables());
    // Create a new case split
    QueryDivider::InputRegion initialRegion;
    InputQuery* inputQuery = _engine.getInputQuery();
    for (const auto& variable : inputVariables) {
        initialRegion._lowerBounds[variable] = inputQuery->getLowerBounds()[variable];
        initialRegion._upperBounds[variable] = inputQuery->getUpperBounds()[variable];
    }
    
    auto split = std::unique_ptr<PiecewiseLinearCaseSplit>(new PiecewiseLinearCaseSplit());
    
    // Add bound as equations for each input variable
    for (const auto& variable : inputVariables) {
        double lb = initialRegion._lowerBounds[variable];
        double ub = initialRegion._upperBounds[variable];
        split->storeBoundTightening(Tightening(variable, lb,
                                               Tightening::LB));
        split->storeBoundTightening(Tightening(variable, ub,
                                               Tightening::UB));
    }

    String queryId = Options::get()->getString(Options::QUERY_ID);
    SubQueries subQueries;

    if ( Options::get()->getString( Options::DIVIDE_STRATEGY ) == "largest-interval" )
    {
        queryDivider = std::unique_ptr<QueryDivider>(new LargestIntervalDivider(inputVariables));

    }
    else
    {
        queryDivider = std::unique_ptr<QueryDivider>(new ReluDivider(_engine));
    }
    queryDivider->createSubQueries(pow(2, divides), queryId,
                                   *split, 0, subQueries);

    return subQueries;
}

void Marabou::dumpSubQueriesAsThunks( const SubQueries &subQueries ) const
{
    // Get options
    const unsigned timeoutInSeconds = Options::get()->getInt( Options::TIMEOUT );
    const unsigned numOnlineDivides = Options::get()->getInt( Options::NUM_ONLINE_DIVIDES );
    const unsigned numInitialDivides = Options::get()->getInt( Options::NUM_INITIAL_DIVIDES );
    const String networkFilePath = Options::get()->getString( Options::INPUT_FILE_PATH );
    const String propertyFilePath = Options::get()->getString( Options::PROPERTY_FILE_PATH );
    const String summaryFilePath = Options::get()->getString( Options::SUMMARY_FILE );
    const String mergePath = Options::get()->getString( Options::MERGE_FILE );
    const String selfHash = Options::get()->getString( Options::SELF_HASH );
    const String currentQueryId = Options::get()->getString( Options::QUERY_ID );
    const String divideStrategy = Options::get()->getString( Options::DIVIDE_STRATEGY );
    const double timeoutFactor = Options::get()->getFloat( Options::TIMEOUT_FACTOR );

    assert(selfHash.length() > 0);
    assert(mergePath.length() > 0);
    assert(summaryFilePath.length() > 0);

    // Hash files
    const std::string mergeHash = gg::hash::file_force( mergePath.ascii() );
    const std::string networkFileHash = gg::hash::file_force( networkFilePath.ascii() );


    // Declare suffixes

    // Initialize merge thunk argument and dependency lists.
    std::vector<gg::thunk::Thunk::DataItem> thunkHashes;
    std::vector<std::string> mergeArguments;
    mergeArguments.push_back("merge");

    std::cout << "subqueries: " << subQueries.size() << std::endl;
    size_t n_subproblems = 1 << std::max(numOnlineDivides, numInitialDivides);

    auto subQueryPointer = subQueries.begin();
    for (size_t i = 0; i < n_subproblems; ++i)
    {
        const std::string queryId = currentQueryId.ascii() + std::string("-") + std::to_string(i + 1);
        const std::string propFilePath = queryId + PROP_SUFFIX;

        // Emit subproblem property file
        if (i < subQueries.size())
        {
            std::ifstream oldFile{ propertyFilePath.ascii() };
            std::ofstream propFile{ propFilePath };
            propFile << oldFile.rdbuf();
            const auto& split = (*subQueryPointer)->_split;
            auto bounds = split->getBoundTightenings();
            if ( divideStrategy == "largest-interval" )
            {
                for ( const auto bound : bounds )
                {
                    propFile << "x"
                             << bound._variable
                             << (bound._type == Tightening::LB ? " >= " : " <= ")
                             << bound._value
                             << "\n";
                }
            }
            else
            {
                for ( const auto& reluPhasePair : split->getReluPhases() )
                {
                    propFile << reluPhasePair.first()
                             << " "
                             << 2 - static_cast<unsigned>(reluPhasePair.second())
                             << "\n";
                }
            }
        }
        else
        {
            std::ofstream propFile{ propFilePath };
            propFile << "x0 <= 0\n";
            propFile << "x0 >= 1\n";
        }
        const std::string propHash = gg::hash::file_force( propFilePath );

        // List all potential output files
        std::vector<std::string> outputFileNames;
        outputFileNames.emplace_back(summaryFilePath.ascii());
        for (unsigned i = 1; i <= (1U << numOnlineDivides); ++i)
        {
            outputFileNames.push_back(queryId + "-" + std::to_string(i) + PROP_SUFFIX);
            outputFileNames.push_back(queryId + "-" + std::to_string(i) + THUNK_SUFFIX);
        }

        unsigned nextTimeout = numInitialDivides > 0
            ? timeoutInSeconds
            : static_cast<unsigned>(0.5 + timeoutInSeconds * timeoutFactor);
        if (nextTimeout > 890) {
            nextTimeout = 890;
        }

        // Construct thunk
        const gg::thunk::Thunk subproblemThunk{
            { selfHash.ascii(),
                {
                    "Marabou",
                    "--gg-output",
                    "--timeout",
                    std::to_string(nextTimeout),
                    "--timeout-factor",
                    std::to_string(timeoutFactor),
                    "--initial-divides",
                    "0",
                    "--num-online-divides",
                    std::to_string(numOnlineDivides),
                    "--summary-file",
                    summaryFilePath.ascii(),
                    "--merge-file",
                    gg::thunk::data_placeholder(mergeHash),
                    "--query-id",
                    queryId,
                    "--divide-strategy",
                    divideStrategy.ascii(),
                    "--self-hash",
                    selfHash.ascii(),
                    "--verbosity",
                    "0",
                    gg::thunk::data_placeholder(networkFileHash),
                    gg::thunk::data_placeholder(propHash),
                },
                {} },
            {
                { networkFileHash, "" },
                { propHash, "" },
            },
            {
                { selfHash.ascii(), "" },
                { mergeHash, "" },
            },
            outputFileNames
        };

        ThunkWriter::write( subproblemThunk, queryId + THUNK_SUFFIX );
        auto subProblemThunkHash = subproblemThunk.hash();
        thunkHashes.emplace_back( subProblemThunkHash, "" );
        mergeArguments.push_back( gg::thunk::data_placeholder( subProblemThunkHash ) );
        if (i < subQueries.size())
        {
            ++subQueryPointer;
        }
    }

    const gg::thunk::Thunk mergeThunk
    {
        { mergeHash, std::move(mergeArguments), {} },
        {},
        std::move(thunkHashes),
        { { mergeHash, "" } },
        { std::string("out") }
    };

    ThunkWriter::write( mergeThunk, summaryFilePath.ascii() );
}

void Marabou::createEmptySubproblemOutputs() const
{
    std::string queryId = std::string( Options::get()->getString( Options::QUERY_ID ).ascii() );
    const unsigned numOnlineDivides = Options::get()->getInt( Options::NUM_ONLINE_DIVIDES );
    for (unsigned i = 1; i <= (1U << numOnlineDivides); ++i) {
        std::ofstream prop{ queryId + "-" + std::to_string(i) + PROP_SUFFIX };
        prop << "NONE";
        std::ofstream thunk{ queryId + "-" + std::to_string(i) + THUNK_SUFFIX };
        thunk << "NONE";
    }
}

BiasStrategy Marabou::setBiasStrategyFromOptions( const String strategy )
{
    if ( strategy == "centroid" )
        return BiasStrategy::Centroid;
    else if ( strategy == "sampling" )
        return BiasStrategy::Sampling;
    else if ( strategy == "random" )
        return BiasStrategy::Random;
    else
        {
            printf ("Unknown bias strategy, using default (centroid).\n");
            return BiasStrategy::Centroid;
        }
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
