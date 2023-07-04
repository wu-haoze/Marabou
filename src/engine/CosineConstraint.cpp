/*********************                                                        */
/*! \file CosineConstraint.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Teruhiro Tagomori, Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** See the description of the class in CosineConstraint.h.
 **/

#include "CosineConstraint.h"

#include "TranscendentalConstraint.h"
#include "Debug.h"
#include "DivideStrategy.h"
#include "FloatUtils.h"
#include "GlobalConfiguration.h"
#include "ITableau.h"
#include "InputQuery.h"
#include "MStringf.h"
#include "MarabouError.h"
#include "Statistics.h"
#include "TableauRow.h"

#include "math.h"

#ifdef _WIN32
#define __attribute__(x)
#endif

CosineConstraint::CosineConstraint( unsigned b, unsigned f )
    : TranscendentalConstraint()
    , _b( b )
    , _f( f )
    , _haveEliminatedVariables( false )
{
}

CosineConstraint::CosineConstraint( const String &serializedCosine )
    : _haveEliminatedVariables( false )
{
    String constraintType = serializedCosine.substring( 0, 6 );
    ASSERT( constraintType == String( "cosine" ) );

    // Remove the constraint type in serialized form
    String serializedValues = serializedCosine.substring( 7, serializedCosine.length() - 7 );
    List<String> values = serializedValues.tokenize( "," );

    ASSERT( values.size() == 2 );

    auto var = values.begin();
    _f = atoi( var->ascii() );
    ++var;
    _b = atoi( var->ascii() );
}

TranscendentalFunctionType CosineConstraint::getType() const
{
    return TranscendentalFunctionType::COSINE;
}

TranscendentalConstraint *CosineConstraint::duplicateConstraint() const
{
    CosineConstraint *clone = new CosineConstraint( _b, _f );
    *clone = *this;
    return clone;
}

void CosineConstraint::restoreState( const TranscendentalConstraint *state )
{
    const CosineConstraint *cosine = dynamic_cast<const CosineConstraint *>( state );
    *this = *cosine;
}

void CosineConstraint::registerAsWatcher( ITableau *tableau )
{
    tableau->registerToWatchVariable( this, _b );
    tableau->registerToWatchVariable( this, _f );
}

void CosineConstraint::unregisterAsWatcher( ITableau *tableau )
{
    tableau->unregisterToWatchVariable( this, _b );
    tableau->unregisterToWatchVariable( this, _f );
}

void CosineConstraint::notifyLowerBound( unsigned variable, double bound )
{
    ASSERT( variable == _b || variable == _f );

    if ( _statistics )
        _statistics->incLongAttribute(
            Statistics::NUM_BOUND_NOTIFICATIONS_TO_TRANSCENDENTAL_CONSTRAINTS );

    if ( variable == _f )
      tightenLowerBound( _f, std::min(1.0, std::max(-1.0, bound ) ) );
    else {
    if ( tightenLowerBound( variable, bound ) )
    {
        if ( existsUpperBound( _b ) )
        {
          double newLb = -1;
          double newUb = 1;
          findRangeOfCosOutput( getLowerBound( _b ), getUpperBound( _b ), newLb, newUb );
          tightenLowerBound( _f, std::min(1.0, std::max(-1.0, newLb ) ) );
          tightenUpperBound( _f, std::min(1.0, std::max(-1.0, newUb ) ) );
        }
      }
    }
}

void CosineConstraint::notifyUpperBound( unsigned variable, double bound )
{
    ASSERT( variable == _b || variable == _f );

    if ( _statistics )
        _statistics->incLongAttribute(
            Statistics::NUM_BOUND_NOTIFICATIONS_TO_TRANSCENDENTAL_CONSTRAINTS );

    if ( variable == _f )
      tightenUpperBound( _f, std::min(1.0, std::max(-1.0, bound ) ) );
    else {
    if ( tightenUpperBound( variable, bound ) )
    {
        if ( existsLowerBound( _b ) )
        {
          double newLb = -1;
          double newUb = 1;
          findRangeOfCosOutput( getLowerBound( _b ), getUpperBound( _b ), newLb, newUb );
          tightenLowerBound( _f, std::min(1.0, std::max(-1.0, newLb ) ) );
          tightenUpperBound( _f, std::min(1.0, std::max(-1.0, newUb ) ) );
        }
      }
    }
}

bool CosineConstraint::participatingVariable( unsigned variable ) const
{
    return ( variable == _b ) || ( variable == _f );
}

