/*********************                                                        */
/*! \file MpsParser.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Rachel Lim, Guy Katz, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include "DisjunctionConstraint.h"
#include "File.h"
#include "FloatUtils.h"
#include "InputParserError.h"
#include "InputQuery.h"
#include "MStringf.h"
#include "MpsParser.h"
#include "PiecewiseLinearCaseSplit.h"
#include "Tightening.h"
#include <cmath>
#include <cstdio>

MpsParser::MpsParser( const String &path )
    : _numRows( 0 )
    , _numVars( 0 )
    , _indexOfObjective ( -1 )
{
    parse( path );
}

void MpsParser::parse( const String &path )
{
    // Load file and check if it exists
    if ( !File::exists( path ) )
        throw InputParserError( InputParserError::FILE_DOESNT_EXIST, path.ascii() );

    File file( path );
    file.open( IFile::MODE_READ );

    // Skip two header lines (NAME and ROWS)
    file.readLine();
    file.readLine();

    // Begin parsing the "ROWS" section
    String line;

    while ( true )
    {
        line = file.readLine();

        if ( line.contains( "COLUMNS" ) )
             break;

	    parseRow( line );
    }

    MPS_LOG( Stringf( "Number of rows parsed: %u", _numRows ).ascii() );

    // Finished parsing rows, proceed to columns
    bool markingInteger = false;
    while ( true )
    {
        line = file.readLine();

        if ( line.contains( "RHS" ) )
            break;

        parseColumn( line, markingInteger );
    }
    ASSERT( !markingInteger );

    MPS_LOG( Stringf( "Number of variables detected: %u\n", _numVars ).ascii() );

    // Finished parsing columns, proceed to rhs
    while ( true )
    {
        line = file.readLine();

        if ( line.contains( "BOUNDS" ) || line.contains( "ENDATA" ) )
            break;

        parseRhs( line );
    }

    // The bounds section is optional, process it if it exists
    if ( line.contains( "BOUNDS" ) )
    {
        while ( true )
        {
            line = file.readLine();

            if ( line.contains( "ENDATA" ) )
                break;

            parseBounds( line );
        }
    }

    setRemainingBounds();
}

void MpsParser::parseRow( const String &line )
{
    List<String> tokens = line.tokenize( "\t\n " );

    if ( tokens.size() != 2 )
	    throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );

    auto it = tokens.begin();
    String type = *it;
    ++it;
    String name = *it;

    // Handle the row type
    switch ( type.ascii()[0] )
    {
    case 'E':
        _equationIndexToRowType[_numRows] = RowType::EQ;
        break;

    case 'L':
        _equationIndexToRowType[_numRows] = RowType::LE;
        break;

    case 'G':
        _equationIndexToRowType[_numRows] = RowType::GE;
        break;

    case 'N':
        if ( _indexOfObjective != -1 )
            throw InputParserError( InputParserError::MULTIPLE_OBJECTIVES );
        _equationIndexToRowType[_numRows] = RowType::OBJ;
        _indexOfObjective = _numRows;
        break;
    default:
        return;
    }

    // Store equation by name and index
    _equationNameToIndex[name] = _numRows;
    _equationIndexToName[_numRows] = name;
    ++_numRows;
}

void MpsParser::parseColumn( const String &line, bool &markingInteger )
{
    List<String> tokens = line.tokenize( "\t\n " );

    // Need an odd number of tokens: row name + pairs
    if ( tokens.size() % 2 == 0 )
	    throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );

    // Check if this line is marking the beginning or the end of integral constraints.
    if ( *( ++tokens.begin() ) == "'MARKER'" )
    {
        if ( tokens.size() != 3 )
            throw InputParserError( InputParserError::UNEXPECTED_INPUT,
                                    line.ascii() );
        if ( *tokens.rbegin() == "'INTORG'" )
        {
            ASSERT( !markingInteger );
            markingInteger = true;
        }
        else if ( *tokens.rbegin() == "'INTEND'" )
        {
            ASSERT( markingInteger );
            markingInteger = false;
        }
        return;
    }
    else
    {
        // Variable name and index
        auto it = tokens.begin();
        String name = *it;
        ++it;
        if ( !_variableNameToIndex.exists( name ) )
        {
            _variableNameToIndex[name] = _numVars;
            _variableIndexToName[_numVars] = name;
            ++_numVars;
        }

        unsigned varIndex = _variableNameToIndex[name];

        // Marking integer variables if needed.
        if ( markingInteger )
            _integerVariables.insert( varIndex );

        // Parse the remaining token pairs
        while ( it != tokens.end() )
        {
            String equationName = *it;
            ++it;
            double coefficient = atof( it->ascii() );
            ++it;

            if ( _equationNameToIndex.exists( equationName ) )
            {
                // The pair describes a coefficient in a known equation
                unsigned equationIndex = _equationNameToIndex[equationName];
                _equationIndexToCoefficients[equationIndex][varIndex] = coefficient;
            }
            else
            {
                // The pair describes a coefficient in an unknown equation (the objective function?)
                if ( coefficient != 0 )
                    throw InputParserError( InputParserError::UNEXPECTED_INPUT,
                                            Stringf( "Problematic pair: %s, %.2lf", equationName.ascii(), coefficient ).ascii() );
            }
        }
    }
}

void MpsParser::parseRhs( const String &line )
{
    List<String> tokens = line.tokenize( "\t\n " );

    // Need an odd number of tokens: RHS + pairs
    if ( tokens.size() % 2 == 0 )
	    throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );

    auto it = tokens.begin();
    String name = *it;
    ++it;

    // Parse the remaining token pairs
    while ( it != tokens.end() )
    {
        String equationName = *it;
        ++it;
        double scalar = atof( it->ascii() );
        ++it;

        if ( !_equationNameToIndex.exists( equationName ) )
            throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );


        unsigned equationIndex = _equationNameToIndex[equationName];
        _equationIndexToRhs[equationIndex] = scalar;
    }
}

void MpsParser::parseBounds( const String &line )
{
    List<String> tokens = line.tokenize( "\t\n " );

    if ( tokens.size() == 3 )
    {
        auto it = tokens.begin();
        String type = *it;
        ++it;

        String dontCareName = *it;
        ++it;
        String varName = *it;

        if ( !_variableNameToIndex.exists( varName ) )
            throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );

        unsigned varIndex = _variableNameToIndex[varName];

        if ( type == "FR" )
        {
            _varToLowerBounds[varIndex] = -DBL_MAX;
            _varToUpperBounds[varIndex] = DBL_MAX;
            // unbounded variable
            return;
        }
        else if ( type == "BV" )
        {
            if ( !_varToLowerBounds.exists( varIndex ) ||
                 _varToLowerBounds[varIndex] < 0 )
                _varToLowerBounds[varIndex] = 0;
            if ( !_varToUpperBounds.exists( varIndex ) ||
                 _varToUpperBounds[varIndex] > 1 )
                _varToUpperBounds[varIndex] = 1;
        }
        else
        {
            throw InputParserError( InputParserError::UNSUPPORTED_BOUND_TYPE,
                                    line.ascii() );
        }
    }
    else if ( tokens.size() == 4 )
    {
        auto it = tokens.begin();
        String type = *it;
        ++it;

        String dontCareName = *it;
        ++it;
        String varName = *it;
        ++it;
        double scalar = atof( it->ascii() );

        if ( !_variableNameToIndex.exists( varName ) )
            throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );

        unsigned varIndex = _variableNameToIndex[varName];

        if ( type == "UP" )
        {
            // Upper bound
            if ( !_varToUpperBounds.exists( varIndex ) || ( _varToUpperBounds[varIndex] > scalar ) )
                _varToUpperBounds[varIndex] = scalar;
        }
        else if ( type == "LO" )
        {
            // Lower bound
            if ( !_varToLowerBounds.exists( varIndex ) || ( _varToLowerBounds[varIndex] < scalar ) )
                _varToLowerBounds[varIndex] = scalar;
        }
        else if ( type == "FX" )
        {
            // Upper and lower bound
            if ( !_varToUpperBounds.exists( varIndex ) || ( _varToUpperBounds[varIndex] > scalar ) )
                _varToUpperBounds[varIndex] = scalar;

            if ( !_varToLowerBounds.exists( varIndex ) || ( _varToLowerBounds[varIndex] < scalar ) )
                _varToLowerBounds[varIndex] = scalar;
        }
        else
        {
            throw InputParserError( InputParserError::UNSUPPORTED_BOUND_TYPE, line.ascii() );
        }
    }
    else
    {
        throw InputParserError( InputParserError::UNEXPECTED_INPUT, line.ascii() );
    }
}

void MpsParser::setRemainingBounds()
{
    // Variables with no bounds specified have LB of 0 and UB of inf.
    for ( unsigned i = 0; i < _numVars; ++i )
    {
        if ( !_varToLowerBounds.exists( i ) &&
             ( !_varToUpperBounds.exists( i ) || _varToUpperBounds[i] >= 0 ) )
	    _varToLowerBounds[i] = 0;
    }
}

unsigned MpsParser::getNumVars() const
{
    return _numVars;
}

unsigned MpsParser::getNumEquations() const
{
    return _numRows;
}

String MpsParser::getVarName( unsigned index ) const
{
    return _variableIndexToName[index];
}

String MpsParser::getEquationName( unsigned index ) const
{
    return _equationIndexToName[index];
}

double MpsParser::getUpperBound( unsigned index ) const
{
    return _varToUpperBounds.exists( index ) ? _varToUpperBounds[index] : DBL_MAX;
}

double MpsParser::getLowerBound( unsigned index ) const
{
    return _varToLowerBounds.exists( index ) ? _varToLowerBounds[index] : -DBL_MAX;
}

void MpsParser::generateQuery( InputQuery &inputQuery ) const
{
    inputQuery.setNumberOfVariables( _numVars );

    populateBounds( inputQuery );
    populateEquations( inputQuery );

    addPiecewiseLinearConstraints( inputQuery );
}

void MpsParser::populateBounds( InputQuery &inputQuery ) const
{
    for ( const auto &it : _varToUpperBounds )
        inputQuery.setUpperBound( it.first, it.second );

    for ( const auto &it : _varToLowerBounds )
        inputQuery.setLowerBound( it.first, it.second );
}

void MpsParser::populateEquations( InputQuery &inputQuery ) const
{
    for ( unsigned index = 0; index < _numRows; ++index )
    {
        if ( static_cast<int>( index ) == _indexOfObjective )
        {
            // Ignore objective function since we only handle feasibility query
            // for now.
            continue;
        }
        else
        {
            Equation equation;
            populateEquation( equation, index );
            inputQuery.addEquation( equation );
        }
    }
}

void MpsParser::populateEquation( Equation &equation, unsigned index ) const
{
    const Map<unsigned, double> &coeffs = _equationIndexToCoefficients[index];

    for ( const auto &pair : coeffs )
        equation.addAddend( pair.second, pair.first );

    switch ( _equationIndexToRowType[index] )
    {
    case RowType::EQ:
        equation.setType( Equation::EQ );
        break;

    case RowType::LE:
        equation.setType( Equation::LE );
        break;

    case RowType::GE:
        equation.setType( Equation::GE );
        break;

    case RowType::OBJ:
        // Ignore objective for now
        ASSERT( false );
        break;
    }

    if ( _equationIndexToRhs.exists( index ) )
        equation.setScalar( _equationIndexToRhs[index] );
    else
        equation.setScalar( 0 );
}

void MpsParser::addPiecewiseLinearConstraints( InputQuery &inputQuery ) const
{
    std::cout << inputQuery.getNumberOfVariables() << std::endl;
    std::cout << _integerVariables.size() << std::endl;
    for ( const auto &integerVariable : _integerVariables )
    {
        double lb = getLowerBound( integerVariable );
        double ub = getUpperBound( integerVariable );

        List<PiecewiseLinearCaseSplit> splits;
        for ( int i = ceil( lb ); i <= floor( ub ); ++i )
        {
            PiecewiseLinearCaseSplit split;
            split.storeBoundTightening( Tightening( integerVariable, i, Tightening::LB ) );
            split.storeBoundTightening( Tightening( integerVariable, i, Tightening::UB ) );
            splits.append( split );
        }
        DisjunctionConstraint *disj = new DisjunctionConstraint( splits );
        inputQuery.addPiecewiseLinearConstraint( disj );
    }
}
