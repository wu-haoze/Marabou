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

Invariant::Invariant( List<Tightening> &tightenings )
{
    for ( auto &tightening : tightenings )
        storeBoundTightening( tightening );
}

void Invariant::storeBoundTightening( const Tightening &tightening )
{
    _bounds.append( tightening );

    // store the negation of the activation pattern
    PiecewiseLinearCaseSplit split;
    if ( tightening._type == Tightening::LB )
    {
        split.storeBoundTightening( Tightening( tightening._variable,
                                                tightening._value,
                                                Tightening::UB ) );
    } else {
        split.storeBoundTightening( Tightening( tightening._variable,
                                                tightening._value,
                                                 Tightening::LB ) );
    }
    _activationPatterns.append( split );
}

List<Tightening> Invariant::getBoundTightenings() const
{
    return _bounds;
}

List<PiecewiseLinearCaseSplit> Invariant::getActivationPatterns() const
{
    return _activationPatterns;
}

void Invariant::dump() const
{
    printf( "\nDumping invariant\n" );
    printf( "\tActivation Pattern is:\n" );
    for ( const auto &bound : _bounds )
    {
        printf( "\t\tVariable: %u. New bound: %.2lf. Bound type: %s\n",
                bound._variable, bound._value, bound._type == Tightening::LB ? "lower" : "upper" );
    }
}

bool Invariant::operator==( const Invariant &other ) const
{
    return ( _bounds == other._bounds );
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
