/*********************                                                        */
/*! \file FlippingStrategy.h
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

#ifndef __FlippingStrategy_h__
#define __FlippingStrategy_h__

#include "IEngine.h"
#include "PiecewiseLinearConstraint.h"

class FlippingStrategy
{
public:
    struct CostComponent
    {
        CostComponent()
            : _constraint( NULL )
            , _phase( PhaseStatus::PHASE_NOT_FIXED )
        {
        }

        CostComponent( PiecewiseLinearConstraint *constraint, PhaseStatus phase )
            : _constraint( constraint )
            , _phase( phase )
        {
        }

        PiecewiseLinearConstraint *_constraint;
        PhaseStatus _phase;
    }

    virtual CostComponent pickPiecewiseLinearConstraintToFlip()
    {
        return CostComponent();
    }
};

#endif // __FlippingStrategy_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
