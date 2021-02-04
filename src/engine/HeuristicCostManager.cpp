/*********************                                                        */
/*! \file HeuristicCostManager.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Parth Shah, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "GlobalConfiguration.h"
#include "HeuristicCostManager.h"
#include "Options.h"

#include <cstdlib>
#include <numeric>

HeuristicCostManager::HeuristicCostManager( IEngine *engine )
    : _engine( engine )
    , _networkLevelReasoner( NULL )
    , _gurobi( NULL )
    , _statistics( NULL )
    , _noiseParameter( Options::get()->getFloat( Options::NOISE_PARAMETER ) )
    , _initializationStrategy( Options::get()->getString( Options::INITIALIZATION_STRATEGY ) )
    , _flippingStrategy( Options::get()->getString( Options::FLIPPING_STRATEGY ) )
    , _probabilityDensityParameter( Options::get()->getFloat( Options::PROBABILITY_DENSITY_PARAMETER ) )
{
}

/*
  Reset the HeuristicCostManager
*/
void HeuristicCostManager::reset()
{
    _heuristicCost.clear();
    for ( const auto &plConstraint : _plConstraintsInHeuristicCost )
        plConstraint->resetCostFunctionComponent();
    _plConstraintsInHeuristicCost.clear();
    _previousHeuristicCost.clear();
}

/*
  Create the initial cost function for local search
*/
void HeuristicCostManager::initiateCostFunctionForLocalSearch()
{
    struct timespec start = TimeUtils::sampleMicro();

    COST_LOG( Stringf( "Initiating cost function for local search with strategy %s...",
                      _initializationStrategy.ascii() ).ascii() );

    for ( const auto &plConstraint : _plConstraints )
        plConstraint->resetCostFunctionComponent();
    _plConstraintsInHeuristicCost.clear();
    _heuristicCost.clear();
    if ( _initializationStrategy == "currentAssignment" )
        initiateCostFunctionForLocalSearchBasedOnCurrentAssignment( _plConstraints );
    else if ( _initializationStrategy == "inputAssignment" )
        initiateCostFunctionForLocalSearchBasedOnInputAssignment( _plConstraints );
    else if ( _initializationStrategy == "random" )
        initiateCostFunctionForLocalSearchRandomly( _plConstraints );
    else
        throw MarabouError( MarabouError::UNKNOWN_LOCAL_SEARCH_STRATEGY,
                            Stringf( "Unknown initialization stategy %s", _initializationStrategy.ascii() ).ascii() );

    COST_LOG( "initiating cost function for local search - done" );

    _weights = std::vector<double>( _plConstraintsInHeuristicCost.size() );

    if ( _statistics )
    {
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                                  TimeUtils::timePassed( start, end ) );
    }
}

/*
  Called when local optima is reached but not all PLConstraint is satisfied.

  There are two scenarios:
  scenario 1:
  If the local optima is not zero, we flip the cost term for certain PLConstraint already in the cost function.
*/
void HeuristicCostManager::updateHeuristicCost()
{
    struct timespec start = TimeUtils::sampleMicro();

    COST_LOG( Stringf( "Updating heuristic cost with strategy %s", _flippingStrategy.ascii() ).ascii() );
    COST_LOG( Stringf( "Heuristic cost before updates: %f", computeHeuristicCost() ).ascii() ) ;

    _previousHeuristicCost.clear();
    for ( const auto &constraint : _plConstraintsInHeuristicCost )
    {
        _previousHeuristicCost[constraint] = constraint->getPhaseOfHeuristicCost();
    }

    if ( _flippingStrategy == "gwsat" )
        updateHeuristicCostGWSAT();
    else if ( _flippingStrategy == "gwsat2" )
        updateHeuristicCostGWSAT2();
    else if ( _flippingStrategy == "mcmc1" )
        updateHeuristicCostMCMC1();
    else if ( _flippingStrategy == "mcmc2" )
        updateHeuristicCostMCMC2();
    else
        throw MarabouError( MarabouError::UNKNOWN_LOCAL_SEARCH_STRATEGY,
                            Stringf( "Unknown flipping stategy %s", _flippingStrategy.ascii() ).ascii() );

    COST_LOG( Stringf( "Heuristic cost after updates: %f", computeHeuristicCost() ).ascii() ) ;
    COST_LOG( "Updating heuristic cost - done\n" );

    if ( _statistics )
    {
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->incLongAttr( Statistics::NUM_PROPOSED_FLIPS, 1 );
        _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                                 TimeUtils::timePassed( start, end ) );
    }
}

