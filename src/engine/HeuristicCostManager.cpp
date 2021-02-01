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

#include "HeuristicCostManager.h"
#include "Options.h"

#include <cstdlib>

HeuristicCostManager::HeuristicCostManager( IEngine *engine, SmtCore *smtCore )
    : _engine( engine )
    , _smtCore( smtCore )
    , _networkLevelReasoner( NULL )
    , _gurobi( NULL )
    , _statistics( NULL )
    , _noiseParameter( Options::get()->getFloat( Options::NOISE_PARAMETER ) )
    , _initializationStrategy( Options::get()->getString( Options::INITIALIZATION_STRATEGY ) )
    , _flippingStrategy( Options::get()->getString( Options::FLIPPING_STRATEGY ) )
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
    _lastHeuristicCostUpdate = HeuristicCostUpdate();
}

Map<unsigned, double> &HeuristicCostManager::getHeuristicCost()
{
    return _heuristicCost;
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

    struct timespec end = TimeUtils::sampleMicro();
    _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                             TimeUtils::timePassed( start, end ) );
}

/*
  Called when local optima is reached but not all PLConstraint is satisfied.

  There are two scenarios:
  scenario 1:
  If the local optima is not zero, we flip the cost term for certain PLConstraint already in the cost function.
*/
bool HeuristicCostManager::updateHeuristicCost()
{
    struct timespec start = TimeUtils::sampleMicro();

    COST_LOG( Stringf( "Updating heuristic cost with strategy %s", _flippingStrategy.ascii() ).ascii() );

    if ( _flippingStrategy == "gwsat" )
        _lastHeuristicCostUpdate = updateHeuristicCostGWSAT();
    else
        throw MarabouError( MarabouError::UNKNOWN_LOCAL_SEARCH_STRATEGY,
                            Stringf( "Unknown flipping stategy %s", _flippingStrategy.ascii() ).ascii() );

    COST_LOG( Stringf( "Heuristic cost after updates: %f", computeHeuristicCost() ).ascii() ) ;
    COST_LOG( "Updating heuristic cost - done\n" );

    struct timespec end = TimeUtils::sampleMicro();
    _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                             TimeUtils::timePassed( start, end ) );

    return _lastHeuristicCostUpdate._descenceGuaranteed;
}

void HeuristicCostManager::undoLastHeuristicCostUpdate()
{
    if ( _lastHeuristicCostUpdate._constraint == NULL )
        return;

    _lastHeuristicCostUpdate._constraint->
        addCostFunctionComponent( _heuristicCost,
                                  _lastHeuristicCostUpdate._originalPhaseOfHeuristicCost );
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

    struct timespec end = TimeUtils::sampleMicro();
    _statistics->incLongAttr( Statistics::TIME_UPDATING_COST_FUNCTION_MICRO,
                             TimeUtils::timePassed( start, end ) );
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

void HeuristicCostManager::setGurobi( GurobiWrapper *gurobi )
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

HeuristicCostUpdate HeuristicCostManager::updateHeuristicCostGWSAT()
{
    /*
      Following the heuristics from
      https://www.researchgate.net/publication/2637561_Noise_Strategies_for_Improving_Local_Search
      with probability p, flip the cost term of a randomly chosen unsatisfied PLConstraint
      with probability 1 - p, flip the cost term of the PLConstraint that reduces in the greatest decline in the cost
    */
    bool useNoiseStrategy = ( (float) rand() / RAND_MAX ) <= _noiseParameter;

    PiecewiseLinearConstraint *plConstraintToFlip = NULL;
    PhaseStatus phaseStatusToFlipTo = PHASE_NOT_FIXED;

    COST_LOG( Stringf( "Heuristic cost before updates: %f", computeHeuristicCost() ).ascii() ) ;

    bool descenceGuaranteed = true;
    if ( !useNoiseStrategy )
    {
        // Flip the cost term that reduces the cost by the most
        COST_LOG( "Using default strategy to pick a PLConstraint and flip its heuristic cost..." );
        double maxReducedCost = 0;
        Vector<PiecewiseLinearConstraint *> &violatedPlConstraints =
            _engine->getViolatedPiecewiseLinearConstraints();
        for ( const auto &plConstraint : violatedPlConstraints )
        {
            double reducedCost = 0;
            PhaseStatus phaseStatusOfReducedCost = plConstraint->getAddedHeuristicCost();
            ASSERT( phaseStatusOfReducedCost != PhaseStatus::PHASE_NOT_FIXED );
            plConstraint->getReducedHeuristicCost( reducedCost, phaseStatusOfReducedCost );

            if ( reducedCost > maxReducedCost )
            {
                maxReducedCost = reducedCost;
                plConstraintToFlip = plConstraint;
                phaseStatusToFlipTo = phaseStatusOfReducedCost;
            }
        }
    }

    if ( !plConstraintToFlip ||  useNoiseStrategy )
    {
        // Assume violated pl constraints has been updated.
        // If using noise stategy, we just flip a random
        // PLConstraint.
        COST_LOG( "Using noise strategy to pick a PLConstraint and flip its heuristic cost..." );
        descenceGuaranteed = false;
        unsigned plConstraintIndex = (unsigned) rand() % _plConstraintsInHeuristicCost.size();
        plConstraintToFlip = _plConstraintsInHeuristicCost[plConstraintIndex];
        Vector<PhaseStatus> phaseStatuses = plConstraintToFlip->getAlternativeHeuristicPhaseStatus();
        unsigned phaseIndex = (unsigned) rand() % phaseStatuses.size();
        phaseStatusToFlipTo = phaseStatuses[phaseIndex];
        _smtCore->reportRandomFlip();
        _statistics->incLongAttr( Statistics::NUM_PROPOSED_FLIPS, 1 );
        _statistics->incLongAttr( Statistics::NUM_ACCEPTED_FLIPS, 1 );
    }

    ASSERT( plConstraintToFlip && phaseStatusToFlipTo != PHASE_NOT_FIXED );

    HeuristicCostUpdate update = HeuristicCostUpdate( plConstraintToFlip,
                                                      plConstraintToFlip->getPhaseOfHeuristicCost(),
                                                      descenceGuaranteed );
    plConstraintToFlip->addCostFunctionComponent( _heuristicCost, phaseStatusToFlipTo );
    return update;
}
