/*********************                                                        */
/*! \file SODDivider.h
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

LookAheadDivider::LookAheadDivider( std::shared_ptr<Engine> engine )
    : _engine( std::move( engine ) )
{
}

void LookAheadDivider::createSubQueries( unsigned numNewSubqueries, const String
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
    for ( unsigned i = 0; i < numBisects - 1; ++i )
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

PiecewiseLinearConstraint *LookAheadDivider::getPLConstraintToSplit
( const PiecewiseLinearCaseSplit &split )
{
    unsigned numFixed = engine.getNumberOfFixedRelus();
    engine.applySplit(split);
    do
        performSymbolicBoundTightening();
    while ( applyAllValidConstraintCaseSplits() );
    unsigned newlyFixed = engine.getNumberOfFixedRelus() - numFixed;
    return NULL;
}


//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
