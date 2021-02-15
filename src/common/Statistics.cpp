/*********************                                                        */
/*! \file Statistics.cpp
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

#include "FloatUtils.h"
#include "Statistics.h"
#include "TimeUtils.h"

Statistics::Statistics()
    : _timedOut( false )
{
    /*************************** Unsigned Attributes **************************/
    // Overall
    _unsignedAttributes[NUM_PIECEWISE_LINEAR_CONSTRAINTS] = 0;
    _unsignedAttributes[NUM_ACTIVE_PIECEWISE_LINEAR_CONSTRAINTS] = 0;

    // Preprocessing
    _unsignedAttributes[NUM_EQUATIONS_REMOVED_BY_PREPROCESSING] = 0;
    _unsignedAttributes[NUM_CONSTRAINTS_REMOVED_BY_PREPROCESSING] = 0;
    _unsignedAttributes[NUM_VARIABLES_REMOVED_BY_PREPROCESSING] = 0;

    // Search
    _unsignedAttributes[CURRENT_STACK_DEPTH] = 0;
    _unsignedAttributes[NUM_VISITED_TREE_STATES] = 1;

    /***************************** Long Attributes ****************************/
     // Overall
    _longAttributes[NUM_MAIN_LOOP_ITERATIONS] = 0;
    _longAttributes[TIME_MAIN_LOOP_MICRO] = 0;

    // Preprocessing
    _longAttributes[TIME_PREPROCESSING_MICRO] = 0;

    // Search
    _longAttributes[TIME_BRANCHING_HEURISTICS_MICRO] = 0;
    _longAttributes[TIME_SMT_CORE_PUSH_MICRO] = 0;
    _longAttributes[TIME_SMT_CORE_POP_MICRO] = 0;
    _longAttributes[TIME_CHECKING_QUIT_CONDITION_MICRO] = 0;

    // Simplex
    _longAttributes[NUM_SIMPLIEX_STEPS] = 0;
    _longAttributes[NUM_SIMPLIEX_CALLS] = 0;
    _longAttributes[NUM_PROPOSED_FLIPS] = 0;
    _longAttributes[NUM_REJECTED_FLIPS] = 0;
    _longAttributes[NUM_ACCEPTED_FLIPS] = 0;
    _longAttributes[TIME_SIMPLEX_STEPS_MICRO] = 0;
    _longAttributes[TIME_COMPUTE_HEURISTIC_COST_MICRO] = 0;
    _longAttributes[TIME_UPDATING_COST_FUNCTION_MICRO] = 0;
    _longAttributes[TIME_COLLECTING_VIOLATED_PLCONSTRAINT_MICRO] = 0;
    _longAttributes[TIME_ADDING_CONSTRAINTS_TO_LP_SOLVER_MICRO] = 0;

    // Tightening
    _longAttributes[NUM_COST_LEMMAS] = 0;
    _longAttributes[NUM_EXPLICIT_BASIS_BOUND_TIGHTENING_ATTEMPT] = 0;
    _longAttributes[NUM_CONSTRAINT_MATRIX_BOUND_TIGHTENING_ATTEMPT] = 0;
    _longAttributes[NUM_SYMBOLIC_BOUND_TIGHTENING_ATTEMPT] = 0;
    _longAttributes[NUM_LP_BOUND_TIGHTENING_ATTEMPT] = 0;
    _longAttributes[NUM_EXPLICIT_BASIS_BOUND_TIGHTENING] = 0;
    _longAttributes[NUM_CONSTRAINT_MATRIX_BOUND_TIGHTENING] = 0;
    _longAttributes[NUM_SYMBOLIC_BOUND_TIGHTENING] = 0;
    _longAttributes[NUM_LP_BOUND_TIGHTENING] = 0;
    _longAttributes[TIME_EXPLICIT_BASIS_BOUND_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_CONSTRAINT_MATRIX_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_SYMBOLIC_BOUND_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_LP_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_PERFORMING_VALID_CASE_SPLITS_MICRO] = 0;

    // Statistics
    _longAttributes[TIME_HANDLING_STATISTICS_MICRO] = 0;

    /***************************** Double Attributes ****************************/
    _doubleAttributes[MINIMAL_COST_SO_FAR] = 0;
}

void Statistics::resetTimeStatsForMainLoop()
{
    /***************************** Long Attributes ****************************/
     // Overall
    _longAttributes[TIME_MAIN_LOOP_MICRO] = 0;

    // Search
    _longAttributes[TIME_BRANCHING_HEURISTICS_MICRO] = 0;
    _longAttributes[TIME_SMT_CORE_PUSH_MICRO] = 0;
    _longAttributes[TIME_SMT_CORE_POP_MICRO] = 0;
    _longAttributes[TIME_CHECKING_QUIT_CONDITION_MICRO] = 0;

    // Simplex
    _longAttributes[TIME_SIMPLEX_STEPS_MICRO] = 0;
    _longAttributes[TIME_COMPUTE_HEURISTIC_COST_MICRO] = 0;
    _longAttributes[TIME_UPDATING_COST_FUNCTION_MICRO] = 0;
    _longAttributes[TIME_COLLECTING_VIOLATED_PLCONSTRAINT_MICRO] = 0;
    _longAttributes[TIME_ADDING_CONSTRAINTS_TO_LP_SOLVER_MICRO] = 0;

    // Tightening
    _longAttributes[TIME_EXPLICIT_BASIS_BOUND_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_CONSTRAINT_MATRIX_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_SYMBOLIC_BOUND_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_LP_TIGHTENING_MICRO] = 0;
    _longAttributes[TIME_PERFORMING_VALID_CASE_SPLITS_MICRO] = 0;

    // Statistics
    _longAttributes[TIME_HANDLING_STATISTICS_MICRO] = 0;
}


