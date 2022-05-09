/*********************                                                        */
/*! \file OptionParser.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include "Debug.h"
#include "OptionParser.h"
#include "Options.h"

OptionParser::OptionParser()
{
    // This constructor should not be called
    ASSERT( false );
}

OptionParser::OptionParser( Map<unsigned, bool> *boolOptions,
                            Map<unsigned, int> *intOptions,
                            Map<unsigned, float> *floatOptions,
                            Map<unsigned, std::string> *stringOptions )
    : _optionDescription( "Supported options" )
    , _boolOptions( boolOptions )
    , _intOptions( intOptions )
    , _floatOptions( floatOptions )
    , _stringOptions( stringOptions )
{
}

void OptionParser::initialize()
{
    // Possible options
    _optionDescription.add_options()
        ( "pl-aux-eq",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::PREPROCESSOR_PL_CONSTRAINTS_ADD_AUX_EQUATIONS]) ),
          "PL constraints generate auxiliary equations" )
        ( "snc",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::DNC_MODE]) ),
          "Use the split-and-conquer solving mode: largest-interval/polarity/auto. default: auto" )
        ( "no-parallel-deepsoi",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::PARALLEL_DEEPSOI]) ),
          "Do not use the parallel deep-soi solving mode when multiple threads are allowed." )
        ( "restore-tree-states",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::RESTORE_TREE_STATES]) ),
          "Restore tree states in SnC mode" )
        ( "dump-bounds",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::DUMP_BOUNDS]) ),
          "Dump the bounds after preprocessing" )
        ( "input",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::INPUT_FILE_PATH]) ),
          "Neural netowrk file" )
        ( "property",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::PROPERTY_FILE_PATH]) ),
          "Property file" )
        ( "input-query",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::INPUT_QUERY_FILE_PATH]) ),
          "Input Query file" )
        ( "summary-file",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::SUMMARY_FILE]) ),
          "Summary file" )
        ( "query-dump-file",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::QUERY_DUMP_FILE]) ),
          "Query dump file" )
        ( "soi-search-strategy",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::SOI_SEARCH_STRATEGY]) ),
          "Strategy for stochastically minimizing the soi: mcmc/walksat. default: mcmc" )
        ( "soi-init-strategy",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::SOI_INITIALIZATION_STRATEGY]) ),
          "Strategy for initialize the soi function: input-assignment/current-assignment. default: input-assignment" )
        ( "num-workers",
          boost::program_options::value<int>( &((*_intOptions)[Options::NUM_WORKERS]) ),
          "(SnC) Number of workers" )
        ( "blas-threads",
          boost::program_options::value<int>( &((*_intOptions)[Options::NUM_BLAS_THREADS]) ),
          "Number of threads to use for matrix multiplication with OpenBLAS" )
        ( "split-strategy",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::SNC_SPLITTING_STRATEGY]) ),
          "(SnC) The splitting strategy" )
        ( "tightening-strategy",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::SYMBOLIC_BOUND_TIGHTENING_TYPE]) ),
          "type of bound tightening technique to use: sbt/deeppoly/none. default: deeppoly" )
        ( "initial-divides",
          boost::program_options::value<int>( &((*_intOptions)[Options::NUM_INITIAL_DIVIDES]) ),
          "(SnC) Number of times to initially bisect the input region" )
        ( "initial-timeout",
          boost::program_options::value<int>( &((*_intOptions)[Options::INITIAL_TIMEOUT]) ),
          "(SnC) The initial timeout" )
        ( "num-online-divides",
          boost::program_options::value<int>( &((*_intOptions)[Options::NUM_ONLINE_DIVIDES]) ),
          "(SnC) Number of times to further bisect a sub-region when a timeout occurs" )
        ( "timeout",
          boost::program_options::value<int>( &((*_intOptions)[Options::TIMEOUT]) ),
          "Global timeout" )
        ( "verbosity",
          boost::program_options::value<int>( &((*_intOptions)[Options::VERBOSITY]) ),
          "Verbosity of engine::solve(). 0: does not print anything (for SnC), 1: print"
          "out statistics in the beginning and end, 2: print out statistics during solving." )
        ( "reluplex-split-threshold",
          boost::program_options::value<int>( &((*_intOptions)[Options::CONSTRAINT_VIOLATION_THRESHOLD]) ),
          "Max number of tries to repair a relu before splitting" )
        ( "branch",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::SPLITTING_STRATEGY]) ),
          "The branching strategy (earliest-relu/pseudo-impact/largest-interval/relu-violation/polarity" )
        ( "soi-split-threshold",
          boost::program_options::value<int>( &((*_intOptions)[Options::DEEP_SOI_REJECTION_THRESHOLD]) ),
          "Max number of rejected phase pattern proposal before splitting" )
        ( "seed",
          boost::program_options::value<int>( &((*_intOptions)[Options::SEED]) ),
          "The random seed." )
        ( "timeout-factor",
          boost::program_options::value<float>( &((*_floatOptions)[Options::TIMEOUT_FACTOR]) ),
          "(SnC) The timeout factor" )
        ( "help",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::HELP]) ),
          "Prints the help message")
        ( "version",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::VERSION]) ),
          "Prints the version number")
         ( "preprocessor-bound-tolerance",
          boost::program_options::value<float>( &((*_floatOptions)[Options::PREPROCESSOR_BOUND_TOLERANCE]) ),
          "epsilon for preprocessor bound tightening comparisons" )
        ( "mcmc-beta",
          boost::program_options::value<float>( &((*_floatOptions)[Options::PROBABILITY_DENSITY_PARAMETER]) ),
          "beta parameter in MCMC search." )
#ifdef ENABLE_GUROBI
        ( "milp",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::SOLVE_WITH_MILP]) ),
          "Use a MILP solver to solve the input query" )
        ( "lp-solver",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::LP_SOLVER]) ),
          "Solver for the LPs during the complete analysis: native/gurobi. default: native" )
        ( "milp-tightening",
          boost::program_options::value<std::string>( &((*_stringOptions)[Options::MILP_SOLVER_BOUND_TIGHTENING_TYPE ]) ),
          "The MILP solver bound tightening type: lp/lp-inc/milp/milp-inc/iter-prop/none. default: lp" )
        ( "milp-timeout",
          boost::program_options::value<float>( &((*_floatOptions)[Options::MILP_SOLVER_TIMEOUT]) ),
          "Per-ReLU timeout for iterative propagation" )
        ( "num-simulations",
          boost::program_options::value<int>( &((*_intOptions)[Options::NUMBER_OF_SIMULATIONS]) ),
          "Number of simulations generated per neuron" )
        ( "lp-tightening-after-split",
          boost::program_options::bool_switch( &((*_boolOptions)[Options::PERFORM_LP_TIGHTENING_AFTER_SPLIT]) ),
          "Whether to skip a LP tightening after a case split" )
        ( "inc-lin",
          boost::program_options::value<int>( &((*_intOptions)[Options::NUMBER_OF_INCREMENTAL_LINEARIZATIONS]) ),
          "Number of incremental linearizations" )
        ( "eager-lin",
          boost::program_options::value<int>( &((*_intOptions)[Options::EAGER_LINEARIZATION]) ),
          "Perform linearization eagerly" )
#endif // ENABLE_GUROBI

        ;

    // Positional options, for the mandatory options
    _positionalOptions.add( "input", 1 );
    _positionalOptions.add( "property", 2 );
}

void OptionParser::parse( int argc, char **argv )
{
    boost::program_options::store
        ( boost::program_options::command_line_parser( argc, argv )
          .options( _optionDescription ).positional( _positionalOptions ).run(),
          _variableMap );
    boost::program_options::notify( _variableMap );
}

bool OptionParser::valueExists( const String &option )
{
    return _variableMap.count( option.ascii() ) != 0;
}

int OptionParser::extractIntValue( const String &option )
{
    ASSERT( valueExists( option ) );
    return _variableMap[option.ascii()].as<int>();
}

void OptionParser::printHelpMessage() const
{
    std::cerr << _optionDescription << std::endl;
};

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