void HeuristicCostManager::undoLastHeuristicCostUpdate()
{
    for ( const auto &pair : _previousHeuristicCost )
        pair.first->addCostFunctionComponent( _heuristicCost, pair.second );
}

void HeuristicCostManager::removeCostComponentFromHeuristicCost
( PiecewiseLinearConstraint *constraint )
{
    ASSERT( _plConstraints.exists( constraint ) );
    if ( _plConstraintsInHeuristicCost.exists( constraint ) )
    {
        constraint->removeCostFunctionComponent( _heuristicCost );
        _plConstraintsInHeuristicCost.erase( constraint );
    }
}

void HeuristicCostManager::updateCostTermsForSatisfiedPLConstraints()
{
    struct timespec start = TimeUtils::sampleMicro();

    COST_LOG( "Updating cost terms for satisfied constraint..." );
    COST_LOG( Stringf( "Heuristic cost before updating cost terms for satisfied constraint: %f",
                      computeHeuristicCost() ).ascii() );
    for ( const auto &plConstraint : _plConstraints )
    {
        if ( plConstraint->isActive() &&
             ( !plConstraint->phaseFixed() ) &&
             plConstraint->satisfied() )
        {
            double reducedCost = 0;
            PhaseStatus phaseStatusOfReducedCost = PhaseStatus::PHASE_NOT_FIXED;
            plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );
            if ( FloatUtils::isPositive( reducedCost ) )
            {
                // We can make the heuristic cost 0 by just flipping the cost term.
                plConstraint->addCostFunctionComponent
                    ( _heuristicCost, phaseStatusOfReducedCost );
            }
        }
    }
    COST_LOG( Stringf( "Heuristic cost after updating cost terms for satisfied constraint: %f",
                      computeHeuristicCost() ).ascii() );
    COST_LOG( "Updating cost terms for satisfied constraint - done\n" );

    if ( _statistics )
    {
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                                  TimeUtils::timePassed( start, end ) );
    }
}

void HeuristicCostManager::dumpHeuristicCost()
{
    String s = "";
    bool first = true;
    for ( const auto &pair : _heuristicCost )
    {
        if ( first )
        {
            s += Stringf( "%.2f x%u ", pair.second, pair.first );
            first = false;
        }
        else
        {
            s += Stringf( "+ %.2f x%u ", pair.second, pair.first );
        }
    }
    std::cout << s.ascii() << std::endl;
    return;
}

bool HeuristicCostManager::acceptProposedUpdate( double previousCost, double currentCost )
{
    struct timespec start = TimeUtils::sampleMicro();

    double proposalProbabilityRatio = 1;
    if ( _flippingStrategy == "mcmc2" )
    {
        double max = FloatUtils::infinity();
        double min = FloatUtils::infinity();
        unsigned i = 0;
        for ( const auto &plConstraint : _plConstraintsInHeuristicCost )
        {
            double reducedCost = 0;
            PhaseStatus phaseStatusOfReducedCost = plConstraint->getPhaseOfHeuristicCost();
            ASSERT( phaseStatusOfReducedCost != PhaseStatus::PHASE_NOT_FIXED );
            plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );
            if ( reducedCost < min ) min = reducedCost;
            if ( reducedCost > max ) max = reducedCost;
            _weights[i++] = reducedCost;
        }

        double range = max - min;
        for ( unsigned i = 0; i < _weights.size(); ++i )
            _weights[i] = ( _weights[i] - min ) / range;

        double sum = std::accumulate( _weights.begin(), _weights.end(), 0.0 );
        proposalProbabilityRatio = _probabilityOfLastProposal / ( _weights[_lastFlippedConstraintIndex] / sum );
    }


    double prob = exp( -_probabilityDensityParameter * ( currentCost - previousCost ) ) * proposalProbabilityRatio;
    COST_LOG( Stringf( "Previous Cost: %.2f. Cost after proposed flip: %.2f."
                       " Proposal probability ratio: %.2f.\n"
                       "Probability to accept the flip: %.2lf%%", previousCost, currentCost,
                       proposalProbabilityRatio, prob ).ascii() );

    bool flip = prob >= 1 || ( (float) rand() / RAND_MAX ) < prob;

    if ( _statistics )
    {
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                                  TimeUtils::timePassed( start, end ) );
    }

    return flip;
}

