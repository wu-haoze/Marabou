/*********************                                                        */
/*! \file ReluLookAheadDivider.h
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
#include "ReluLookAheadDivider.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"

ReluLookAheadDivider::ReluLookAheadDivider( std::shared_ptr<Engine> engine )
    : _engine( std::move( engine ) )
{
}

void ReluLookAheadDivider::createSubQueries( unsigned numNewSubqueries, const String
                                             queryIdPrefix, const
                                             PiecewiseLinearCaseSplit &previousSplit,
                                             const unsigned timeoutInSeconds,
                                             SubQueries &subQueries )
{
    unsigned numBisects = (unsigned)log2( numNewSubqueries );

    List<PiecewiseLinearCaseSplit *> splits;
    auto split = new PiecewiseLinearCaseSplit();
    *split = previousSplit;
    splits.append( split );

    // Repeatedly bisect the dimension with the largest interval
    for ( unsigned i = 0; i < numBisects; ++i )
    {
        List<PiecewiseLinearCaseSplit *> newSplits;
        for ( const auto &split : splits )
        {
            PiecewiseLinearConstraint *pLConstraintToSplit =
                getPLConstraintToSplit( *split );
            auto caseSplits = pLConstraintToSplit->getCaseSplits();
            for ( const auto &caseSplit : caseSplits )
            {
                auto newSplit = new PiecewiseLinearCaseSplit();
                *newSplit = caseSplit;

                for ( const auto &tightening : split->getBoundTightenings() )
                    newSplit->storeBoundTightening( tightening );

                for ( const auto &equation : split->getEquations() )
                    newSplit->addEquation( equation );

                newSplits.append( newSplit );
            }
            delete split;
        }
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
        subQuery->_split.reset(split);
        subQuery->_timeoutInSeconds = timeoutInSeconds;
        subQueries.append( subQuery );
    }
}

PiecewiseLinearConstraint *ReluLookAheadDivider::getPLConstraintToSplit
( const PiecewiseLinearCaseSplit &split )
{
    unsigned threshold = 5;

    EngineState *engineStateBeforeSplit = new EngineState();
    _engine->storeState( *engineStateBeforeSplit, true );

    _engine->applySplit( split );
    unsigned numActiveUpperbound = _engine->propagateAndGetNumberOfActiveConstraints();
    EngineState *engineState = new EngineState();
    _engine->storeState( *engineState, true );

    auto plConstraints = _engine->getPLConstraints();

    PiecewiseLinearConstraint *constraintToSplit = NULL;
    float minCost = numActiveUpperbound;

    for ( const auto &constraint : plConstraints )
    {
        if ( !( constraint->phaseFixed() ) )
        {
            auto caseSplits = constraint->getCaseSplits();
            unsigned max_ = 0;
            unsigned min_ = numActiveUpperbound;
            for ( const auto& caseSplit : caseSplits )
            {
                _engine->applySplit( caseSplit );
                unsigned numActive = _engine->propagateAndGetNumberOfActiveConstraints();
                if ( max_ < numActive )
                    max_ = numActive;
                if ( min_ > numActive )
                    min_ = numActive;
                _engine->restoreState( *engineState );
            }
            if ( min_ < threshold )
                min_ = max_;
            float newCost = float(min_ + max_) / 2;
            if ( newCost < minCost )
            {
                minCost = newCost;
                constraintToSplit = constraint;
            }
            if ( minCost <= threshold * threshold )
                break;
        }
    }

    _engine->restoreState( *engineStateBeforeSplit );
    delete engineStateBeforeSplit;
    delete engineState;
    return constraintToSplit;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
