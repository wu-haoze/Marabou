/*********************                                                        */
/*! \file IncrementalLinearizatoin.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Teruhiro Tagomori
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

#include "FloatUtils.h"
#include "GurobiWrapper.h"
#include "IncrementalLinearization.h"
#include "Options.h"
#include "TimeUtils.h"

IncrementalLinearization::IncrementalLinearization( MILPEncoder &milpEncoder,
                                                    InputQuery &inputQuery )
    : _milpEncoder( milpEncoder )
    , _inputQuery( inputQuery )
{
}

IEngine::ExitCode IncrementalLinearization::solveWithIncrementalLinearization
( GurobiWrapper &gurobi,
  double timeoutInSeconds,
  unsigned threads,
  unsigned verbosity )
{

    List<TranscendentalConstraint *> tsConstraints = _inputQuery.getTranscendentalConstraints();

    unsigned incrementalCount = 0;
    double remainingTimeoutInSeconds = timeoutInSeconds;
    unsigned cutOff = 30; // START BY ADDING THIS MANY CONSTRAINT!

    while ( remainingTimeoutInSeconds > 0 )
    {
        printf( "\n"
                "--------------------------------------------------\n"
                "--------Start incremental linearization: %u--------\n"
                "--------------------------------------------------\n",
                incrementalCount + 1 );
        
        // Extract the last solution
        Map<String, double> assignment;
        double costOrObjective;
        gurobi.extractSolution( assignment, costOrObjective );

        // Number of new split points
        unsigned numSatisfied = 0;
        unsigned numTangent = 0;
        unsigned numSecant = 0;
        unsigned numSkipped = 0;

        std::vector<TranscendentalConstraint *> temp(tsConstraints.begin(), tsConstraints.end());
        std::random_shuffle(temp.begin(), temp.end());

        for ( const auto &tsConstraint : temp )
        {
            switch ( tsConstraint->getType() )
            {
                case TranscendentalFunctionType::SIGMOID:
                {
                    incrementLinearConstraint( tsConstraint,
                                               assignment,
                                               numSatisfied,
                                               numTangent,
                                               numSecant,
                                               numSkipped,
                                               cutOff );
                    break;
                }
                default:
                {
                    throw MarabouError( MarabouError::UNSUPPORTED_TRANSCENDENTAL_CONSTRAINT,
                                        "IncrementalLinearization::solveWithIncrementalLinearization: "
                                        "Only Sigmoid is supported\n" );
                }
            }
        }

        printf( "satisfied:%u\ntangent:%u\nsecant:%u\nskipped:%u\n",
                numSatisfied, numTangent, numSecant, numSkipped );
        printf( "In total:%u\n",
                numSatisfied + numTangent + numSecant + numSkipped );

        if ( numSatisfied == tsConstraints.size() )
        {
            // All sigmoid constraints are satisfied.
            return IEngine::SAT;
        }

        if ( numTangent + numSecant > 0 )
        {
            //printf( "%u split points were added.\n", numOfNewSplitPoints );
            struct timespec start = TimeUtils::sampleMicro();
            _milpEncoder.reset();
            _milpEncoder.encodeInputQuery( gurobi, _inputQuery );
            gurobi.setTimeLimit( remainingTimeoutInSeconds );
            gurobi.setNumberOfThreads( threads );
            gurobi.setVerbosity( verbosity < 100 );
            gurobi.solve();
            struct timespec end = TimeUtils::sampleMicro();
            unsigned long long passedTime = TimeUtils::timePassed( start, end );
            remainingTimeoutInSeconds -= passedTime / 1000000;
            
            // for debug
            // gurobi.dumpModel( Stringf("gurobi_%u.lp", incrementalCount).ascii() );
            // if ( gurobi.haveFeasibleSolution () )
            //     gurobi.dumpModel( Stringf("gurobi_%u.sol", incrementalCount).ascii() );
        }
        else
        {
            printf( "No longer solve with linearlizations because no new constraint was added.\n" );
            return IEngine::UNKNOWN;
        }

        if ( gurobi.haveFeasibleSolution() )
        {
            if ( cutOff > 10000 )
                cutOff = 10000;
            else
                cutOff *= 2;
            incrementalCount++;
            continue;
        }
        else if ( gurobi.infeasible() )
            return IEngine::UNSAT;
        else if ( gurobi.timeout() || remainingTimeoutInSeconds <= 0 )
            return IEngine::TIMEOUT;
        else
            throw NLRError( NLRError::UNEXPECTED_RETURN_STATUS_FROM_GUROBI );
    }
    return IEngine::UNKNOWN;
}

void IncrementalLinearization::incrementLinearConstraint
( TranscendentalConstraint *constraint,
  const Map<String, double> &assignment,
  unsigned &satisfied,
  unsigned &tangentAdded,
  unsigned &secantAdded,
  unsigned &skipped,
  unsigned cutOff )
{
    SigmoidConstraint *sigmoid = ( SigmoidConstraint * )constraint;
    unsigned sourceVariable = sigmoid->getB();  // x_b
    unsigned targetVariable = sigmoid->getF();  // x_f

    // get x of the found solution and calculate y of the x
    // This x is going to become a new split point
    double xpt = assignment[_milpEncoder.getVariableNameFromVariable( sourceVariable )];
    double ypt = sigmoid->sigmoid( xpt );
    double yptOfSol = assignment[_milpEncoder.getVariableNameFromVariable( targetVariable )];

    if ( constraint->phaseFixed() ||
         FloatUtils::areEqual
         ( ypt, yptOfSol, GlobalConfiguration::SIGMOID_CONSTRAINT_COMPARISON_TOLERANCE ) )
    {
        ++satisfied;
        return;
    }

    //std::cout << "Solution: (" << xpt << " " << yptOfSol << ")" << std::endl;

    const bool clipUse = GlobalConfiguration::SIGMOID_CLIP_POINT_USE;
    const double clipPoint = GlobalConfiguration::SIGMOID_CLIP_POINT_OF_LINEARIZATION;
    bool above = FloatUtils::gt( yptOfSol, ypt );
    bool justTangent = ( ( FloatUtils::lte( _milpEncoder.getUpperBound( sourceVariable ), 0 ) && !above ) &&
                         ( FloatUtils::gte( _milpEncoder.getLowerBound( sourceVariable ), 0 ) && above ) );

    if ( (clipUse && ( xpt <= -clipPoint || xpt >= clipPoint ) ) ||
         ( !justTangent && secantAdded == cutOff ) )
    {
        ++skipped;
        return;
    }

    // If true, secant lines are added, otherwise a tangent line is added.
    sigmoid->addCutPoint( xpt, above );

    if( !justTangent )
    {
        ++secantAdded;
    }
    else
    {
        ++tangentAdded;
    }
}
