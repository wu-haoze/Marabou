/*********************                                                        */
/*! \file Test_IncrementalLinearization.h
** \verbatim
** Top contributors (to current version):
**   Teruhiro Tagomori, Andrew Wu
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** [[ Add lengthier description here ]]
**/

#include "FloatUtils.h"
#include "IncrementalLinearization.h"
#include "InputQuery.h"
#include "MILPEncoder.h"
#include "MarabouError.h"

#include <cxxtest/TestSuite.h>
#include <string.h>

class IncrementalLinearizationTestSuite : public CxxTest::TestSuite
{
public:
    void setUp()
    {
    }

    void tearDown()
    {
    }

    void test_incremental_linearization()
    {
        Engine engine;
        //InputQuery
    }
};
