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

class Invariant
{
public:
    Invariant( List<Tightening> &tightenings );

    List<Tightening> getBoundTightenings() const;

    List<PiecewiseLinearCaseSplit> getActivationPatterns() const;

    /*
      Dump the invariant - for debugging purposes.
    */
    void dump() const;

    /*
      Equality operator.
    */
    bool operator==( const Invariant &other ) const;

private:
    /*
      Store information regarding a bound tightening.
    */
    void storeBoundTightening( const Tightening &tightening );


    /*
      Bound tightening information.
    */
    List<Tightening> _bounds;

    /*
      To check whether the invariant holds given the current
      constraints A. We need to check A /\ caseSplit, for each
      caseSplit in _activationPatterns.
    */
    List<PiecewiseLinearCaseSplit> _activationPatterns;

};

#endif // __Invariant_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
