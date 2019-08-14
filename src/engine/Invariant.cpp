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

void Invariant::addActivationPattern( unsigned layerIndex, unsigned nodeIndex,
                                      int direction )
{

    _patterns[SymbolicBoundTightener::NodeIndex( layerIndex, nodeIndex )] = direction;
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
