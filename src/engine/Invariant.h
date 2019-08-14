/*********************                                                        */
/*! \file Invariant.h
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

#ifndef __Invariant_h__
#define __Invariant_h__

#include "Tightening.h"
#include "PiecewiseLinearCaseSplit.h"
#include "SymbolicBoundTightener.h"
#include "Map.h"

class Invariant
{
public:
    Invariant();

    List<PiecewiseLinearCaseSplit> getActivationPatterns( SymbolicBoundTightener *sbt );

    void addActivationPattern( unsigned layerIndex, unsigned nodeIndex, int direction );

    /*
      Dump the invariant - for debugging purposes.
    */
    void dump() const;

private:
    Map<SymbolicBoundTightener::NodeIndex, int> _patterns;
};

#endif // __Invariant_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
