/*********************                                                        */
/*! \file SumOfInfeasibilitiesManager.h
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

#ifndef __SumOfInfeasibilitiesManager_h__
#define __SumOfInfeasibilitiesManager_h__

#include "GlobalConfiguration.h"
#include "InputQuery.h"
#include "ITableau.h"
#include "LinearExpression.h"
#include "List.h"
#include "NetworkLevelReasoner.h"
#include "PiecewiseLinearConstraint.h"
#include "SoIInitializationStrategy.h"
#include "SoISearchStrategy.h"
#include "Statistics.h"
#include "Vector.h"

#include "T/stdlib.h"

#define SOI_LOG( x, ... ) LOG( GlobalConfiguration::SOI_LOGGING, "SoIManager: %s\n", x )

class SumOfInfeasibilitiesManager
{
public:

    SumOfInfeasibilitiesManager( const InputQuery &inputQuery, const ITableau
                                 &tableau );

    /*
      Returns the actual current phase pattern from _currentPhasePattern
    */
    LinearExpression getSoIPhasePattern() const;

    /*
      Returns the actual proposed phase pattern from _currentPhasePattern
      and _currentProposal
    */
    LinearExpression getProposedSoIPhasePattern() const;

    /*
      Called at the beginning of the local search (DeepSoI).
      Choose the first phase pattern by heuristically taking a cost term
      from each unfixed activation function.
    */
    void initializePhasePattern();

    /*
      Called when the previous heuristic cost cannot be minimized to 0 (i.e., no
      satisfying assignment found for the previous activation pattern).
      In this case, we need to try a new phase pattern. We achieve this by
      propose an update to the previous phase pattern,
      stored in _currentProposal.
    */
    void proposePhasePatternUpdate();

    /*
      The acceptance heuristic is standard: if the newCost is less than
      the current cost, we always accept. Otherwise, the probability
      to accept the proposal is reversely proportional to the difference between
      the newCost and the _costOfcurrentphasepattern.
    */
    bool decideToAcceptCurrentProposal( double costOfCurrentPhasePattern,
                                        double costOfProposedPhasePattern );

    /*
      Update _currentPhasePattern with _currentProposal.
    */
    void acceptCurrentProposal();

    // Go through each PLConstraint, check whether it is satisfied by the
    // current assignment but the cost term is not zero. In that case,
    // we use the cost term corresponding to the phase of the current assignment
    // for that PLConstraint. This way, the overall SoI cost is reduced for free.
    void updateCurrentPhasePatternForSatisfiedPLConstraints();

    // During the Simplex execution, the phase of a piecewise linear constraint
    // might be fixed due to additional tightening.
    // In that case, we remove the cost term for that piecewise linear constraint
    // from the heuristic cost.
    void removeCostComponentFromHeuristicCost( PiecewiseLinearConstraint
                                               *constraint );

    /*
      Obtain the current variable assignment from the Tableau.
    */
    void obtainCurrentAssignment();

    void setStatistics( Statistics *statistics );

    /* For debug use */
    void setPhaseStatusInCurrentPhasePattern( PiecewiseLinearConstraint
                                              *constraint, PhaseStatus phase );

private:
    const List<PiecewiseLinearConstraint *> &_plConstraints;
    // Used for the heuristic initialization of the phase pattern.
    NLR::NetworkLevelReasoner *_networkLevelReasoner;
    unsigned _numberOfVariables;
    // Used for accessing the current variable assignment.
    const ITableau &_tableau;

    // Parameters that controls the local search heuristics
    SoIInitializationStrategy _initializationStrategy;
    SoISearchStrategy _searchStrategy;
    double _probabilityDensityParameter;

    /*
      The representation of the current phase pattern (one linear phase of the
      non-linear SoI function) as a mapping from PLConstraints to phase patterns.
      We do not keep the concrete LinearExpression explicitly but will concretize
      it on the fly. This makes it cheap to update the phase pattern.
    */
    Map<PiecewiseLinearConstraint *, PhaseStatus> _currentPhasePattern;

    /*
      The proposed update to the current phase pattern. For instance, it can
      contain one of the ReLUConstraint in the _currentPhasePattern
      with PhaseStatus flipped.
    */
    Map<PiecewiseLinearConstraint *, PhaseStatus> _currentProposal;

    /*
      The constraints in the current phase pattern (i.e., participating in the
      SoI) stored in a Vector for ease of random access.
    */
    Vector<PiecewiseLinearConstraint *> _plConstraintsInCurrentPhasePattern;

    /*
      A local copy of the current variable assignment, which is refreshed via
      the obtainCurrentAssignment() method.
    */
    Map<unsigned, double> _currentAssignment;

    Statistics *_statistics;

    /*
      Clear _currentPhasePattern and _currentProposal
    */
    void resetPhasePattern();

    /*
      Set _currentPhasePattern according to the current input assignment.
    */
    void initializePhasePatternWithCurrentInputAssignment();

    /*
      Choose one piecewise linear constraint in the current phase pattern
      and set it to a uniform-randomly chosen alternative phase status (for ReLU
      this means we just flip the phase status).
    */
    void proposePhasePatternUpdateRandomly();

    /*
      Iterate over the piecewise linear constraints in the current phase pattern
      to find one with the largest "reduced cost". See the "getReducedCost"
      method below.
      If no constraint has positive reduced cost (we are at a local optima), we
      fall back to proposePhasePatternUpdateRandomly()
    */
    void proposePhasePatternUpdateWalksat();

    /*
      This method computes the reduced cost of a plConstraint participating
      in the phase pattern. The reduced cost is the largest value by which the
      cost (w.r.t. the current assignment) will decrease if we choose a
      different phase for the plConstraint in the phase pattern. This value is
      stored in reducedCost. The phase corresponding to the largest reduction
      is stored in phaseOfReducedCost.
      Note that the phase can be negative, which means the current phase is
      (locally) optimal.
    */
    void getReducedCost( PiecewiseLinearConstraint *plConstraint, double
                         &reducedCost, PhaseStatus &phaseOfReducedCost ) const;
};

#endif // __SumOfInfeasibilitiesManager_h__
