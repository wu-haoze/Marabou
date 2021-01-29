/*********************                                                        */
/*! \file Engine.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling, Andrew Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __Engine_h__
#define __Engine_h__

#include "AutoConstraintBoundTightener.h"
#include "AutoCostFunctionManager.h"
#include "AutoRowBoundTightener.h"
#include "AutoTableau.h"
#include "BoundManager.h"
#include "context/context.h"
#include "DivideStrategy.h"
#include "SnCDivideStrategy.h"
#include "GlobalConfiguration.h"
#include "GurobiWrapper.h"
#include "IEngine.h"
#include "InputQuery.h"
#include "Map.h"
#include "MILPEncoder.h"
#include "Preprocessor.h"
#include "SignalHandler.h"
#include "SmtCore.h"
#include "Statistics.h"
#include "SymbolicBoundTighteningType.h"

#include "Vector.h"

#include <atomic>

#ifdef _WIN32
#undef ERROR
#endif

#define ENGINE_LOG(x, ...) LOG(GlobalConfiguration::ENGINE_LOGGING, "Engine: %s\n", x)
#define SOI_LOG(x, ...) LOG(GlobalConfiguration::LOCAL_SEARCH_LOGGING, "Local search: %s\n", x)

class EngineState;
class InputQuery;
class PiecewiseLinearConstraint;
class String;

class Engine : public IEngine, public SignalHandler::Signalable
{
public:
    Engine();

    /*
      Attempt to find a feasible solution for the input within a time limit
      (a timeout of 0 means no time limit). Returns true if found, false if infeasible.
    */
    bool solve( unsigned timeoutInSeconds = 0 );

    /*
      Process the input query and pass the needed information to the
      underlying tableau. Return false if query is found to be infeasible,
      true otherwise.
     */
    bool processInputQuery( InputQuery &inputQuery );
    bool processInputQuery( InputQuery &inputQuery, bool preprocess );

    /*
      If the query is feasiable and has been successfully solved, this
      method can be used to extract the solution.
     */
    void extractSolution( InputQuery &inputQuery );

    /*
      Methods for storing and restoring the state of the engine.
    */
    void storeTableauState( TableauState &state ) const;
    void restoreTableauState( const TableauState &state );
    void storeState( EngineState &state, bool storeAlsoTableauState ) const;
    void restoreState( const EngineState &state );
    void setNumPlConstraintsDisabledByValidSplits( unsigned numConstraints );

    /*
      A request from the user to terminate
    */
    void quitSignal();

    const Statistics *getStatistics() const;

    InputQuery *getInputQuery();

    /*
      Get the exit code
    */
    Engine::ExitCode getExitCode() const;

    /*
      Get the quitRequested flag
    */
    std::atomic_bool *getQuitRequested();

    /*
      Get the list of input variables
    */
    List<unsigned> getInputVariables() const;

    /*
      Add equations and tightenings from a split.
    */
    void applySplit( const PiecewiseLinearCaseSplit &split );

    /*
      Reset the state of the engine, before solving a new query
      (as part of DnC mode).
    */
    void reset();

    /*
      Reset the statistics object
    */
    void resetStatistics();

    /*
      Clear the violated PL constraints
    */
    void clearViolatedPLConstraints();

    /*
      Set the Engine's level of verbosity
    */
    void setVerbosity( unsigned verbosity );

    /*
      Pick the piecewise linear constraint for splitting
    */
    PiecewiseLinearConstraint *pickSplitPLConstraint();

    /*
      Call-back from QueryDividers
      Pick the piecewise linear constraint for splitting
    */
    PiecewiseLinearConstraint *pickSplitPLConstraintSnC( SnCDivideStrategy strategy );

    /*
      PSA: The following two methods are for DnC only and should be used very
      cautiously.
     */
    void resetSmtCore();
    void resetExitCode();
    void resetBoundTighteners();

