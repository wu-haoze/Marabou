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
#include "DisjunctionConstraint.h"
#include "GlobalConfiguration.h"
#include "File.h"
#include "MStringf.h"
#include "Marabou.h"
#include "Options.h"
#include "PropertyParser.h"
#include "MarabouError.h"
#include "QueryLoader.h"

#ifdef _WIN32
#undef ERROR
#endif

Marabou::Marabou()
    : _acasParser( NULL )
    , _engine( NULL )
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
    String summaryFilePath = Options::get()->getString( Options::SUMMARY_FILE );
    if ( File::exists( summaryFilePath ) )
    {
	std::cout << "Summary file exists!" << std::endl;
	exit(0);
    }

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
        _inputQuery = QueryLoader::loadQuery( inputQueryFilePath );
        //_inputQuery.constructNetworkLevelReasoner();
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
        _inputQuery.constructNetworkLevelReasoner();

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

    String queryDumpFilePath = Options::get()->getString( Options::QUERY_DUMP_FILE );
    if ( queryDumpFilePath.length() > 0 )
    {
        _inputQuery.saveQuery( queryDumpFilePath );
        printf( "\nInput query successfully dumped to file\n" );
        exit( 0 );
    }
}

void Marabou::solveQuery()
{
    if ( solveViaRelaxation() )
        return;

    _engine = new Engine();
    if ( _engine->processInputQuery( _inputQuery ) )
        _engine->solve( Options::get()->getInt( Options::TIMEOUT ) );

    if ( _engine->getExitCode() == Engine::SAT )
        _engine->extractSolution( _inputQuery );
}

bool Marabou::solveViaRelaxation()
{
    const List<List<unsigned>> *equivalence = _inputQuery.getEquivalence();
    if ( equivalence->size() == 0 )
        return false;

    Set<unsigned> notInAbstraction;
    Set<unsigned> knownUNSAT;
    notInAbstraction.insert(1);
    DisjunctionConstraint *disj = NULL;
    for ( const auto &constraint : _inputQuery.getPiecewiseLinearConstraints() )
    {
        if ( constraint->getType() == DISJUNCTION )
        {
            disj = (DisjunctionConstraint *)constraint;
            break;
        }
    }
    List<PiecewiseLinearCaseSplit> disjuncts = disj->getCaseSplits();
    _inputQuery.removePiecewiseLinearConstraint( disj );
    std::cout << "Number of disjuncts: " << disjuncts.size() - 1 << std::endl;
    delete disj;

    if ( Options::get()->getBool( Options::RELAXATION ) )
    {
        std::unique_ptr<Engine> preprocessEngine = std::unique_ptr<Engine>( new Engine() );
        preprocessEngine->setVerbosity(0);
        InputQuery inputQuery = _inputQuery;
        preprocessEngine->processInputQuery( inputQuery );

        while ( notInAbstraction.size() < disjuncts.size() - 1 )
        {
            std::cout << "Solving convex relaxation..." << std::endl;
            inputQuery = _inputQuery;
            for ( const auto &equiv : *equivalence )
            {
                if ( disjuncts.size() != equiv.size() )
                    throw MarabouError( MarabouError::MISMATCH );
                unsigned firstVar = *(equiv.begin());
                double minLb = FloatUtils::infinity();
                double maxUb = FloatUtils::negativeInfinity();
                unsigned counter = 0;
                // Update the bounds in the inputQuery
                for ( const auto &var : equiv )
                {
                    if ( var == firstVar || notInAbstraction.exists( counter ) )
                    {
                        ++counter;
                        continue;
                    }
                    auto bound = preprocessEngine->getReindexedVarBounds( var );
                    //printf("%u lb: %f, ub: %f\n", var, bound.first, bound.second);

                    double lb = bound.first;
                    if ( lb < minLb )
                        minLb = lb;
                    double ub = bound.second;
                    if ( ub > maxUb )
                        maxUb = ub;
                    ++counter;
                }
                ASSERT( FloatUtils::isFinite( minLb ) && FloatUtils::isFinite( maxUb ) );
                inputQuery.setLowerBound( firstVar, minLb );
                inputQuery.setUpperBound( firstVar, maxUb );
            }

            PiecewiseLinearCaseSplit fixAbstraction;
            for ( const auto &equiv : *equivalence )
            {
                unsigned firstVar = *(equiv.begin());
                fixAbstraction.storeBoundTightening
                    ( Tightening( firstVar,
                                  inputQuery.getLowerBound( firstVar ),
                                  Tightening::UB ) );
            }

            _engine = new Engine();

            unsigned counter = 0;
            List<PiecewiseLinearCaseSplit> newDisjuncts;
            for ( const auto &split : disjuncts )
            {
                if ( counter != 0 && notInAbstraction.exists( counter )
		     && (!knownUNSAT.exists( counter )) )
                {
		    std::cout << "Adding disjunct " << counter << std::endl;
                    PiecewiseLinearCaseSplit newSplit = split;
                    for ( const auto &t : fixAbstraction.getBoundTightenings() )
                    {
                        newSplit.storeBoundTightening( t );
                    }
                    newDisjuncts.append(newSplit);
                }
                counter++;
            }
            newDisjuncts.append( *( disjuncts.begin() ) );

            DisjunctionConstraint *newDisj = new DisjunctionConstraint( newDisjuncts );

            String s;
            newDisj->dump(s);
            std::cout << s.ascii() << std::endl;

            inputQuery.addPiecewiseLinearConstraint( newDisj );

            _engine->lastDisjunctAbstraction();
            if ( _engine->processInputQuery( inputQuery ) )
                _engine->solve();
            if ( _engine->getExitCode() == Engine::UNSAT ||
		 _engine->getExitCode() == Engine::SAT )
                return true;
            delete _engine;

	    // At this point, no sat is found, all the concrete disjuncts are unsat.
	    for ( const auto &v : notInAbstraction )
	    {	       
		knownUNSAT.insert( v );
	    }
	    
            // Refine the abstraction by removing a node from the abstraction	 
            notInAbstraction.insert( notInAbstraction.size() + 1 );
            std::cout << "Solving convex relaxation - inconclusive" << std::endl;
        }
    }

    unsigned counter = 0;
    List<PiecewiseLinearCaseSplit> newDisjuncts;
    for ( const auto &split : disjuncts )
    {
        if ( counter > 0 &&  (!knownUNSAT.exists( counter )) )
            newDisjuncts.append(split);
        counter++;
    }
    DisjunctionConstraint *newDisj = new DisjunctionConstraint( newDisjuncts );
    _inputQuery.addPiecewiseLinearConstraint( newDisj );
    return false;
}

