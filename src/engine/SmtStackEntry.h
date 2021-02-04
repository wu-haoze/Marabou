/*********************                                                        */
/*! \file SmtStackEntry.h
** \verbatim
** Top contributors (to current version):
**   Guy Katz, Haoze Wu
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** [[ Add lengthier description here ]]

**/

#ifndef __SmtStackEntry_h__
#define __SmtStackEntry_h__

#include "PiecewiseLinearCaseSplit.h"

/*
  A stack entry consists of the engine state before the split,
  the active split, the alternative splits (in case of backtrack),
  and also any implied splits that were discovered subsequently.
*/
struct SmtStackEntry
{
public:
    PiecewiseLinearCaseSplit _activeSplit;
    List<PiecewiseLinearCaseSplit> _alternativeSplits;
};

#endif // __SmtStackEntry_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