private:
    enum BasisRestorationRequired {
        RESTORATION_NOT_NEEDED = 0,
        STRONG_RESTORATION_NEEDED = 1,
        WEAK_RESTORATION_NEEDED = 2
    };

    enum BasisRestorationPerformed {
        NO_RESTORATION_PERFORMED = 0,
        PERFORMED_STRONG_RESTORATION = 1,
        PERFORMED_WEAK_RESTORATION = 2,
    };

    /*
      CVC4 Context Data structure
    */
    CVC4::context::Context _context;

    BoundManager _boundManager;

    /*
      Collect and print various statistics.
    */
    Statistics _statistics;

    /*
      The tableau object maintains the equations, assignments and bounds.
    */
    AutoTableau _tableau;

    /*
      The existing piecewise-linear constraints.
    */
    List<PiecewiseLinearConstraint *> _plConstraints;

    /*
      Piecewise linear constraints that are currently violated.
    */
    Vector<PiecewiseLinearConstraint *> _violatedPlConstraints;

    /*
      Preprocessed InputQuery
    */
    InputQuery _preprocessedQuery;

    /*
      Bound tightener.
    */
    AutoRowBoundTightener _rowBoundTightener;

    /*
      The SMT engine is in charge of case splitting.
    */
    SmtCore _smtCore;

    /*
      Number of pl constraints disabled by valid splits.
    */
    unsigned _numPlConstraintsDisabledByValidSplits;

    /*
      Query preprocessor.
    */
    Preprocessor _preprocessor;

    /*
      Is preprocessing enabled?
    */
    bool _preprocessingEnabled;

    /*
      Is the initial state stored?
    */
    bool _initialStateStored;

    /*
      Work memory (of size m)
    */
    double *_work;

    /*
      Indicates a user/DnCManager request to quit
    */
    std::atomic_bool _quitRequested;

    /*
      A code indicating how the run terminated.
    */
    ExitCode _exitCode;

    /*
      An object in charge of managing bound tightenings
      proposed by the PiecewiseLinearConstriants.
    */
    AutoConstraintBoundTightener _constraintBoundTightener;

    /*
      The number of visited states when we performed the previous
      restoration. This field serves as an indication of whether or
      not progress has been made since the previous restoration.
    */
    unsigned long long _numVisitedStatesAtPreviousRestoration;

    /*
      An object that knows the topology of the network being checked,
      and can be used for various operations such as network
      evaluation of topology-based bound tightening.
     */
    NLR::NetworkLevelReasoner *_networkLevelReasoner;

    /*
      Verbosity level:
      0: print out minimal information
      1: print out statistics only in the beginning and the end
      2: print out statistics during solving
    */
    unsigned _verbosity;

    /*
      Strategy used for internal splitting
    */
    DivideStrategy _splittingStrategy;

    /*
      Type of symbolic bound tightening
    */
    SymbolicBoundTighteningType _symbolicBoundTighteningType;

    /*
      Disjunction that is used for splitting but doesn't exist in the beginning
    */
    std::unique_ptr<PiecewiseLinearConstraint> _disjunctionForSplitting;

    /*
      Solve the query with MILP encoding
    */
    bool _solveWithMILP;

    /*
      GurobiWrapper object
    */
    std::unique_ptr<GurobiWrapper> _gurobi;

    /*
      MILPEncoder
    */
    std::unique_ptr<MILPEncoder> _milpEncoder;

    /*
      Return true iff all variables are within bounds.
     */
    bool allVarsWithinBounds() const;

    /*
      Collect all violated piecewise linear constraints.
    */
    void collectViolatedPlConstraints();

    /*
      Return true iff all piecewise linear constraints hold.
    */
    bool allPlConstraintsHold();

    /*
      Apply all bound tightenings (row and matrix-based) in
      the queue.
    */
    void applyAllBoundTightenings();

    /*
      Apply any bound tightenings found by the row tightener.
    */
    void applyAllRowTightenings();

    /*
      Apply any bound tightenings entailed by the constraints.
    */
    void applyAllConstraintTightenings();

    /*
      Apply all valid case splits proposed by the constraints.
      Return true if a valid case split has been applied.
    */
    bool applyAllValidConstraintCaseSplits();
    bool applyValidConstraintCaseSplit( PiecewiseLinearConstraint *constraint );

    /*
      Update statitstics, print them if needed.
    */
    void mainLoopStatistics();

    /*
      Perform bound tightening on the constraint matrix A.
    */
    void tightenBoundsOnConstraintMatrix();

    /*
      Perform a round of symbolic bound tightening, taking into
      account the current state of the piecewise linear constraints.
    */
    void performSymbolicBoundTightening();

    /*
      Check whether a timeout value has been provided and exceeded.
    */
    bool shouldExitDueToTimeout( unsigned timeout ) const;

    /*
      Helper functions for input query preprocessing
    */
    void informConstraintsOfInitialBounds( InputQuery &inputQuery ) const;
    void invokePreprocessor( const InputQuery &inputQuery, bool preprocess );
    void printInputBounds( const InputQuery &inputQuery ) const;
    void removeRedundantEquations( const double *constraintMatrix );
    void selectInitialVariablesForBasis( const double *constraintMatrix, List<unsigned> &initialBasis, List<unsigned> &basicRows );
    void initializeTableau( const double *constraintMatrix, const List<unsigned> &initialBasis );
    void initializeNetworkLevelReasoning();
    double *createConstraintMatrix();
    void addAuxiliaryVariables();
    void augmentInitialBasisIfNeeded( List<unsigned> &initialBasis, const List<unsigned> &basicRows );
    void performMILPSolverBoundedTightening();

    /*
      Update the preferred direction to perform fixes and the preferred order
      to handle case splits
    */
    void updateDirections();

    /*
      Among the earliest K ReLUs, pick the one with Polarity closest to 0.
      K is equal to GlobalConfiguration::POLARITY_CANDIDATES_THRESHOLD
    */
    PiecewiseLinearConstraint *pickSplitPLConstraintBasedOnPolarity();

    PiecewiseLinearConstraint *pickSplitPLConstraintBABSR();

    /*
      Pick the first unfixed ReLU in the topological order
    */
    PiecewiseLinearConstraint *pickSplitPLConstraintBasedOnTopology();

    /*
      Pick the input variable with the largest interval
    */
    PiecewiseLinearConstraint *pickSplitPLConstraintBasedOnIntervalWidth();

    /*
      Solve the input query with a MILP solver (Gurobi)
    */
    bool solveWithMILPEncoding( unsigned timeoutInSeconds );

    /*
      Extract the satisfying assignment from the MILP solver
    */
    void extractSolutionFromGurobi( InputQuery &inputQuery );

    bool concretizeAndCheckInputAssignment();

    /*
      Evaluate the input assignment in the tableau with the network-level reasoner.
    */
    void concretizeInputAssignment();

    /*
      Check whether the assignment from the network level reasoner is a valid one.
      If so, store the assignment in the tableau.
    */
    bool checkAssignmentFromNetworkLevelReasoner();

    /*
      Perform bound tightening operations that require
      access to the explicit basis matrix.
    */
    void explicitBasisBoundTightening();

    /*
      Check whether the assignment is satisfying for the inputQuery.
    */
    bool checkAssignment( InputQuery &inputQuery,
                          const Map<unsigned, double> assignments );

    /****************************** local search ****************************/
    void pushContext();

    void popContext();

    /*
      Copy of the original input query
    */
    InputQuery _originalInputQuery;
    bool _solutionFoundAndStoredInOriginalQuery;

    /*
      Seed for random stuff
    */
    unsigned _seed;

    /*
      The probability to use a noise strategy in local search
    */
    float _noiseParameter;

    /*
      The strategies of local search
    */
    String _flippingStrategy;
    String _initializationStrategy;

    Map<unsigned, double> _heuristicCost;

    Vector<PiecewiseLinearConstraint *> _plConstraintsInHeuristicCost;

    bool solveWithGurobi( unsigned timeoutInSeconds );

    /*
      Performs local search at the search level.
      Either throws InfeasibleQueryException,
      or return false with _needToSplit set to true and a branching variable picked.
      or return true with satisfying solution stored in the tableau.
    */
    bool performLocalSearch();

    /*
      Create the initial cost function for local search
    */
    void initiateCostFunctionForLocalSearch();

    /*
      Based on current assignment
    */
    void initiateCostFunctionForLocalSearchBasedOnCurrentAssignment
    ( const List<PiecewiseLinearConstraint *> &plConstraintsToAdd );

    /*
      Based on input assignment
    */
    void initiateCostFunctionForLocalSearchBasedOnInputAssignment
    ( const List<PiecewiseLinearConstraint *> &plConstraintsToAdd );

    /*
      Pick a phase at uniform random
    */
    void initiateCostFunctionForLocalSearchRandomly
    ( const List<PiecewiseLinearConstraint *> &plConstraintsToAdd );

    // Optimize w.r.t. the current heuristic cost function
    void optimizeForHeuristicCost();

    /*
      Called when local optima is reached but not all PLConstraint is satisfied.

      There are two scenarios:
      scenario 1:
         If the local optima is not zero, we flip the cost term for certain PLConstraint already in the cost function.

      scenario 2:
          If the local optima is zero, we add more PLConstraints to the cost function.
    */
    void updateHeuristicCost();

    /*
      Heuristic to flip the cost component of a PLConstraint
      following the heuristics from WalkSAT
      flip the cost term of the first PLConstraint that reduces the cost

      if not available,
      with probability p, flip the cost term of the first PLConstraint that increases the cost the least.
      with probability 1- p, flip the cost term of a randomly chosen PLConstraint
    */
    void updateHeuristicCostWalkSAT();

    /*
      Heuristic to flip the cost component of a PLConstraint:
      following the heuristics from
      https://www.researchgate.net/publication/2637561_Noise_Strategies_for_Improving_Local_Search
      with probability p, flip the cost term of a randomly chosen PLConstraint
      with probability 1 - p, flip the cost term of the PLConstraint that reduces in the greatest decline in the cost
    */
    void updateHeuristicCostGWSAT();

    /*
      SOI helper functions
    */
    // Go through the cost term for each PLConstraint, check whether it is satisfied.
    // If it is satisfied but the cost term is not zero, flip the cost term so that
    // the cost term is zero.
    void updateCostTermsForSatisfiedPLConstraints();

    // Notify the plConstraints of the assignments from Gurobi
    void notifyPLConstraintsAssignments();

    /*
      For SOI Debugging
    */
    void dumpHeuristicCost();
    double computeHeuristicCost();

};

#endif // __Engine_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