List<unsigned> CosineConstraint::getParticipatingVariables() const
{
    return List<unsigned>( { _b, _f } );
}

void CosineConstraint::dump( String &output ) const
{
    output = Stringf( "CosineConstraint: x%u = Cosine( x%u ).\n", _f, _b );

    output += Stringf( "b in [%s, %s], ",
                       existsLowerBound( _b ) ? Stringf( "%lf", getLowerBound( _b ) ).ascii() : "-inf",
                       existsUpperBound( _b ) ? Stringf( "%lf", getUpperBound( _b ) ).ascii() : "inf" );

    output += Stringf( "f in [%s, %s]",
                       existsLowerBound( _f ) ? Stringf( "%lf", getLowerBound( _f ) ).ascii() : "1",
                       existsUpperBound( _f ) ? Stringf( "%lf", getUpperBound( _f ) ).ascii() : "0" );
}

void CosineConstraint::updateVariableIndex( unsigned oldIndex, unsigned newIndex )
{
	ASSERT( oldIndex == _b || oldIndex == _f );
    ASSERT( !_assignment.exists( newIndex ) &&
            !_lowerBounds.exists( newIndex ) &&
            !_upperBounds.exists( newIndex ) &&
            newIndex != _b && newIndex != _f );

    if ( _assignment.exists( oldIndex ) )
    {
        _assignment[newIndex] = _assignment.get( oldIndex );
        _assignment.erase( oldIndex );
    }

    if ( _lowerBounds.exists( oldIndex ) )
    {
        _lowerBounds[newIndex] = _lowerBounds.get( oldIndex );
        _lowerBounds.erase( oldIndex );
    }

    if ( _upperBounds.exists( oldIndex ) )
    {
        _upperBounds[newIndex] = _upperBounds.get( oldIndex );
        _upperBounds.erase( oldIndex );
    }

    if ( oldIndex == _b )
        _b = newIndex;
    else if ( oldIndex == _f )
        _f = newIndex;
}

void CosineConstraint::eliminateVariable( __attribute__((unused)) unsigned variable,
                                        __attribute__((unused)) double fixedValue )
{
    ASSERT( variable == _b || variable == _f );

    // In a Cosine constraint, if a variable is removed the entire constraint can be discarded.
    _haveEliminatedVariables = true;
}

bool CosineConstraint::constraintObsolete() const
{
    return _haveEliminatedVariables;
}

void CosineConstraint::getEntailedTightenings( List<Tightening> &tightenings ) const
{
  if (existsLowerBound(_b)) {
    double bLowerBound = getLowerBound( _b );
    tightenings.append( Tightening( _b, bLowerBound, Tightening::LB ) );
  }
  if (existsLowerBound(_f)) {
    double fLowerBound = getLowerBound( _f );
    tightenings.append( Tightening( _f, fLowerBound, Tightening::LB ) );
  }
  if (existsUpperBound(_b)) {
    double bUpperBound = getUpperBound( _b );
    tightenings.append( Tightening( _b, bUpperBound, Tightening::UB ) );
  }
  if (existsUpperBound(_f)) {
    double fUpperBound = getUpperBound( _f );
    tightenings.append( Tightening( _f, fUpperBound, Tightening::UB ) );
  }
}

String CosineConstraint::serializeToString() const
{
    return Stringf( "cosine,%u,%u", _f, _b );
}

unsigned CosineConstraint::getB() const
{
    return _b;
}

unsigned CosineConstraint::getF() const
{
    return _f;
}

double CosineConstraint::cosine( double x )
{
    return std::cos( x );
}

double CosineConstraint::cosineDerivative( double x )
{
  return std::sin( -x );
}

// Define a function that takes a lower and upper bound of x as parameters
void CosineConstraint::findRangeOfCosOutput(double lower, double upper, double &newLb, double &newUb) {
  // Check if the range is valid
  if (lower > upper) {
    return;
  }
  // Check if the range covers the whole period of cos(x)
  if (upper - lower >= 2 * M_PI) {
    newLb = -1;
    newUb = 1;
    return;
  }

  // Initialize the min and max values to be the cos of the lower bound
  double min = std::cos(upper);
  double max = std::cos(upper);
  // Loop through the range of x with a small increment
  for (double x = lower; x <= upper; x += 0.0005) {
    // Calculate the cos of x
    double y = cos(x);
    // Update the min and max values if needed
    if (y < min) {
      min = y;
    }
    if (y > max) {
      max = y;
    }
  }
  newLb = min;
  newUb = max;
}
