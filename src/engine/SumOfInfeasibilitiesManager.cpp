/*********************                                                        */
/*! \file SumOfInfeasibilitiesManager.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze (Andrew) Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include "FloatUtils.h"
#include "MarabouError.h"
#include "Options.h"
#include "SumOfInfeasibilitiesManager.h"

#include "Set.h"

SumOfInfeasibilitiesManager::SumOfInfeasibilitiesManager( const InputQuery
                                                          &inputQuery,
                                                          const ITableau
                                                          &tableau )
    : _plConstraints( inputQuery.getPiecewiseLinearConstraints() )
    , _networkLevelReasoner( inputQuery.getNetworkLevelReasoner() )
    , _numberOfVariables( inputQuery.getNumberOfVariables() )
    , _tableau( tableau )
    , _initializationStrategy( Options::get()->getSoIInitializationStrategy() )
    , _searchStrategy( Options::get()->getSoISearchStrategy() )
    , _probabilityDensityParameter( Options::get()->getFloat
                                    ( Options::PROBABILITY_DENSITY_PARAMETER ) )
{}

void SumOfInfeasibilitiesManager::resetPhasePattern()
{
    _currentPhasePattern.clear();
    _currentProposal.clear();
    _plConstraintsInCurrentPhasePattern.clear();
}

LinearExpression SumOfInfeasibilitiesManager::getSoIPhasePattern() const
{
    LinearExpression cost;
    for ( const auto &pair : _currentPhasePattern )
    {
        pair.first->getCostFunctionComponent( cost, pair.second );
    }
    return cost;
}

LinearExpression SumOfInfeasibilitiesManager::getProposedSoIPhasePattern() const
{
    DEBUG({
            // Check that the constraints in the proposal is a subset of those
            // in the currentPhasePattern
            ASSERT( Set<PiecewiseLinearConstraint *>::containedIn
                    ( _currentProposal.keys(), _currentPhasePattern.keys() ) );
        });

    LinearExpression cost;
    for ( const auto &pair : _currentProposal )
        pair.first->getCostFunctionComponent( cost, pair.second );

    for ( const auto &pair : _currentPhasePattern )
        if ( !_currentProposal.exists( pair.first ) )
            pair.first->getCostFunctionComponent( cost, pair.second );

    return cost;
}

void SumOfInfeasibilitiesManager::initializePhasePattern()
{
    resetPhasePattern();
    if ( _initializationStrategy == SoIInitializationStrategy::INPUT_ASSIGNMENT
         && _networkLevelReasoner )
    {
        initializePhasePatternWithCurrentInputAssignment();
    }
    else
    {
        throw MarabouError
            ( MarabouError::UNABLE_TO_INITIALIZATION_PHASE_PATTERN );
    }
    for ( const auto &pair : _currentPhasePattern )
        _plConstraintsInCurrentPhasePattern.append( pair.first );
}

void SumOfInfeasibilitiesManager::initializePhasePatternWithCurrentInputAssignment()
{
    ASSERT( _networkLevelReasoner );
    Map<unsigned, double> assignment;
    _networkLevelReasoner->concretizeInputAssignment( assignment );

    for ( const auto &plConstraint : _plConstraints )
    {
        ASSERT( !_currentPhasePattern.exists( plConstraint ) );
        if ( plConstraint->isActive() && !plConstraint->phaseFixed() )
        {
            _currentPhasePattern[plConstraint] =
                plConstraint->getPhaseStatusInAssignment( assignment );
        }
    }
}

void SumOfInfeasibilitiesManager::proposePhasePatternUpdate()
{
    _currentProposal.clear();
    if ( _searchStrategy == SoISearchStrategy::MCMC )
    {
        proposePhasePatternUpdateRandomly();
    }
    else
    {
        // Walksat
        proposePhasePatternUpdateWalksat();
    }
}

void SumOfInfeasibilitiesManager::proposePhasePatternUpdateRandomly()
{
    DEBUG({
            // _plConstraintsInCurrentPhasePattern should contain the same
            // plConstraints in _currentPhasePattern
            ASSERT( _plConstraintsInCurrentPhasePattern.size() ==
                    _currentPhasePattern.size() );
            for ( const auto &pair : _currentPhasePattern )
                ASSERT( _plConstraintsInCurrentPhasePattern.exists
                        ( pair.first ) );
        });

    unsigned index = ( unsigned ) rand() %
                       _plConstraintsInCurrentPhasePattern.size();
    PiecewiseLinearConstraint *plConstraintToUpdate =
        _plConstraintsInCurrentPhasePattern[index];
    PhaseStatus currentPhase = _currentPhasePattern[plConstraintToUpdate];
    List<PhaseStatus> allPhases = plConstraintToUpdate->getAllCases();
    allPhases.erase( currentPhase );
    if ( allPhases.size() == 1 )
    {
        // There are only two possible phases. So we just flip the phase.
        _currentProposal[plConstraintToUpdate] = *( allPhases.begin() );
    }
    else
    {
        auto it = allPhases.begin();
        unsigned index =  ( unsigned ) rand() % allPhases.size();
        while ( index > 0 )
        {
            ++it;
            --index;
        }
        _currentProposal[plConstraintToUpdate] = *it;
    }
}

void SumOfInfeasibilitiesManager::proposePhasePatternUpdateWalksat()
{
    // Flip the cost term that reduces the cost by the most
    PiecewiseLinearConstraint *plConstraintToUpdate = NULL;
    PhaseStatus updatedPhase = PHASE_NOT_FIXED;
    double maxReducedCost = 0;
    for ( const auto &plConstraint : _plConstraintsInCurrentPhasePattern )
    {
        double reducedCost = 0;
        PhaseStatus phaseStatusOfReducedCost = PHASE_NOT_FIXED;
        getReducedCost( plConstraint, reducedCost, phaseStatusOfReducedCost );

        if ( reducedCost > maxReducedCost )
        {
            plConstraintToUpdate = plConstraint;
            updatedPhase = phaseStatusOfReducedCost;
        }
    }

    if ( plConstraintToUpdate )
    {
        _currentProposal[plConstraintToUpdate] = updatedPhase;
    }
    else
    {
        proposePhasePatternUpdateRandomly();
    }
}

bool SumOfInfeasibilitiesManager::decideToAcceptCurrentProposal
( double costOfCurrentPhasePattern, double costOfProposedPhasePattern )
{
    if ( costOfProposedPhasePattern < costOfCurrentPhasePattern )
        return true;
    else
    {
        // The smaller the difference between the proposed phase pattern and the
        // current phase pattern, the more likely to accept the proposal.
        double prob = exp( -_probabilityDensityParameter *
                           ( costOfProposedPhasePattern -
                             costOfCurrentPhasePattern ) );
        return ( (float) rand() / RAND_MAX ) < prob;
    }
}


void SumOfInfeasibilitiesManager::acceptCurrentProposal()
{
    // We update _currentPhasePattern with entries in _currentProposal
    for ( const auto &pair : _currentProposal )
    {
        _currentPhasePattern[pair.first] = pair.second;
    }
}

void SumOfInfeasibilitiesManager::updateCurrentPhasePatternForSatisfiedPLConstraints()
{
    for ( const auto &pair : _currentPhasePattern )
    {
        if ( pair.first->satisfied() )
        {
            PhaseStatus satisfiedPhaseStatus =
                pair.first->getPhaseStatusInAssignment( _currentAssignment );
            _currentPhasePattern[pair.first] = satisfiedPhaseStatus;
        }
    }
}

void SumOfInfeasibilitiesManager::removeCostComponentFromHeuristicCost
( PiecewiseLinearConstraint *constraint )
{
    if ( _currentPhasePattern.exists( constraint ) )
    {
        _currentPhasePattern.erase( constraint );
        ASSERT( _plConstraintsInCurrentPhasePattern.exists( constraint ) );
        _plConstraintsInCurrentPhasePattern.erase( constraint );
    }
}

void SumOfInfeasibilitiesManager::obtainCurrentAssignment()
{
    _currentAssignment.clear();
    for ( unsigned i = 0; i < _numberOfVariables; ++i )
        _currentAssignment[i] = _tableau.getValue( i );
}

void SumOfInfeasibilitiesManager::setStatistics( Statistics *statistics )
{
    _statistics = statistics;
}

void SumOfInfeasibilitiesManager::getReducedCost( PiecewiseLinearConstraint *
                                                  plConstraint, double
                                                  &reducedCost, PhaseStatus
                                                  &phaseOfReducedCost ) const
{
    ASSERT( _currentPhasePattern.exists( plConstraint ) );

    PhaseStatus currentPhase = _currentPhasePattern[plConstraint];
    List<PhaseStatus> allPhases = plConstraint->getAllCases();
    allPhases.erase( currentPhase );
    ASSERT( allPhases.size() > 0 ); // Otherwise, the constraint must be fixed.

    LinearExpression costComponent;
    plConstraint->getCostFunctionComponent( costComponent, currentPhase );
    double currentCost = costComponent.evaluate( _currentAssignment );

    reducedCost = FloatUtils::infinity();
    phaseOfReducedCost = PHASE_NOT_FIXED;
    for ( const auto &phase : allPhases )
    {
        LinearExpression otherCostComponent;
        plConstraint->getCostFunctionComponent( otherCostComponent, phase );
        double otherCost = otherCostComponent.evaluate( _currentAssignment );
        double currentReducedCost = currentCost - otherCost;
        if ( FloatUtils::lt( currentReducedCost, reducedCost ) )
        {
            reducedCost = currentReducedCost;
            phaseOfReducedCost = phase;
        }
    }
    ASSERT( reducedCost != FloatUtils::infinity() &&
            phaseOfReducedCost != PHASE_NOT_FIXED );
}
