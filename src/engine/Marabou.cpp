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
#include "MStringf.h"
#include "Marabou.h"
#include "MarabouError.h"
#include "Options.h"
#include "PropertyParser.h"
#include "MarabouError.h"
#include "QueryLoader.h"
#include "QueryDivider.h"

#include "thunk/thunk.hh"
#include "util/util.hh"

#ifdef _WIN32
#undef ERROR
#endif

Marabou::Marabou( unsigned verbosity )
    : _acasParser( NULL )
    , _engine( verbosity )
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
    solveQuery();

    struct timespec end = TimeUtils::sampleMicro();

    unsigned long long totalElapsed = TimeUtils::timePassed( start, end );
    displayResults( totalElapsed );
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

        printf( "InputQuery: %s\n", inputQueryFilePath.ascii() );
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
        printf( "Network: %s\n", networkFilePath.ascii() );

        // For now, assume the network is given in ACAS format
        _acasParser = new AcasParser( networkFilePath );
        _acasParser->generateQuery( _inputQuery );

        /*
          Step 2: extract the property in question
        */
        String propertyFilePath = Options::get()->getString( Options::PROPERTY_FILE_PATH );
        if ( propertyFilePath != "" )
        {
            printf( "Property: %s\n", propertyFilePath.ascii() );
            PropertyParser().parse( propertyFilePath, _inputQuery );
        }
        else
            printf( "Property: None\n" );

        printf( "\n" );
    }
}

void Marabou::solveQuery()
{
    unsigned timeoutInSeconds = Options::get()->getInt( Options::TIMEOUT );
    if ( _engine.processInputQuery( _inputQuery ) )
        _engine.solve( timeoutInSeconds );

    if ( _engine.getExitCode() == Engine::SAT )
        _engine.extractSolution( _inputQuery );

    if ( _engine.getExitCode() == Engine::TIMEOUT )
    {
	_engine.reset();
        const List<unsigned> inputVariables( _engine.getInputVariables() );
        std::unique_ptr<QueryDivider> queryDivider  = std::unique_ptr<QueryDivider>
        ( new LargestIntervalDivider( inputVariables ) );

        // Create a new case split
        QueryDivider::InputRegion initialRegion;
        InputQuery *inputQuery = _engine.getInputQuery();
        for ( const auto &variable : inputVariables )
        {
	    initialRegion._lowerBounds[variable] =
		inputQuery->getLowerBounds()[variable];
	    initialRegion._upperBounds[variable] =
		inputQuery->getUpperBounds()[variable];
        }

        auto split = std::unique_ptr<PiecewiseLinearCaseSplit>
	    ( new PiecewiseLinearCaseSplit() );

        // Add bound as equations for each input variable
        for ( const auto &variable : inputVariables )
        {
	    double lb = initialRegion._lowerBounds[variable];
	    double ub = initialRegion._upperBounds[variable];
	    split->storeBoundTightening( Tightening( variable, lb,
						     Tightening::LB ) );
	    split->storeBoundTightening( Tightening( variable, ub,
						     Tightening::UB ) );
        }
	unsigned numDivides = Options::get()->getInt( Options::NUM_ONLINE_DIVIDES );
	String queryId = Options::get()->getString( Options::QUERY_ID );
	SubQueries subQueries;
        queryDivider->createSubQueries( pow( 2, numDivides ), queryId,
                        *split, timeoutInSeconds, subQueries );
	for ( const auto &subQuery : subQueries )
	{
	    dumpSubQuery( *subQuery );
	}
    }
}

void Marabou::displayResults( unsigned long long microSecondsElapsed ) const
{
    Engine::ExitCode result = _engine.getExitCode();
    String resultString;

    if ( result == Engine::UNSAT )
    {
        resultString = "UNSAT";
        printf( "UNSAT\n" );
    }
    else if ( result == Engine::SAT )
    {
        resultString = "SAT";
        printf( "SAT\n" );

        printf( "Input assignment:\n" );
        for ( unsigned i = 0; i < _inputQuery.getNumInputVariables(); ++i )
            printf( "\tx%u = %lf\n", i, _inputQuery.getSolutionValue( _inputQuery.inputVariableByIndex( i ) ) );

        printf( "\n" );
        printf( "Output:\n" );
        for ( unsigned i = 0; i < _inputQuery.getNumOutputVariables(); ++i )
            printf( "\ty%u = %lf\n", i, _inputQuery.getSolutionValue( _inputQuery.outputVariableByIndex( i ) ) );
        printf( "\n" );
    }
    else if ( result == Engine::TIMEOUT )
    {
        resultString = "TIMEOUT";
        printf( "Timeout\n" );
    }
    else if ( result == Engine::ERROR )
    {
        resultString = "ERROR";
        printf( "Error\n" );
    }
    else
    {
        resultString = "UNKNOWN";
        printf( "UNKNOWN EXIT CODE! (this should not happen)" );
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

        // Field #4: average pivot time in micro seconds
        summaryFile.write( Stringf( "%u",
                                    _engine.getStatistics()->getAveragePivotTimeInMicro() ) );

        summaryFile.write( "\n" );
    }
}

void Marabou::dumpSubQuery( const SubQuery &subquery )
{
    std::ofstream o{subquery._queryId.ascii()};
    const auto& split = subquery._split;
    auto bounds = split->getBoundTightenings();
    for ( const auto bound : bounds )
    {
        if ( bound._type == Tightening::LB )
            o << "x" << bound._variable << " >= " << bound._value << "\n";
        else
            o << "x" << bound._variable << " <= " << bound._value << "\n";
    }
    //const std::string marabou_hash = safe_getenv( "MARABOU_HASH" );
    //const Thunk subproblemThunk {
    //{ marabou_hash, {"Marabou", "--timeout=" + timeoutInSeconds * _timeoutFactor, "--num-online-divides=" + _numOnlineDivides,
    //            "--timeout-factor=" + _timeoutFactor, ""},  
        
    //}
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