void Statistics::print()
{
    printf( "\n%s Statistics update:\n", TimeUtils::now().ascii() );

    struct timespec now = TimeUtils::sampleMicro();

    unsigned long long totalElapsed = TimeUtils::timePassed( _startTime, now );

    unsigned seconds = totalElapsed / 1000000;
    unsigned minutes = seconds / 60;
    unsigned hours = minutes / 60;

    printf( "\t--- Time Statistics ---\n" );
    printf( "\tTotal time elapsed: %llu milli (%02u:%02u:%02u)\n",
            totalElapsed / 1000, hours, minutes - ( hours * 60 ), seconds - ( minutes * 60 ) );

    unsigned long long timeMainLoopMicro = getLongAttr( TIME_MAIN_LOOP_MICRO );
    seconds = timeMainLoopMicro / 1000000;
    minutes = seconds / 60;
    hours = minutes / 60;
    printf( "\t\tMain loop: %llu milli (%02u:%02u:%02u)\n",
            timeMainLoopMicro / 1000, hours, minutes - ( hours * 60 ), seconds - ( minutes * 60 ) );

    unsigned long long timePreprocessingMicro = getLongAttr( TIME_PREPROCESSING_MICRO );
    seconds = timePreprocessingMicro / 1000000;
    minutes = seconds / 60;
    hours = minutes / 60;
    printf( "\t\tPreprocessing time: %llu milli (%02u:%02u:%02u)\n",
            timePreprocessingMicro / 1000, hours, minutes - ( hours * 60 ), seconds - ( minutes * 60 ) );

    unsigned long long totalUnknown = totalElapsed - timeMainLoopMicro - timePreprocessingMicro;
    seconds = totalUnknown / 1000000;
    minutes = seconds / 60;
    hours = minutes / 60;
    printf( "\t\tUnknown: %llu milli (%02u:%02u:%02u)\n",
            totalUnknown / 1000, hours, minutes - ( hours * 60 ), seconds - ( minutes * 60 ) );

    printf( "\tBreakdown for main loop:\n" );
    unsigned long long total = 0;

    unsigned long long val = getLongAttr( TIME_SMT_CORE_PUSH_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] SMT core push: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_SMT_CORE_POP_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] SMT core pop: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_BRANCHING_HEURISTICS_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Picking branching variable: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_SIMPLEX_STEPS_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Simplex steps: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_ADDING_CONSTRAINTS_TO_LP_SOLVER_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Adding constraints to lp solver: %llu milli.\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_CHECKING_QUIT_CONDITION_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Checking quit condition: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_COMPUTE_HEURISTIC_COST_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Computing Heuristic cost: %llu milli.\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_UPDATING_COST_FUNCTION_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Updating Cost Function: %llu milli.\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_EXPLICIT_BASIS_BOUND_TIGHTENING_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Explicit-basis bound tightening: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_CONSTRAINT_MATRIX_TIGHTENING_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Constraint-matrix bound tightening: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_SYMBOLIC_BOUND_TIGHTENING_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Symbolic Bound Tightening: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ) , val / 1000 );

    val = getLongAttr( TIME_LP_TIGHTENING_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] LP-based Bound Tightening: %llu milli\n"
            , printPercents( val, timeMainLoopMicro ) , val / 1000 );

    val = getLongAttr( TIME_PERFORMING_VALID_CASE_SPLITS_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Valid case splits: %llu milli.\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_COLLECTING_VIOLATED_PLCONSTRAINT_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Collecting violated PlConstraints: %llu milli.\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    val = getLongAttr( TIME_HANDLING_STATISTICS_MICRO );
    total += val;
    printf( "\t\t[%.2lf%%] Handling statistics: %llu milli.\n"
            , printPercents( val, timeMainLoopMicro ), val / 1000 );

    printf( "\t\t[%.2lf%%] Unaccounted for: %llu milli\n"
            , printPercents( timeMainLoopMicro - total,
                             timeMainLoopMicro )
            , timeMainLoopMicro >
            total ? ( timeMainLoopMicro - total ) / 1000 : 0
            );

    printf( "\t--- Preprocessor Statistics ---\n" );
    printf( "\tNumber of eliminated variables: %u\n",
            getUnsignedAttr( NUM_VARIABLES_REMOVED_BY_PREPROCESSING ) );
    printf( "\tNumber of constraints removed due to variable elimination: %u\n",
            getUnsignedAttr( NUM_CONSTRAINTS_REMOVED_BY_PREPROCESSING ) );
    printf( "\tNumber of equations removed due to variable elimination: %u\n",
            getUnsignedAttr( NUM_EQUATIONS_REMOVED_BY_PREPROCESSING ) );

    printf( "\t--- Engine Statistics ---\n" );
    printf( "\tNumber of main loop iterations: %llu\n"
            , getLongAttr( NUM_MAIN_LOOP_ITERATIONS )
            );
    printf( "\tNumber of active piecewise-linear constraints: %u / %u\n"
            , getUnsignedAttr( NUM_ACTIVE_PIECEWISE_LINEAR_CONSTRAINTS )
            , getUnsignedAttr( NUM_PIECEWISE_LINEAR_CONSTRAINTS )
            );

    printf( "\t--- SmtCore Statistics ---\n" );
    printf( "\tCurrent depth is %u. Total visited states: %u. \n"
            , getUnsignedAttr( CURRENT_STACK_DEPTH )
            , getUnsignedAttr( NUM_VISITED_TREE_STATES ) );

    printf( "\t--- Bound Tightening Statistics ---\n" );
    printf( "\t\tNumber of added cost lemmas: %llu.\n"
            , getLongAttr( NUM_COST_LEMMAS ) );

    printf( "\t\tNumber of explicit basis matrices examined by row tightener: %llu. Consequent tightenings: %llu\n"
            , getLongAttr( NUM_EXPLICIT_BASIS_BOUND_TIGHTENING_ATTEMPT )
            , getLongAttr( NUM_EXPLICIT_BASIS_BOUND_TIGHTENING ) );

    printf( "\t\tNumber of bound tightening rounds on the entire constraint matrix: %llu. "
            "Consequent tightenings: %llu(%.1f per millisecond).\n"
            , getLongAttr( NUM_CONSTRAINT_MATRIX_BOUND_TIGHTENING_ATTEMPT )
            , getLongAttr( NUM_CONSTRAINT_MATRIX_BOUND_TIGHTENING )
            , printAverage( getLongAttr( NUM_CONSTRAINT_MATRIX_BOUND_TIGHTENING ),
                            getLongAttr( TIME_CONSTRAINT_MATRIX_TIGHTENING_MICRO ) / 1000 ) );

    printf( "\t\tNumber of Symbolic Bound Tightening Rounds: %llu. Tightenings proposed: %llu"
            "(%.1f per millisecond).\n"
            , getLongAttr( NUM_SYMBOLIC_BOUND_TIGHTENING_ATTEMPT )
            , getLongAttr( NUM_SYMBOLIC_BOUND_TIGHTENING )
            , printAverage( getLongAttr( NUM_SYMBOLIC_BOUND_TIGHTENING ),
                            getLongAttr( TIME_SYMBOLIC_BOUND_TIGHTENING_MICRO ) / 1000 ) );

    printf( "\t\tNumber of MILP  Bound Tightening Rounds: %llu. Tightenings proposed: %llu\n"
            , getLongAttr( NUM_LP_BOUND_TIGHTENING_ATTEMPT )
            , getLongAttr( NUM_LP_BOUND_TIGHTENING ) );

    printf( "\t--- Simplex Statistics ---\n" );
    printf( "\tNumber of Simplex steps performed: %llu. \n"
            , getLongAttr( NUM_SIMPLIEX_STEPS ) );
    printf( "\tNumber of Simplex calls: %llu. \n"
            , getLongAttr( NUM_SIMPLIEX_CALLS ) );

    unsigned long long numProposed = getLongAttr( NUM_PROPOSED_FLIPS );
    unsigned long long numAccepted = getLongAttr( NUM_ACCEPTED_FLIPS );
    unsigned long long numRejected = getLongAttr( NUM_REJECTED_FLIPS );

    printf( "\tFlip proposed %llu. Acceptance rate: %llu (%.2lf%%). Flip rejected: %llu. \n"
            , numProposed
            , numAccepted
            , printPercents( numAccepted, numProposed )
            , numRejected );

    struct timespec end = TimeUtils::sampleMicro();
    _longAttributes[TIME_HANDLING_STATISTICS_MICRO] += TimeUtils::timePassed( now, end );
}

void Statistics::stampStartingTime()
{
    _startTime = TimeUtils::sampleMicro();
}

unsigned long long Statistics::getTotalTime() const
{
    struct timespec now = TimeUtils::sampleMicro();
    return TimeUtils::timePassed( _startTime, now );
}

void Statistics::timeout()
{
    _timedOut = true;
}

bool Statistics::hasTimedOut() const
{
    return _timedOut;
}

double Statistics::printPercents( unsigned long long part, unsigned long long total ) const
{
    if ( total == 0 )
        return 0;

    return 100.0 * part / total;
}

double Statistics::printAverage( unsigned long long part, unsigned long long total ) const
{
    if ( total == 0 )
        return 0;

    return (double)part / total;
}
