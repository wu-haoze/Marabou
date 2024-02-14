/*********************                                                        */
/*! \file IncrementalLinearizatoin.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Teruhiro Tagomori, Andrew Wu
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

#include "IncrementalLinearization.h"

#include "Engine.h"
#include "FloatUtils.h"
#include "GurobiWrapper.h"
#include "InputQuery.h"
#include "Options.h"
#include "TimeUtils.h"

#include <random>

namespace CEGAR {

IncrementalLinearization::IncrementalLinearization( InputQuery &inputQuery, Engine *engine )
    : _inputQuery( inputQuery )
    , _engine( std::unique_ptr<Engine>( engine ) )
    , _timeoutInMicroSeconds( 0 )
    , _round( 0 )
    , _numAdditionalEquations( 0 )
    , _numAdditionalPLConstraints( 0 )
    , _numConstraintsToRefine(
          Options::get()->getInt( Options::NUM_CONSTRAINTS_TO_REFINE_INC_LIN ) )
    , _refinementScalingFactor(
          Options::get()->getFloat( Options::REFINEMENT_SCALING_FACTOR_INC_LIN ) )
{
    srand( Options::get()->getInt( Options::SEED ) );
    for ( const auto &c : _inputQuery.getNonlinearConstraints() )
        _nlConstraints.append( c );
}

void IncrementalLinearization::solve()
{
    /*
      Invariants at the beginning of the loop:
      1. _inputQuery contains the assignment in the previous refinement round
      2. _timeoutInMicroSeconds is positive
    */
    while ( true )
    {
        struct timespec start = TimeUtils::sampleMicro();
        printStatus();

        // Refine the non-linear constraints using the counter-example stored
        // in the _inputQuery
        unsigned numRefined = refine();
        if ( numRefined != 0 )
            return;

        // Create a new engine
        _engine = std::unique_ptr<Engine>( new Engine() );
        _engine->setVerbosity( 2 );

        // Solve the refined abstraction
        if ( _engine->processInputQuery( _inputQuery ) )
        {
            _engine->solve( static_cast<long double>( _timeoutInMicroSeconds ) /
                            MICROSECONDS_TO_SECONDS );
        }

        if ( _engine->getExitCode() == IEngine::UNKNOWN )
        {
            _inputQuery.clearSolution();
            _engine->extractSolution( _inputQuery );
            _timeoutInMicroSeconds -= TimeUtils::timePassed( start, TimeUtils::sampleMicro() );
            _numConstraintsToRefine =
                std::min( _nlConstraints.size(),
                          (unsigned)( _numConstraintsToRefine * _refinementScalingFactor ) );
        }
        else
            return;
    }
}

unsigned IncrementalLinearization::refine()
{
    INCREMENTAL_LINEARIZATION_LOG( "Performing abstraction refinement..." );

    InputQuery refinement;
    refinement.setNumberOfVariables( _inputQuery.getNumberOfVariables() );
    _engine->extractSolution( refinement );
    _engine->extractBounds( refinement );

    _nlConstraints.shuffle();

    unsigned numRefined = 0;
    for ( const auto &nlc : _nlConstraints )
    {
        numRefined += nlc->attemptToRefine( refinement );
        if ( numRefined >= _numConstraintsToRefine )
            break;
    }

    for ( const auto &e : refinement.getEquations() )
        _inputQuery.addEquation( e );

    for ( const auto &plc : refinement.getPiecewiseLinearConstraints() )
    {
        _inputQuery.addPiecewiseLinearConstraint( plc );
    }
    // Ownership of the additional constraints are transferred.
    refinement.getPiecewiseLinearConstraints().clear();

    INCREMENTAL_LINEARIZATION_LOG(
        Stringf( "Refined %u non-linear constraints", numRefined ).ascii() );
    return numRefined;
}

/*
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
bool justTangent = ( ( FloatUtils::lte( _milpEncoder.getUpperBound( sourceVariable ), 0 ) && !above
) && ( FloatUtils::gte( _milpEncoder.getLowerBound( sourceVariable ), 0 ) && above ) );

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
*/

void IncrementalLinearization::printStatus()
{
    printf( "\n--- Incremental linearization round %u ---\n", ++_round );
    printf( "Added %u equations, %u piecewise-linear constraints.\n",
            _numAdditionalEquations,
            _numAdditionalPLConstraints );
}

Engine *IncrementalLinearization::releaseEngine()
{
    return _engine.release();
}

void IncrementalLinearization::setInitialTimeoutInMicroSeconds(
    unsigned long long timeoutInMicroSeconds )
{
    _timeoutInMicroSeconds =
        ( timeoutInMicroSeconds == 0 ? FloatUtils::infinity() : timeoutInMicroSeconds );
}

} // namespace CEGAR
