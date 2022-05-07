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

IncrementalLinearization::IncrementalLinearization( MILPEncoder &milpEncoder )
    : _milpEncoder( milpEncoder)
{
}

IEngine::ExitCode IncrementalLinearization::solveWithIncrementalLinearization( GurobiWrapper &gurobi, List<TranscendentalConstraint *> tsConstraints, double timeoutInSeconds )
{
    unsigned incrementalCount = 0;
    const unsigned numOfIncrementalLinearizations = Options::get()->getInt( Options::NUMBER_OF_INCREMENTAL_LINEARIZATIONS );
    
    double restTimeoutInSeconds = timeoutInSeconds;

    while ( incrementalCount < numOfIncrementalLinearizations && restTimeoutInSeconds > 0 )
    {
        gurobi.setTimeLimit( restTimeoutInSeconds );
        printf( "Start incremental linearization: %u\n",
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
        for ( const auto &tsConstraint : tsConstraints )
        {
            switch ( tsConstraint->getType() )
            {
                case TranscendentalFunctionType::SIGMOID:
                {
                    incrementLinearConstraint( gurobi,
                                               tsConstraint,
                                               assignment,
                                               numSatisfied,
                                               numTangent,
                                               numSecant,
                                               numSkipped );
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
            gurobi.solve();
            struct timespec end = TimeUtils::sampleMicro();
            unsigned long long passedTime = TimeUtils::timePassed( start, end );
            restTimeoutInSeconds -= passedTime / 1000000;
            
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
            incrementalCount++;
            continue;
        }
        else if ( gurobi.infeasible() )
            return IEngine::UNSAT;
        else if ( gurobi.timeout() || restTimeoutInSeconds <= 0 )
            return IEngine::TIMEOUT;
        else
            throw NLRError( NLRError::UNEXPECTED_RETURN_STATUS_FROM_GUROBI );
    }
    return IEngine::UNKNOWN;
}

void IncrementalLinearization::incrementLinearConstraint( GurobiWrapper &gurobi,
                                                          TranscendentalConstraint *constraint,
                                                          const Map<String, double> &assignment,
                                                          unsigned &satisfied,
                                                          unsigned &tangentAdded,
                                                          unsigned &secantAdded,
                                                          unsigned &skipped)
{
    SigmoidConstraint *sigmoid = ( SigmoidConstraint * )constraint;
    unsigned sourceVariable = sigmoid->getB();  // x_b
    unsigned targetVariable = sigmoid->getF();  // x_f

    // get x of the found solution and calculate y of the x
    // This x is going to become a new split point
    double xpt = assignment[_milpEncoder.getVariableNameFromVariable( sourceVariable )];
    double ypt = sigmoid->sigmoid( xpt );
    double yptOfSol = assignment[_milpEncoder.getVariableNameFromVariable( targetVariable )];

    std::cout << "Solution: " << xpt << " " << yptOfSol << std::endl;

    if ( constraint->phaseFixed() ||
         FloatUtils::areEqual
         ( ypt, yptOfSol, GlobalConfiguration::SIGMOID_CONSTRAINT_COMPARISON_TOLERANCE ) )
    {
        ++satisfied;
        return;
    }

    const bool clipUse = GlobalConfiguration::SIGMOID_CLIP_POINT_USE;
    const double clipPoint = GlobalConfiguration::SIGMOID_CLIP_POINT_OF_LINEARIZATION;
    if ( clipUse && ( xpt <= -clipPoint || xpt >= clipPoint ) )
    {
        ++skipped;
        return;
    }

    // If true, secant lines are added, otherwise a tangent line is added.
    bool isSolInsideOfConvex = ( ( !FloatUtils::isNegative( xpt ) && ypt > yptOfSol )
                                 || ( FloatUtils::isNegative( xpt ) && ypt < yptOfSol ) );

    // get current secant points
    const TranscendentalConstraint::SecantPoints &secantPts = sigmoid->getSecantPoints();

    std::cout << "Sec: ";
    for ( const auto &pt : secantPts )
    {
        std::cout << "("<< pt._x << ", " << pt._y << ") ";
    }
    std::cout << "\ntan: ";
    const TranscendentalConstraint::TangentPoints &tangentPts = sigmoid->getTangentPoints();

    for ( const auto &pt : tangentPts )
    {
        std::cout << "("<< pt._x << ", " << pt._y << ") ";
    }
    std::cout << std::endl;

    // get lower bound and upper bound
    ASSERT( secantPts.size() > 1 );
    auto it = secantPts.begin();
    double sourceLb = it->_x;
    it = secantPts.end();
    --it;
    double sourceUb = it->_x;

    if( !isSolInsideOfConvex )
    {
        // get current tangent points
        const TranscendentalConstraint::TangentPoints &tangentPts = sigmoid->getTangentPoints();

        for ( const auto &pt : tangentPts )
        {
            if ( FloatUtils::areEqual( pt._x, xpt ))
            {
                printf("same tangent point needs to be added, this should not happen!!!!\n");
                // if xpt is same as one of current tangent points, no longer continues.
                return;
            }
        }

        // add a tangent line
        _milpEncoder.addTangentLineOnSigmoid( gurobi, sigmoid, xpt, ypt, sourceLb, sourceUb );
        INCREMENTAL_LINEARIZATION_LOG( Stringf( "new xpt: %f, new ypt: %f for a tangent line",
                                        xpt, ypt ).ascii() );
        sigmoid->addTangentPoint( xpt, ypt );
        ++tangentAdded;
    }
    else
    {
        // generate xpts and ypts for secant lines.
        double xpts[secantPts.size() + 1];
        double ypts[secantPts.size() + 1];
        unsigned i = 0;
        double ptSet = false;
        for ( const auto &pt : secantPts )
        {
            if ( FloatUtils::areEqual( pt._x, xpt ))
            {
                printf("same secant point needs to be added, this should not happen!!!!\n");
                // if xpt is same as one of current secant points, no longer continues.
                return;
            }
            else if ( FloatUtils::gt( pt._x, xpt ) && !ptSet )
            {
                xpts[i] = xpt;
                ypts[i] = ypt;
                i++;
                ptSet = true;
            }

            xpts[i] = pt._x;
            ypts[i] = pt._y;
            i++;
        }

        // add secant lines
        _milpEncoder.addSecantLinesOnSigmoid( gurobi, sigmoid, secantPts.size() + 1, xpts, ypts, sourceLb, sourceUb );
        INCREMENTAL_LINEARIZATION_LOG( Stringf( "new xpt: %f, new ypt: %f for secant lines",
                                        xpt, ypt ).ascii() );
        sigmoid->addSecantPoint( xpt, ypt );
        ++secantAdded;
    }
}
