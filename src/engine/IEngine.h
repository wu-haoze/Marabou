/*********************                                                        */
/*! \file IEngine.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#ifndef __IEngine_h__
#define __IEngine_h__

#ifdef _WIN32
#undef ERROR
#endif

#include "ExitCode.h"

namespace marabou {

class IEngine
{
public:
    virtual ~IEngine() {};

    virtual bool solve( unsigned timeoutInSeconds ) = 0;

    virtual ExitCode getExitCode() const = 0;
};

}

#endif // __IEngine_h__
