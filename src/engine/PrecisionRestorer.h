/*********************                                                        */
/*! \file PrecisionRestorer.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#ifndef __PrecisionRestorer_h__
#define __PrecisionRestorer_h__

#include "EngineState.h"

using namespace marabou;

class PrecisionRestorer
{
public:
    enum RestoreBasics {
        RESTORE_BASICS = 0,
        DO_NOT_RESTORE_BASICS = 1,
    };

    void storeInitialEngineState( const TheoryEngine &engine );

    void restorePrecision( TheoryEngine &engine,
                           ITableau &tableau,
                           SmtCore &smtCore,
                           RestoreBasics restoreBasics );

private:
    EngineState _initialEngineState;
};

#endif // __PrecisionRestorer_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
