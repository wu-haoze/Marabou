/*********************                                                        */
/*! \file ActivationPatternDivider.cpp
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
#include "ActivationPatternDivider.h"
#include "MarabouError.h"
#include "MStringf.h"
#include "PiecewiseLinearCaseSplit.h"

#include <random>

ActivationPatternDivider::ActivationPatternDivider( const List<unsigned>
                                                     &inputVariables,
                                                     NetworkLevelReasoner
                                                     *networkLevelReasoner,
                                                     unsigned numberOfSegments,
                                                     unsigned pointsPerSegment )
    : _inputVariables( inputVariables )
    , _networkLevelReasoner( networkLevelReasoner )
    , _numberOfSegments( numberOfSegments )
    , _pointsPerSegment( pointsPerSegment )
    , _numberOfPoints( pointsPerSegment * ( numberOfSegments + 1 ) )
{
    allocateMemory();
}

ActivationPatternDivider::~ActivationPatternDivider()
{
    freeMemoryIfNeeded();
}

void ActivationPatternDivider::freeMemoryIfNeeded()
{
    if ( _samplePoints )
        for ( unsigned i = 0; i < _numberOfPoints; ++i )
            delete _samplePoints[i];
    delete _samplePoints;
    _samplePoints = NULL;
    if ( _patterns )
        for ( unsigned i = 0; i < _numberOfPoints; ++i )
            delete _patterns[i];
    delete _patterns;
    _patterns = NULL;
}

void ActivationPatternDivider::allocateMemory()
{
    unsigned numberOfInputVariables = _inputVariables.size();
    _samplePoints = new double*[_numberOfPoints];
    if ( !_samplePoints )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "ActivationPatternDivider::_samplePoints" );
    for ( unsigned i = 0; i < _numberOfPoints; ++i )
        {
            _samplePoints[i] = new double[numberOfInputVariables];
            if ( !_samplePoints[i] )
                throw MarabouError( MarabouError::ALLOCATION_FAILED, "ActivationPatternDivider::_samplePoints[i]" );
            std::fill_n( _samplePoints[i], numberOfInputVariables, 0 );
        }

    _patterns = new ActivationPattern *[_numberOfPoints];
    if ( !_patterns )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "ActivationPatternDivider::_patterns" );

    for ( unsigned i = 0; i < _numberOfPoints; ++i )
    {
        _patterns[i] = new ActivationPattern;
        if ( !_patterns[i] )
            throw MarabouError( MarabouError::ALLOCATION_FAILED, "ActivationPatternDivider::_patterns[i]" );
    }
}

void ActivationPatternDivider::createSubQueries( unsigned numNewSubqueries,
                                                 const String queryIdPrefix,
                                                 const PiecewiseLinearCaseSplit
                                                 &previousSplit,
                                                 const unsigned timeoutInSeconds,
                                                 SubQueries &subQueries )
{
    unsigned numBisects = (unsigned)log2( numNewSubqueries );

    List<InputRegion> inputRegions;

    // Create the first input region from the previous case split
    InputRegion region;
    List<Tightening> bounds = previousSplit.getBoundTightenings();
    for ( const auto &bound : bounds )
    {
        if ( bound._type == Tightening::LB )
        {
            region._lowerBounds[bound._variable] = bound._value;
        }
        else
        {
            ASSERT( bound._type == Tightening::UB );
            region._upperBounds[bound._variable] = bound._value;
        }
    }
    inputRegions.append( region );

    // Repeatedly bisect the dimension with the largest interval
    for ( unsigned i = 0; i < numBisects; ++i )
    {
        List<InputRegion> newInputRegions;
        for ( const auto &inputRegion : inputRegions )
        {
            // Get the dimension with the largest variance in activation patterns
            unsigned dimensionToSplit = getLargestVariance( inputRegion );
            bisectInputRegion( inputRegion, dimensionToSplit, newInputRegions );
        }
        inputRegions = newInputRegions;
    }

    unsigned queryIdSuffix = 1; // For query id
    // Create a new subquery for each newly created input region
    for ( const auto &inputRegion : inputRegions )
    {
        // Create a new query id
        String queryId;
        if ( queryIdPrefix == "" )
            queryId = queryIdPrefix + Stringf( "%u", queryIdSuffix++ );
        else
            queryId = queryIdPrefix + Stringf( "-%u", queryIdSuffix++ );

        // Create a new case split
        auto split = std::unique_ptr<PiecewiseLinearCaseSplit>
            ( new PiecewiseLinearCaseSplit() );
        // Add bound as equations for each input variable
        for ( const auto &variable : _inputVariables )
        {
            double lb = inputRegion._lowerBounds[variable];
            double ub = inputRegion._upperBounds[variable];
            split->storeBoundTightening( Tightening( variable, lb,
                                                     Tightening::LB ) );
            split->storeBoundTightening( Tightening( variable, ub,
                                                     Tightening::UB ) );
        }

        // Construct the new subquery and add it to subqueries
        SubQuery *subQuery = new SubQuery;
        subQuery->_queryId = queryId;
        subQuery->_split = std::move(split);
        subQuery->_timeoutInSeconds = timeoutInSeconds;
        subQueries.append( subQuery );
    }
}

unsigned ActivationPatternDivider::getLargestVariance( const InputRegion
                                                        &inputRegion )
{
    ASSERT( inputRegion._lowerBounds.size() == inputRegion._upperBounds.size() );
    unsigned dimensionToSplit = 0;
    double largestVariance = 0;

    for ( const auto &variable : _inputVariables )
    {
        // Sample points
        if ( samplePoints( inputRegion, variable ) )
        {
            // Compute the activation pattern for each sampled point
            getActivationPatterns();
            unsigned variance = getPatternVariance();
            if ( variance > largestVariance )
            {
                dimensionToSplit = variable;
                largestVariance = variance;
            }
        }
    }
    return dimensionToSplit;
}

bool ActivationPatternDivider::samplePoints( const InputRegion &inputRegion,
                                              unsigned inputVariable )
{
    double lowerBound = inputRegion._lowerBounds[inputVariable];
    double upperBound = inputRegion._upperBounds[inputVariable];
    double width = ( upperBound - lowerBound ) / _numberOfSegments;
    if ( width == 0 )
        return false;

    Vector<double> segments;
    double to_append = lowerBound;
    for (unsigned i = 0; i < _numberOfSegments; ++i )
    {
        segments.append( to_append );
        to_append += width;
    }
    segments.append( upperBound );

    unsigned index = 0;
    for ( const auto &variable : _inputVariables )
     {
         if ( variable != inputVariable )
         {
             unsigned seed = variable;
             lowerBound = inputRegion._lowerBounds[variable];
             upperBound = inputRegion._upperBounds[variable];
             std::uniform_real_distribution<double> uniform( lowerBound,
                                                             upperBound );
             std::default_random_engine randomEngine( seed );
             for ( unsigned i = 0; i < _pointsPerSegment; ++i )
             {
                 double value = uniform( randomEngine );
                 for ( unsigned j = 0; j < _numberOfSegments + 1; ++j )
                     _samplePoints[i * (_numberOfSegments + 1) + j][index] =
                         value;
             }
         } else {
             for ( unsigned i = 0; i < _pointsPerSegment; ++i )
                 for ( unsigned j = 0; j < _numberOfSegments + 1; ++j )
                     _samplePoints[i * (_numberOfSegments + 1) + j][index] =
                         segments[j];
         }
         ++index;
     }
    return true;
}

void ActivationPatternDivider::dumpSampledPoints()
{
    for ( unsigned i = 0; i < _numberOfPoints; ++i )
    {
        for ( unsigned j = 0; j < _inputVariables.size(); ++j )
            std::cout << _samplePoints[i][j] << " ";
        std::cout << std::endl;
    }
}

void ActivationPatternDivider::dumpActivationPatterns()
{
    for ( unsigned i = 0; i < _numberOfPoints; ++i )
        {
            for ( auto &act : *( _patterns[i] ) )
                std::cout << act << " ";
            std::cout << std::endl;
        }
}

void ActivationPatternDivider::getActivationPatterns()
{
    for ( unsigned i = 0; i < _numberOfPoints; ++i )
        _networkLevelReasoner->getActivationPattern( _samplePoints[i],
                                                     _patterns[i] );
}

unsigned ActivationPatternDivider::getPatternVariance()
{
    unsigned variance = 0;
    for ( unsigned i = 0; i < _pointsPerSegment; ++i )
    {
        unsigned start_index = i * ( _numberOfSegments + 1 );
        for ( unsigned j = 0; j < _numberOfSegments; ++j )
            variance += manhattanDistance( *_patterns[start_index + j],
                                           *_patterns[start_index + j + 1] );
    }
    return variance;
}

unsigned ActivationPatternDivider::manhattanDistance( ActivationPattern
                                                       &pattern1,
                                                       ActivationPattern
                                                       &pattern2 )
{
    unsigned dist = 0;
    ASSERT ( pattern1.size() == pattern2.size() );
    for ( unsigned i = 0; i < pattern1.size(); ++i )
        if ( pattern1[i] != pattern2[i] )
            ++dist;
    return dist;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
