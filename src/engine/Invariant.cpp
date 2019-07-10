/*********************                                                        */
/*! \file Invariant.cpp
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

#include "Invariant.h"
#include <cstdio>

Invariant::Invariant()
{
}

void Invariant::addActivationPattern( SymbolicBoundTightener::NodeIndex index,
                                      bool active )
{
    _patterns[index] = active;
}

List<PiecewiseLinearCaseSplit> Invariant::getActivationPatterns( SymbolicBoundTightener *sbt )
{
    sbt->run();
    List<PiecewiseLinearCaseSplit> splits;
    auto nodeIndexToFMapping = sbt->getNodeIndexToFMapping();
    auto nodeIndexToBMapping = sbt->getNodeIndexToBMapping();
    for ( const auto &pattern : _patterns )
    {
        SymbolicBoundTightener::NodeIndex index = pattern.first;
        unsigned b = nodeIndexToBMapping[index];
        unsigned f = nodeIndexToFMapping[index];
        bool active = pattern.second;
        PiecewiseLinearCaseSplit split;
        if ( active )
        {
            // If active, check inactive
            split.storeBoundTightening( Tightening( b, 0.0, Tightening::UB ) );
            split.storeBoundTightening( Tightening( f, 0.0, Tightening::UB ) );
        } else
        {
            split.storeBoundTightening( Tightening( b, 0.0, Tightening::LB ) );
            Equation activeEquation( Equation::EQ );
            activeEquation.addAddend( 1, b );
            activeEquation.addAddend( -1, f );
            activeEquation.setScalar( 0 );
            split.addEquation( activeEquation );
        }
        splits.append( split );
    }
    return splits;
}

void Invariant::dump() const
{
    printf( "\nDumping invariant\n" );
    printf( "\tActivation Pattern is:\n" );
    for ( const auto &pattern : _patterns )
    {
        printf( "\t\tNode: %u %u: %s\n",
                pattern.first._layer, pattern.first._neuron, pattern.second ? "Active" : "Inactive" );
    }
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
