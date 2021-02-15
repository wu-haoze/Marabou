/*********************                                                        */
/*! \file GlobalConfiguration.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Parth Shah, Derek Huang
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __GlobalConfiguration_h__
#define __GlobalConfiguration_h__

#include "DivideStrategy.h"

class GlobalConfiguration
{
public:
    static void print();

    static const double SOFTMAX_BETA;

    static const double EXPONENTIAL_MOVING_AVERAGE_ALPHA;

    static const bool USE_POLARITY_BASED_DIRECTION_HEURISTICS;

    // The default epsilon used for comparing doubles
    static const double DEFAULT_EPSILON_FOR_COMPARISONS;

    // The precision level when convering doubles to strings
    static const unsigned DEFAULT_DOUBLE_TO_STRING_PRECISION;

    // How often should the main loop print statistics?
    static const unsigned STATISTICS_PRINTING_FREQUENCY;

    static const DivideStrategy SPLITTING_HEURISTICS;

    // The frequency to use interval splitting when largest interval splitting strategy is in use.
    static const unsigned INTERVAL_SPLITTING_FREQUENCY;

    // When automatically deciding which splitting strategy to use, we use relu-splitting if
    // the number of inputs is larger than this number.
    static const unsigned INTERVAL_SPLITTING_THRESHOLD;

    // When the row bound tightener is asked to run until saturation, it can enter an infinite loop
    // due to tiny increments in bounds. This number limits the number of iterations it can perform.
    static const unsigned ROW_BOUND_TIGHTENER_SATURATION_ITERATIONS;

    // Sparse ForrestTomlin diagonal element tolerance constant
    static const double SPARSE_FORREST_TOMLIN_DIAGONAL_ELEMENT_TOLERANCE;

    // Toggle query-preprocessing on/off.
    static const bool PREPROCESS_INPUT_QUERY;

    // Assuming the preprocessor is on, toggle whether or not it will attempt to perform variable
    // elimination.
    static const bool PREPROCESSOR_ELIMINATE_VARIABLES;

    // Assuming the preprocessor is on, toggle whether or not PL constraints will be called upon
    // to add auxiliary variables and equations.
    static const bool PREPROCESSOR_PL_CONSTRAINTS_ADD_AUX_EQUATIONS;

    // If the difference between a variable's lower and upper bounds is smaller than this
    // threshold, the preprocessor will treat it as fixed.
    static const double PREPROCESSOR_ALMOST_FIXED_THRESHOLD;

    // If the flag is true, the preprocessor will try to merge two
    // logically-consecutive weighted sum layers into a single
    // weighted sum layer, to reduce the number of variables
    static const bool PREPROCESSOR_MERGE_CONSECUTIVE_WEIGHTED_SUMS;

    // Try to set the initial tableau assignment to an assignment that is legal with
    // respect to the input network.
    static const bool WARM_START;

    // The tolerance for checking whether f = Relu( b )
    static const double RELU_CONSTRAINT_COMPARISON_TOLERANCE;

    // The tolerance for checking whether f = Abs( b )
    static const double ABS_CONSTRAINT_COMPARISON_TOLERANCE;

    // Should the initial basis be comprised only of auxiliary (row) variables?
    static const bool ONLY_AUX_INITIAL_BASIS;

    static const double GAUSSIAN_ELIMINATION_PIVOT_SCALE_THRESHOLD;

    /*
      Explicit (Reluplex-style) bound tightening options
    */

    static const unsigned EXPLICIT_BOUND_TIGHTENING_DEPTH_THRESHOLD;

    enum ExplicitBasisBoundTighteningType {
        // Compute the inverse basis matrix and use it
        COMPUTE_INVERTED_BASIS_MATRIX = 0,
        // Use the inverted basis matrix without using it, via transformations
        USE_IMPLICIT_INVERTED_BASIS_MATRIX = 1,
        // Disable explicit basis bound tightening
        DISABLE_EXPLICIT_BASIS_TIGHTENING = 2,
    };

    // When doing bound tightening using the explicit basis matrix, should the basis matrix be inverted?
    static const ExplicitBasisBoundTighteningType EXPLICIT_BASIS_BOUND_TIGHTENING_TYPE;

    // When doing explicit bound tightening, should we repeat until saturation?
    static const bool EXPLICIT_BOUND_TIGHTENING_UNTIL_SATURATION;

    /*
      Constraint fixing heuristics
    */

    // When a PL constraint proposes a fix that affects multiple variables, should it first query
    // for any relevant linear connections between the variables?
    static const bool USE_SMART_FIX;

    // A heuristic for selecting which of the broken PL constraints will be repaired next. In this case,
    // the one that has been repaired the least number of times so far.
    static const bool USE_LEAST_FIX;

    /*
      Basis factorization options
    */

    // The number of accumualted eta matrices, after which the basis will be refactorized
	static const unsigned REFACTORIZATION_THRESHOLD;

    // The kind of basis factorization algorithm in use
    enum BasisFactorizationType {
        LU_FACTORIZATION,
        SPARSE_LU_FACTORIZATION,
        FORREST_TOMLIN_FACTORIZATION,
        SPARSE_FORREST_TOMLIN_FACTORIZATION,
    };
    static const BasisFactorizationType BASIS_FACTORIZATION_TYPE;

    /* In the polarity-based branching heuristics, only this many earliest nodes
       are considered to branch on.
    */
    static const unsigned POLARITY_CANDIDATES_THRESHOLD;

    /* The max number of DnC splits
    */
    static const unsigned DNC_DEPTH_THRESHOLD;

#ifdef ENABLE_GUROBI
    /*
      The number of threads Gurobi spawns
    */
    static const unsigned GUROBI_NUMBER_OF_THREADS;
    static const bool GUROBI_LOGGING;
#endif // ENABLE_GUROBI

    /*
      Logging options
    */
    static const bool DNC_MANAGER_LOGGING;
    static const bool ENGINE_LOGGING;
    static const bool TABLEAU_LOGGING;
    static const bool SMT_CORE_LOGGING;
    static const bool BASIS_FACTORIZATION_LOGGING;
    static const bool PREPROCESSOR_LOGGING;
    static const bool INPUT_QUERY_LOGGING;
    static const bool GAUSSIAN_ELIMINATION_LOGGING;
    static const bool QUERY_LOADER_LOGGING;
    static const bool NETWORK_LEVEL_REASONER_LOGGING;
    static const bool PLCONSTRAINT_LOGGING;
    static const bool LOCAL_SEARCH_LOGGING;
    static const bool HEURISTIC_COST_MANAGER_LOGGING;
    static const bool PSEUDO_COST_TRACKER_LOGGING;
};

#endif // __GlobalConfiguration_h__

//
// Local Variables:
// compile-command: "make -C .. "
// tags-file-name: "../TAGS"
// c-basic-offset: 4
// End:
//