double HeuristicCostManager::computeHeuristicCost()
{
    double cost = 0;
    for ( const auto &pair : _heuristicCost )
    {
        double value = _gurobi->getValue( pair.first );
        cost += pair.second * value;
    }
    return cost;
}

void HeuristicCostManager::setStatistics( Statistics *statistics )
{
    _statistics = statistics;
}

void HeuristicCostManager::setPLConstraints( List<PiecewiseLinearConstraint *> &plConstraints )
{
    _plConstraints = plConstraints;
}

void HeuristicCostManager::setNetworkLevelReasoner( NLR::NetworkLevelReasoner *networkLevelReasoner )
{
    _networkLevelReasoner = networkLevelReasoner;
}

void HeuristicCostManager::setGurobi( LPSolver *gurobi )
{
    _gurobi = gurobi;
}

void HeuristicCostManager::initiateCostFunctionForLocalSearchBasedOnCurrentAssignment
( const List<PiecewiseLinearConstraint *> &plConstraintsToAdd )
{
    for ( const auto &plConstraint : plConstraintsToAdd )
    {
        ASSERT( !_plConstraintsInHeuristicCost.exists( plConstraint ) );
        if ( plConstraint->isActive() && !plConstraint->phaseFixed() )
        {
            plConstraint->addCostFunctionComponent( _heuristicCost );
            _plConstraintsInHeuristicCost.append( plConstraint );
        }
    }
}

void HeuristicCostManager::initiateCostFunctionForLocalSearchBasedOnInputAssignment
( const List<PiecewiseLinearConstraint *> &plConstraintsToAdd )
{
    _engine->concretizeInputAssignment();
    for ( const auto &plConstraint : plConstraintsToAdd )
    {
        ASSERT( !_plConstraintsInHeuristicCost.exists( plConstraint ) );
        if ( plConstraint->isActive() && !plConstraint->phaseFixed() )
        {
            NLR::NeuronIndex index = _networkLevelReasoner->getNeuronIndexFromPLConstraint( plConstraint );
            double value = _networkLevelReasoner->getLayer( index._layer )->getAssignment()[index._neuron];
            plConstraint->addCostFunctionComponentByOutputValue( _heuristicCost, value );
            _plConstraintsInHeuristicCost.append( plConstraint );
        }
    }
}

void HeuristicCostManager::initiateCostFunctionForLocalSearchRandomly
( const List<PiecewiseLinearConstraint *> &plConstraintsToAdd )
{
    for ( const auto &plConstraint : plConstraintsToAdd )
    {
        ASSERT( !_plConstraintsInHeuristicCost.exists( plConstraint ) );
        if ( plConstraint->isActive() && !plConstraint->phaseFixed() )
        {
            Vector<PhaseStatus> phaseStatuses = plConstraint->getAlternativeHeuristicPhaseStatus();
            unsigned phaseIndex = (unsigned) rand() % phaseStatuses.size();
            PhaseStatus phaseStatusToSet = phaseStatuses[phaseIndex];
            plConstraint->addCostFunctionComponent( _heuristicCost, phaseStatusToSet );
            _plConstraintsInHeuristicCost.append( plConstraint );
        }
    }
}

