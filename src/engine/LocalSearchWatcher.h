/*********************                                                        */
/*! \file LocalSearchWatcher.h
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

#ifndef __LocalSearchWatcher_h__
#define __LocalSearchWatcher_h__

#include "IEngine.h"
#include "PiecewiseLinearConstraint.h"

class LocalSearchWatcher
{
public:
    registerToWatchConstraint( PiecewiseLinearConstraint *constraint );
};

#endif // __LocalSearchWatcher_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
