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
#include "EngineState.h"
#include "ReluDivider.h"
#include "PiecewiseLinearCaseSplit.h"

#include <fstream>

ReluDivider::ReluDivider( std::shared_ptr<IEngine> engine, String summaryFile )
    : _engine( std::move( engine ) )
    , _summaryFile( summaryFile )
{
}

void ReluDivider::createSubQueries( unsigned numNewSubqueries, const String
                                    queryIdPrefix, const
                                    PiecewiseLinearCaseSplit &previousSplit,
                                    const unsigned timeoutInSeconds,
                                    SubQueries &subQueries )
{
    std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
    ofs << Stringf("\nCreating subqueries for Id:%s \n", queryIdPrefix.ascii()).ascii();
    ofs.close();

    unsigned numBisects = (unsigned)log2( numNewSubqueries );

    List<PiecewiseLinearCaseSplit *> splits;
    auto split = new PiecewiseLinearCaseSplit();
    *split = previousSplit;
    splits.append( split );
    _engine->propagate();

    for ( unsigned i = 0; i < numBisects; ++i )
    {
        std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
        ofs << "\t\n" << i << "th level of splitting: \n";
        ofs.close();

        List<PiecewiseLinearCaseSplit *> newSplits;
        for ( const auto &split : splits )
        {
            std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
            ofs << "\tCreating splits\n";
            ofs.close();
            PiecewiseLinearConstraint *pLConstraintToSplit =
                getPLConstraintToSplit( *split );
            if ( pLConstraintToSplit == NULL )
            {
                auto newSplit = new PiecewiseLinearCaseSplit();
                *newSplit = *split;
                newSplits.append( newSplit );
            }
            else
            {
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
            }
            delete split;
        }
        splits = newSplits;
    }
    std::ofstream ofs1 (_summaryFile.ascii(), std::ofstream::app);
    ofs1 << "Splits seleted!\n";

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
    ofs1 << "Subqueries added!\n\n\n";
    ofs1.close();
}

PiecewiseLinearConstraint *ReluDivider::getPLConstraintToSplit
( const PiecewiseLinearCaseSplit &split )
{
    std::ofstream ofs (_summaryFile.ascii(), std::ofstream::app);
    ofs << "\tStoring State!\n";
    EngineState *engineStateBeforeSplit = new EngineState();
    _engine->storeState( *engineStateBeforeSplit, true );
    ofs << "\tState stored, apply Split!\n";
    _engine->applySplit( split );
    ofs << "\tSplit applied! Propagating\n";
    PiecewiseLinearConstraint *constraintToSplit = NULL;
    if ( _engine->propagate() )
    {
        ofs << "\tPropagated!\n";
        constraintToSplit = computeBestChoice();
    }
    if ( !constraintToSplit )
        ofs << "\tNo constraint selected!\n";
    else
        ofs << "\tConstraint selected: " <<  constraintToSplit->getId() << "\n";

    _engine->restoreState( *engineStateBeforeSplit );
    delete engineStateBeforeSplit;
    ofs << "\tState restored!\n";
    ofs.close();
    return constraintToSplit;
}

PiecewiseLinearConstraint *ReluDivider::computeBestChoice()
{
    Map<unsigned, double> balanceEstimates;
    Map<unsigned, double> runtimeEstimates;
    _engine->getEstimates( balanceEstimates, runtimeEstimates );
    PiecewiseLinearConstraint *best = NULL;
    double bestRank = balanceEstimates.size() * 3;
    for ( const auto &entry : runtimeEstimates ){
        if ( entry.second < GlobalConfiguration::RUNTIME_ESTIMATE_THRESHOLD )
        {
            double newRank = balanceEstimates[entry.first];
            if ( newRank < bestRank )
            {
                best = _engine->getConstraintFromId( entry.first );
                bestRank = newRank;
            }
        }
    }
    return best;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