void HeuristicCostManager::updateHeuristicCostGWSAT()
{
    PiecewiseLinearConstraint *plConstraintToFlip = NULL;
    PhaseStatus phaseStatusToFlipTo = PHASE_NOT_FIXED;

    // Flip the cost term that reduces the cost by the most
    COST_LOG( "Using default strategy to pick a PLConstraint and flip its heuristic cost..." );
    double maxReducedCost = 0;
    Vector<PiecewiseLinearConstraint *> &violatedPlConstraints =
        _engine->getViolatedPiecewiseLinearConstraints();
    for ( const auto &plConstraint : violatedPlConstraints )
    {
        double reducedCost = 0;
        PhaseStatus phaseStatusOfReducedCost = plConstraint->getPhaseOfHeuristicCost();
        ASSERT( phaseStatusOfReducedCost != PhaseStatus::PHASE_NOT_FIXED );
        plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );

        if ( reducedCost > maxReducedCost )
        {
            maxReducedCost = reducedCost;
            plConstraintToFlip = plConstraint;
            phaseStatusToFlipTo = phaseStatusOfReducedCost;
        }
    }

    if ( !plConstraintToFlip )
    {
        // Assume violated pl constraints has been updated.
        // If using noise stategy, we just flip a random
        // PLConstraint.
        COST_LOG( "Using noise strategy to pick a PLConstraint and flip its heuristic cost..." );
        unsigned plConstraintIndex = (unsigned) rand() % _plConstraintsInHeuristicCost.size();
        plConstraintToFlip = _plConstraintsInHeuristicCost[plConstraintIndex];
        Vector<PhaseStatus> phaseStatuses = plConstraintToFlip->getAlternativeHeuristicPhaseStatus();
        unsigned phaseIndex = (unsigned) rand() % phaseStatuses.size();
        phaseStatusToFlipTo = phaseStatuses[phaseIndex];
    }

    ASSERT( plConstraintToFlip && phaseStatusToFlipTo != PHASE_NOT_FIXED );

    plConstraintToFlip->addCostFunctionComponent( _heuristicCost, phaseStatusToFlipTo );
}

void HeuristicCostManager::updateHeuristicCostGWSAT2()
{
    PiecewiseLinearConstraint *plConstraintToFlip = NULL;
    PhaseStatus phaseStatusToFlipTo = PHASE_NOT_FIXED;

    // Flip the cost term that reduces the cost by the most
    COST_LOG( "Using default strategy to pick a PLConstraint and flip its heuristic cost..." );
    double maxReducedCost = 0;
    for ( const auto &plConstraint : _plConstraintsInHeuristicCost )
    {
        double reducedCost = 0;
        PhaseStatus phaseStatusOfReducedCost = plConstraint->getPhaseOfHeuristicCost();
        ASSERT( phaseStatusOfReducedCost != PhaseStatus::PHASE_NOT_FIXED );
        plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );

        if ( reducedCost > maxReducedCost )
        {
            maxReducedCost = reducedCost;
            plConstraintToFlip = plConstraint;
            phaseStatusToFlipTo = phaseStatusOfReducedCost;
        }
    }

    if ( !plConstraintToFlip )
    {
        // Assume violated pl constraints has been updated.
        // If using noise stategy, we just flip a random
        // PLConstraint.
        COST_LOG( "Using noise strategy to pick a PLConstraint and flip its heuristic cost..." );
        unsigned plConstraintIndex = (unsigned) rand() % _plConstraintsInHeuristicCost.size();
        plConstraintToFlip = _plConstraintsInHeuristicCost[plConstraintIndex];
        Vector<PhaseStatus> phaseStatuses = plConstraintToFlip->getAlternativeHeuristicPhaseStatus();
        unsigned phaseIndex = (unsigned) rand() % phaseStatuses.size();
        phaseStatusToFlipTo = phaseStatuses[phaseIndex];
    }

    ASSERT( plConstraintToFlip && phaseStatusToFlipTo != PHASE_NOT_FIXED );

    plConstraintToFlip->addCostFunctionComponent( _heuristicCost, phaseStatusToFlipTo );
}

