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

PiecewiseLinearConstraint *LookAheadDivider::getPLConstraintToSplit
( const PiecewiseLinearCaseSplit &split )
{

    EngineState *engineState = new EngineState();
    _engine->storeState( *engineState, true );

    _engine->applySplit( split );
    _engine->propagateSplit();

    unsigned numFixed = _engine->numberOfFixedConstraints();

    EngineState *engineStateAfterSplit = new EngineState();
    _engine->storeState( *engineStateAfterSplit, true );

    PiecewiseLinearConstraint *constraintToSplit = NULL;
    unsigned maxNumFixedConstraints = 0;

    List<PiecewiseLinearConstraint *> plConstraints =
        _engine->getPLConstraints();

    unsigned upperlimit = 50;
    unsigned limit = 0;
    for ( const auto &constraint : plConstraints )
    {
        if ( limit > upperlimit || limit > plConstraints.size() )
        {
            if ( constraintToSplit != NULL )
                break;
            else
                upperlimit += 50;
        }
        if ( !( constraint->phaseFixed() ) )
        {
            unsigned numFixedConstraints = 0;
            auto caseSplits = constraint->getCaseSplits();
            for ( const auto& caseSplit : caseSplits )
            {
                _engine->applySplit( caseSplit );
                _engine->propagateSplit();
                numFixedConstraints += ( _engine->numberOfFixedConstraints()
                                         - numFixed );
                _engine->restoreState( *engineStateAfterSplit );
            }
            if ( numFixedConstraints > maxNumFixedConstraints )
            {
                maxNumFixedConstraints = numFixedConstraints;
                constraintToSplit = constraint;
            }
            ++limit;
        }
    }

    _engine->restoreState( *engineState );
    assert( constraintToSplit != NULL );

    delete engineState;
    delete engineStateAfterSplit;

    return constraintToSplit;
}

void LookAheadDivider::setCandidatePLConstraints( List
                                                  <PiecewiseLinearConstraint *>
                                                  candidatePLConstraints )
{
    _candidatePLConstraints = candidatePLConstraints;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
