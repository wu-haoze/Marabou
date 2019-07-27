/*********************                                                        */
/*! \file ActivationPatternDivider.h
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

#ifndef __ActivationPatternDivider_h__
#define __ActivationPatternDivider_h__

#include "List.h"
#include "QueryDivider.h"
#include "NetworkLevelReasoner.h"

#include <math.h>

class ActivationPatternDivider : public QueryDivider
{
public:
    ActivationPatternDivider( const List<unsigned> &inputVariables,
                              NetworkLevelReasoner *networkLevelReasoner,
                              unsigned numberOfSegments,
                              unsigned pointsPerSegment );

    ~ActivationPatternDivider();

    void createSubQueries( unsigned numNewSubQueries,
                           const String queryIdPrefix,
                           const PiecewiseLinearCaseSplit
                           &previousSplit,
                           const unsigned timeoutInSeconds,
                           SubQueries &subQueries );

private:

    /*
      Allocate memory for the sample points and activation patterns.
    */
    void allocateMemory();

    /*
      Free _samplePoints and _patterns.
    */
    void freeMemoryIfNeeded();

    /*
      Returns the variable with the largest Relu pattern variance
    */
    unsigned getLargestVariance( const InputRegion& inputRegion );

    /*
      Sample points for the given input variable.
    */
    bool samplePoints( const InputRegion& inputRegion, unsigned inputVariable );

    /*
      Get the activation pattern of the sampled points.
    */
    void getActivationPatterns();

    /*
      Get the variance of the activation patterns.
    */
    unsigned getPatternVariance();

    /*
      Get the 1 norm of the Manhattan distance between two activation patterns
    */
    unsigned manhattanDistance( ActivationPattern &pattern1,
                                ActivationPattern &pattern2 );

    /*
      Print the sampled points
    */
    void dumpSampledPoints();

    /*
      Print the activation pattern
    */
    void dumpActivationPatterns();

    // All input variables of the network
    const List<unsigned> _inputVariables;

    // NetworkLevelReasoner for get the Relu pattern
    NetworkLevelReasoner *_networkLevelReasoner;

    unsigned _numberOfSegments;
    unsigned _pointsPerSegment;
    unsigned _numberOfPoints;

    double** _samplePoints;
    ActivationPattern** _patterns;

};

#endif // __ActivationVarianceDivider_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
