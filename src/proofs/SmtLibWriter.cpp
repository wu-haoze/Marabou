/*********************                                                        */
/*! \file SmtLibWriter.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Omri Isac, Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2022 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]
 **/

#include "SmtLibWriter.h"

void SmtLibWriter::addHeader( unsigned numberOfVariables, List<String> &instance,
                              bool nonLinear )
{
    if ( nonLinear )
        instance.append( "( set-logic QF_NRA )\n" );
    else
        instance.append( "( set-logic QF_LRA )\n" );
    for ( unsigned i = 0; i < numberOfVariables; ++i )
        instance.append( "( declare-fun x" + std::to_string( i ) + " () Real )\n" );
}

void SmtLibWriter::addFooter( List<String> &instance )
{
    instance.append(  "( check-sat )\n" );
    instance.append(  "( exit )\n" );
}

void SmtLibWriter::addReLUConstraint( unsigned b, unsigned f, const PhaseStatus status, List<String> &instance )
{
    if ( status == PHASE_NOT_FIXED )
        instance.append(  "( assert ( = x" + std::to_string( f ) + " ( ite ( >= x" + std::to_string( b ) + " 0 ) x" + std::to_string( b )+ " 0 ) ) )\n" );
    else if ( status == RELU_PHASE_ACTIVE )
        instance.append(  "( assert ( = x" + std::to_string( f ) + " x" + std::to_string( b ) + " ) )\n" );
    else if ( status == RELU_PHASE_INACTIVE )
        instance.append(  "( assert ( = x" + std::to_string( f ) + " 0 ) )\n" );
}

void SmtLibWriter::addSoftmaxConstraint( const Vector<unsigned> &inputs,
                                         const Vector<unsigned> &outputs,
                                         List<String> &instance )
{
    std::cout << inputs.size() << " " << outputs.size() << " " << instance.size() << std::endl;
}

void SmtLibWriter::addTableauRow( const Vector<double> &row, List<String> &instance )
{
    unsigned size = row.size();
    unsigned counter = 0;
    String assertRowLine = "( assert ( = 0";

    for ( unsigned i = 0; i < size - 1; ++i )
    {
        if ( FloatUtils::isZero( row[i] ) )
            continue;

        assertRowLine += String( " ( + ( * " ) + signedValue( row[i] ) + " x" + std::to_string( i ) + " )";
        ++counter;
    }

    // Add last element
    assertRowLine += String( " ( * " ) + signedValue( row[size - 1] ) + " x" + std::to_string( size - 1 ) + " )";

    for ( unsigned i = 0; i < counter + 2 ; ++i )
        assertRowLine += String( " )" );

    instance.append( assertRowLine + "\n" );
}

void SmtLibWriter::addEquation( const Equation &equation, List<String> &instance )
{
    unsigned counter = 0;
    String assertRowLine = String( "( assert ( = " ) + signedValue( equation._scalar );

    Vector<Equation::Addend> addends;
    for ( const auto &addend : equation._addends )
        addends.append( addend );
    unsigned size = addends.size();

    for ( unsigned i = 0; i < size - 1; ++i )
    {
        assertRowLine +=
            ( String( " ( + ( * " ) + signedValue( addends[i]._coefficient ) + " x" +
              std::to_string( addends[i]._variable ) + " )" );
        ++counter;
    }

    // Add last element
    assertRowLine += ( String( " ( * " ) + signedValue( addends[size - 1]._coefficient ) +
                       " x" + std::to_string( addends[size - 1]._variable ) + " )" );

    for ( unsigned i = 0; i < counter + 2 ; ++i )
        assertRowLine += String( " )" );

    instance.append( assertRowLine + "\n" );
}

void SmtLibWriter::addQuadraticEquation( const QuadraticEquation &equation, List<String> &instance )
{
    unsigned counter = 0;
    String assertRowLine = String( "( assert ( = " ) + signedValue( equation._scalar );

    Vector<QuadraticEquation::Addend> addends;
    for ( const auto &addend : equation._addends )
        addends.append( addend );
    unsigned size = addends.size();

    for ( unsigned i = 0; i < size - 1; ++i )
    {
        if ( addends[i]._variables.size() == 1 )
        {
            assertRowLine +=
                String( " ( + ( * " ) + signedValue( addends[i]._coefficient ) + " x" +
                std::to_string( addends[i]._variables[0] ) + " )";
        }
        else
        {
            assertRowLine +=
                String( " ( + ( * ( * " ) + signedValue( addends[i]._coefficient ) + " x" +
                std::to_string( addends[i]._variables[0] ) + " ) " +
                std::to_string( addends[i]._variables[1] ) + " )";
        }

        ++counter;
    }

    // Add last element
    if ( addends[size-1]._variables.size() == 1 )
    {
        assertRowLine +=
            String( " ( * " ) + signedValue( addends[size - 1]._coefficient ) + " x" +
            std::to_string( addends[size - 1]._variables[0] ) + " )";
    }
    else
    {
        assertRowLine +=
            String( " ( * ( * " ) + signedValue( addends[size-1]._coefficient ) + " x" +
            std::to_string( addends[size-1]._variables[0] ) + " ) " +
            std::to_string( addends[size-1]._variables[1] ) + " )";
    }

    for ( unsigned i = 0; i < counter + 2 ; ++i )
        assertRowLine += String( " )" );

    instance.append( assertRowLine + "\n" );
}

void SmtLibWriter::addGroundUpperBounds( Vector<double> &bounds, List<String> &instance )
{
    unsigned n = bounds.size();
    for ( unsigned i = 0; i < n; ++i )
        instance.append( String( "( assert ( <= x" + std::to_string( i ) ) + String( " " ) + signedValue( bounds[i] ) + " ) )\n" );
}

void SmtLibWriter::addGroundLowerBounds( Vector<double> &bounds, List<String> &instance )
{
    unsigned n = bounds.size();
    for ( unsigned i = 0; i < n; ++i )
        instance.append( String( "( assert ( >= x" + std::to_string( i ) ) + String( " " ) + signedValue( bounds[i] ) + " ) )\n" );
}

void SmtLibWriter::writeInstanceToFile( IFile &file, const List<String> &instance )
{
    file.open( File::MODE_WRITE_TRUNCATE );

    for ( const String &s : instance )
        file.write( s );

    file.close();
}

String SmtLibWriter::signedValue( double val )
{
    return val > 0 ? std::to_string( val ) : "( - " + std::to_string( abs( val ) ) + " )";
}