void Marabou::displayResults( unsigned long long microSecondsElapsed ) const
{
    Engine::ExitCode result = _engine->getExitCode();
    String resultString;

    if ( result == Engine::UNSAT )
    {
        resultString = "unsat";
        printf( "unsat\n" );
    }
    else if ( result == Engine::SAT )
    {
        resultString = "sat";
        printf( "sat\n" );
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
        if ( Options::get()->getBool( Options::SOLVE_ALL_DISJUNCTS ) )
        {
            File summaryFile( summaryFilePath );
            summaryFile.open( File::MODE_WRITE_TRUNCATE );
            for ( const auto &pair : _engine->getFeasibleDisjuncts() )
            {
                summaryFile.write( Stringf( "%u %s\n", pair.first,
                                            pair.second.ascii() ) );
            }
        }
        else
        {
            File summaryFile( summaryFilePath );
            summaryFile.open( File::MODE_WRITE_TRUNCATE );

            // Field #1: result
            summaryFile.write( resultString );

            // Field #2: total elapsed time
            summaryFile.write( Stringf( " %u ", microSecondsElapsed / 1000000 ) ); // In seconds

            // Field #3: number of visited tree states
            summaryFile.write( Stringf( "%u ",
                                        _engine->getStatistics()->getNumVisitedTreeStates() ) );

            // Field #4: average pivot time in micro seconds
            summaryFile.write( Stringf( "%u",
                                        _engine->getStatistics()->getAveragePivotTimeInMicro() ) );

            summaryFile.write( "\n" );

            if ( resultString == "sat" )
            {
                for ( unsigned i = 0; i < _inputQuery.getNumberOfVariables(); ++i )
                    summaryFile.write( Stringf( "\t%u,%lf\n",
                                                i, _inputQuery.getSolutionValue( i ) ) );
            }
        }
    }
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
