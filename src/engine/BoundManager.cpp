/*********************                                                        */
/*! \file BoundManager.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu, Aleksandar Zeljic
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include "BoundManager.h"

#include "Debug.h"
#include "FloatUtils.h"
#include "InfeasibleQueryException.h"
#include "Tableau.h"
#include "Tightening.h"

using namespace CVC4::context;

BoundManager::BoundManager( Context &context )
    : _context( context )
    , _size( 0 )
    , _tableau( nullptr )
    , _consistentBounds( &_context )
    , _firstInconsistentTightening( 0, 0.0, Tightening::LB )
{
  _consistentBounds = true;
};

BoundManager::~BoundManager()
{
    for ( unsigned i = 0; i < _size; ++i )
    {
        _lowerBounds[i]->deleteSelf();
        _upperBounds[i]->deleteSelf();
        _tightenedLower[i]->deleteSelf();
        _tightenedUpper[i]->deleteSelf();
    }
};

void BoundManager::initialize( unsigned numberOfVariables )
{
    ASSERT( _size == 0 );

    for ( unsigned i = 0; i < numberOfVariables; ++i )
        registerNewVariable();

    ASSERT( _size == numberOfVariables );
}

unsigned BoundManager::registerNewVariable()
{
    ASSERT( _size == _lowerBounds.size() );
    ASSERT( _size == _upperBounds.size() );
    ASSERT( _size == _tightenedLower.size() );
    ASSERT( _size == _tightenedUpper.size() );

    unsigned newVar = _size++;

    _lowerBounds.append( new ( true ) CDO<double>( &_context ) );
    _upperBounds.append( new ( true ) CDO<double>( &_context ) );
    _tightenedLower.append( new ( true ) CDO<bool>( &_context ) );
    _tightenedUpper.append( new ( true ) CDO<bool>( &_context ) );

    *_lowerBounds[newVar] = FloatUtils::negativeInfinity();
    *_upperBounds[newVar] = FloatUtils::infinity();
    *_tightenedLower[newVar] = false;
    *_tightenedUpper[newVar] = false;

    return newVar;
}

unsigned BoundManager::getNumberOfVariables() const
{
    return _size;
}

bool BoundManager::tightenLowerBound( unsigned variable, double value )
{
    bool tightened = setLowerBound( variable, value );
    if ( tightened && _tableau != nullptr )
        _tableau->updateVariableToComplyWithLowerBoundUpdate( variable, value );
    return tightened;
}

bool BoundManager::tightenUpperBound( unsigned variable, double value )
{
    bool tightened = setUpperBound( variable, value );
    if ( tightened && _tableau != nullptr )
        _tableau->updateVariableToComplyWithUpperBoundUpdate( variable, value );
    return tightened;
}

void BoundManager::recordInconsistentBounds( unsigned variable, double value, Tightening::BoundType type )
{
  if ( _consistentBounds )
  {
    _consistentBounds = false;
    _firstInconsistentTightening = Tightening( variable, value, type );
  }
}

bool BoundManager::setLowerBound( unsigned variable, double value )
{
    ASSERT( variable < _size );
    if ( value > getLowerBound( variable ) )
    {
        *_lowerBounds[variable] = value;
        *_tightenedLower[variable] = true;
        if ( !consistentBounds( variable ) )
            recordInconsistentBounds( variable, value, Tightening::LB );
        return true;
    }
    return false;
}


bool BoundManager::setUpperBound( unsigned variable, double value )
{
    ASSERT( variable < _size );
    if ( value < getUpperBound( variable ) )
    {
        *_upperBounds[variable] = value;
        *_tightenedUpper[variable] = true;
        if ( !consistentBounds( variable ) )
          recordInconsistentBounds( variable, value, Tightening::UB );
        return true;
    }
    return false;
}

double BoundManager::getLowerBound( unsigned variable ) const
{
    ASSERT( variable < _size );
    return *_lowerBounds.get(variable);
}

double BoundManager::getUpperBound( unsigned variable ) const
{
    ASSERT( variable < _size );
    return *_upperBounds.get(variable);
}

void BoundManager::getTightenings( List<Tightening> &tightenings )
{
    for ( unsigned i = 0; i < _size; ++i )
    {
        if ( *_tightenedLower[i] )
        {
            tightenings.append( Tightening( i, *_lowerBounds[i], Tightening::LB ) );
            *_tightenedLower[i] = false;
        }

        if ( *_tightenedUpper[i] )
        {
            tightenings.append( Tightening( i, *_upperBounds[i], Tightening::UB ) );
            *_tightenedUpper[i] = false;
        }
    }
}

bool BoundManager::consistentBounds() const
{
    return _consistentBounds;
}

bool BoundManager::consistentBounds( unsigned variable ) const
{
    ASSERT( variable < _size );
    return FloatUtils::gte( getUpperBound( variable ), getLowerBound( variable ) );
}

void BoundManager::registerTableau( Tableau *ptrTableau )
{
    ASSERT( _tableau == nullptr );
    _tableau = ptrTableau;
}
