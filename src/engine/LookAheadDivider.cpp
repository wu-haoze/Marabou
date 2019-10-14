/*********************                                                        */
/*! \file LookAheadDivider.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include "Debug.h"
#include "LookAheadDivider.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"

#include <cmath>

LookAheadDivider::LookAheadDivider( const List<unsigned>
                                    &inputVariables,
                                    std::shared_ptr<Engine> engine )
    : _inputVariables( inputVariables )
    , _engine( std::move( engine ) )
{
}

void LookAheadDivider::createSubQueries( unsigned numNewSubqueries, const String
                                         queryIdPrefix, const
                                         PiecewiseLinearCaseSplit &previousSplit,
                                         const unsigned timeoutInSeconds,
                                         SubQueries &subQueries )
{
    unsigned numBisects = (unsigned)log2( numNewSubqueries );

    // Store the current engine state
    auto engineState = std::unique_ptr<EngineState>( new EngineState() );
    _engine->storeState( *engineState, true );

    List<PiecewiseLinearCaseSplit> splits;
    splits.append( previousSplit );

    // Repeatedly bisect the dimension with the largest interval
    for ( unsigned i = 0; i < numBisects; ++i )
    {
        List<PiecewiseLinearCaseSplit> newSplits;
        for ( const auto &split : splits )
            // Get the dimension with the largest variance in activation patterns
            getDimensionToBisect( split, newSplits, *engineState );
        splits = newSplits;
    }

    unsigned queryIdSuffix = 1; // For query id
    // Create a new subquery for each newly created input region
    for ( const auto &split : splits )
    {
        // Create a new query id
        String queryId;
        if ( queryIdPrefix == "" )
            queryId = queryIdPrefix + Stringf( "%u", queryIdSuffix++ );
        else
            queryId = queryIdPrefix + Stringf( "-%u", queryIdSuffix++ );

        // Construct the new subquery and add it to subqueries
        SubQuery *subQuery = new SubQuery;
        subQuery->_queryId = queryId;
        subQuery->_split = std::unique_ptr<PiecewiseLinearCaseSplit>
            ( new PiecewiseLinearCaseSplit() );
        * (subQuery->_split) = split;
        subQuery->_timeoutInSeconds = timeoutInSeconds;
        subQueries.append( subQuery );
    }
}

static unsigned computeImpact( const List<unsigned> &numsFixedConstraints )
{
    //unsigned max = 0;
    //for ( const auto &num : numsFixedConstraints )
    //    if ( num > max )
    //        max = num;

    //unsigned min = max;
    //for ( const auto &num : numsFixedConstraints )
    //    if ( num < min )
    //        min = num;
    //return min;
    unsigned multiple = 1;
    for ( const auto &num : numsFixedConstraints )
        multiple *= num;
    return multiple;
}

static unsigned computeTieBreaker( const List<unsigned> &numsFixedConstraints )
{
    unsigned sum = 0;
    for ( const auto &num : numsFixedConstraints )
        sum += num;
    return sum;
}

void LookAheadDivider::getDimensionToBisect( const PiecewiseLinearCaseSplit
                                             &inputRegion,
                                             List<PiecewiseLinearCaseSplit>
                                             &newInputRegions,
                                             const EngineState &engineState )
{
    List<PiecewiseLinearCaseSplit> candidateInputRegions;
    //unsigned var = 0;
    int maxImpact = -1;
    int maxTieBreaker = -1;

    for ( const auto &variable : _inputVariables )
    {
        List<PiecewiseLinearCaseSplit> newInputRegions;
        bisectBound( inputRegion, variable, newInputRegions );

        List<unsigned> numsFixedConstraints;
        List<PiecewiseLinearCaseSplit> unsolvedInputRegions;
        for ( const auto &newInputRegion : newInputRegions )
        {
            _engine->applySplit( newInputRegion );
            if ( !( _engine->propagate() ) )
                numsFixedConstraints.append( _engine->numberOfPLConstraints() );
            else
            {
                numsFixedConstraints.append( _engine->numberOfFixedConstraints() );
                unsolvedInputRegions.append( newInputRegion );
            }
            _engine->restoreState( engineState );
        }
        //std::cout << "var: " << variable << std::endl;
        //for ( auto &num : numsFixedConstraints )
        //    std::cout << num << " ";
        //std::cout << std::endl;
        int impact = computeImpact( numsFixedConstraints );
        int tieBreaker = computeTieBreaker( numsFixedConstraints );
        if ( impact > maxImpact ||
             ( impact == maxImpact &&
               tieBreaker > maxTieBreaker ) )
        {
            maxImpact = impact;
            maxTieBreaker = tieBreaker;
            candidateInputRegions = unsolvedInputRegions;
            //var = variable;
            if ( unsolvedInputRegions.size() == 0 )
                break;
        }
    }
    //std::cout << "Var to split: " << var << std::endl;
    for ( const auto &candidateInputRegion : candidateInputRegions )
        newInputRegions.append( candidateInputRegion );
}

void LookAheadDivider::bisectBound( const PiecewiseLinearCaseSplit &split,
                                    unsigned variable,
                                    List<PiecewiseLinearCaseSplit>
                                    &newInputRegions )
{
    PiecewiseLinearCaseSplit split1;
    PiecewiseLinearCaseSplit split2;

    unsigned mid = 0;
    for ( const auto &bound : split.getBoundTightenings() )
    {
        if ( bound._variable != variable )
        {
            split1.storeBoundTightening( bound );
            split2.storeBoundTightening( bound );
        }
        else
        {
            mid += bound._value;
            if ( bound._type == Tightening::LB )
            {
                split1.storeBoundTightening( bound );
            }
            else
            {
                ASSERT( bound._type == Tightening::UB );
                split2.storeBoundTightening( bound );
            }
        }
    }
    mid = mid / 2;
    split1.storeBoundTightening( Tightening( variable, mid, Tightening::UB ) );
    split2.storeBoundTightening( Tightening( variable, mid, Tightening::LB ) );

    newInputRegions.append( split1 );
    newInputRegions.append( split2 );
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
