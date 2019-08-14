/*********************                                                        */
/*! \file PropertyParser.cpp
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

#include "Debug.h"
#include "File.h"
#include "InputParserError.h"
#include "MStringf.h"
#include "InvariantParser.h"
#include <regex>

void InvariantParser::parse( const String &invariantFilePath, Invariant &invariant )
{
    if ( !File::exists( invariantFilePath ) )
    {
        printf( "Error: the specified property file (%s) doesn't exist!\n", invariantFilePath.ascii() );
        throw InputParserError( InputParserError::FILE_DOESNT_EXIST, invariantFilePath.ascii() );
    }

    File propertyFile( invariantFilePath );
    propertyFile.open( File::MODE_READ );

    try
    {
        while ( true )
        {
            String line = propertyFile.readLine().trim();
            processSingleLine( line, invariant );
        }
    }
    catch ( const CommonError &e )
    {
        // A "READ_FAILED" is how we know we're out of lines
        if ( e.getCode() != CommonError::READ_FAILED )
            throw e;
    }
}

void InvariantParser::processSingleLine( const String &line, Invariant &invariant )
{
    List<String> tokens_list = line.tokenize( " " );
    Vector<String> tokens;
    for ( const auto& token : tokens_list )
        tokens.append( token );

    if ( tokens.size() != 2 )
        throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );

    // These variables are of the form ws_2_5
    auto subTokens = tokens[0].tokenize( "_" );

    auto subToken = subTokens.begin();
    ++subToken;
    unsigned layerIndex = atoi( subToken->ascii() );
    ++subToken;
    unsigned nodeIndex = atoi( subToken->ascii() );

    unsigned direction = atoi( tokens[1].ascii() );

    invariant.addActivationPattern( layerIndex, nodeIndex, direction );
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
