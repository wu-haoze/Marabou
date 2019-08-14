/*********************                                                        */
/*! \file InvariantParser.h
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

#ifndef __InvariantParser_h__
#define __InvariantParser_h__

#include "Equation.h"
#include "InputQuery.h"
#include "Invariant.h"
#include "MString.h"
#include "Vector.h"

/*
  This class reads a property from a text file, and stores the property's
  constraints within an InputQuery object.
  Currently, properties can involve the input variables (X's) and the output
  variables (Y's) of a neural network.
*/
class InvariantParser
{
public:
    void parse( const String &invariantFilePath, Invariant &invariant );

private:
    void processSingleLine( const String &line, Invariant &invariant );
};

#endif // __InvariantParser_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