void HeuristicCostManager::updateHeuristicCostMCMC1()
{
    // 1. Flip all the cost term that can improve the cost.
    // 2. If no such cost terms, randomly propose one to flip.

    // Flip the cost term that reduces the cost by the most
    COST_LOG( "Using strategy mcmc1 to pick a PLConstraint and flip its heuristic cost..." );
    Vector<PiecewiseLinearConstraint *> &violatedPlConstraints =
        _engine->getViolatedPiecewiseLinearConstraints();

    unsigned numFlipped = 0;
    for ( const auto &plConstraint : violatedPlConstraints )
    {
        double reducedCost = 0;
        PhaseStatus phaseStatusOfReducedCost = plConstraint->getPhaseOfHeuristicCost();
        ASSERT( phaseStatusOfReducedCost != PhaseStatus::PHASE_NOT_FIXED );
        plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );

        if ( reducedCost > 0 )
        {
            plConstraint->addCostFunctionComponent( _heuristicCost, phaseStatusOfReducedCost );
            ++numFlipped;
        }
    }
    if ( numFlipped > 0 )
    {
        COST_LOG( "Cost function guaranteed to reduce. No random proposal." );
        return;
    }

    COST_LOG( "Cost function not guaranteed to reduce." );
    unsigned plConstraintIndex = (unsigned) rand() % _plConstraintsInHeuristicCost.size();
    PiecewiseLinearConstraint *plConstraintToFlip = _plConstraintsInHeuristicCost[plConstraintIndex];
    Vector<PhaseStatus> phaseStatuses = plConstraintToFlip->getAlternativeHeuristicPhaseStatus();
    unsigned phaseIndex = (unsigned) rand() % phaseStatuses.size();
    PhaseStatus phaseStatusToFlipTo = phaseStatuses[phaseIndex];
    plConstraintToFlip->addCostFunctionComponent( _heuristicCost, phaseStatusToFlipTo );
}

void HeuristicCostManager::updateHeuristicCostMCMC2()
{
    // Turn reduced cost to a probability distribution
    ASSERT( _weights.size() == _plConstraintsInHeuristicCost.size() );

    unsigned i = 0;
    double max = FloatUtils::infinity();
    double min = FloatUtils::infinity();
    for ( const auto &plConstraint : _plConstraintsInHeuristicCost )
    {
        double reducedCost = 0;
        PhaseStatus phaseStatusOfReducedCost = plConstraint->getPhaseOfHeuristicCost();
        ASSERT( phaseStatusOfReducedCost != PhaseStatus::PHASE_NOT_FIXED );
        plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );
        if ( reducedCost < min ) min = reducedCost;
        if ( reducedCost > max ) max = reducedCost;
        _weights[i++] = reducedCost;
    }

    double range = max - min;
    for ( unsigned i = 0; i < _weights.size(); ++i )
        _weights[i] = ( _weights[i] - min ) / range;

    std::discrete_distribution<int> distribution( _weights.begin(), _weights.end() );
    unsigned plConstraintIndex = distribution( _generator );
    PiecewiseLinearConstraint *lastFlippedConstraint = _plConstraintsInHeuristicCost[plConstraintIndex];
    _lastFlippedConstraintIndex = plConstraintIndex;

    Vector<PhaseStatus> phaseStatuses = lastFlippedConstraint->getAlternativeHeuristicPhaseStatus();
    ASSERT( phaseStatuses.size() <= 2 );
    lastFlippedConstraint->addCostFunctionComponent( _heuristicCost, *( phaseStatuses.begin() ) );

    double sum = std::accumulate( _weights.begin(), _weights.end(), 0.0 );
    _probabilityOfLastProposal = _weights[plConstraintIndex] / sum;

    COST_LOG( Stringf( "Constraint to flip picked with probability %.5f", _probabilityOfLastProposal ).ascii() );
}
