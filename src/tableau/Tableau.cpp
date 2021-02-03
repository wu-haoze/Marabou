/*********************                                                        */
/*! \file Tableau.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling, Parth Shah
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "BasisFactorizationFactory.h"
#include "CSRMatrix.h"
#include "ConstraintMatrixAnalyzer.h"
#include "Debug.h"
#include "Equation.h"
#include "FloatUtils.h"
#include "MStringf.h"
#include "MalformedBasisException.h"
#include "MarabouError.h"
#include "PiecewiseLinearCaseSplit.h"
#include "Tableau.h"
#include "TableauRow.h"

#include <string.h>

Tableau::Tableau()
    : _n ( 0 )
    , _m ( 0 )
    , _A( NULL )
    , _sparseColumnsOfA( NULL )
    , _sparseRowsOfA( NULL )
    , _denseA( NULL )
    , _b( NULL )
    , _workM( NULL )
    , _workN( NULL )
    , _unitVector( NULL )
    , _basisFactorization( NULL )
    , _multipliers( NULL )
    , _basicIndexToVariable( NULL )
    , _nonBasicIndexToVariable( NULL )
    , _variableToIndex( NULL )
    , _statistics( NULL )
    , _rhsIsAllZeros( true )
{
}

Tableau::~Tableau()
{
    freeMemoryIfNeeded();
}

void Tableau::freeMemoryIfNeeded()
{
    if ( _A )
    {
        delete _A;
        _A = NULL;
    }

    if ( _sparseColumnsOfA )
    {
        for ( unsigned i = 0; i < _n; ++i )
        {
            if ( _sparseColumnsOfA[i] )
            {
                delete _sparseColumnsOfA[i];
                _sparseColumnsOfA[i] = NULL;
            }
        }

        delete[] _sparseColumnsOfA;
        _sparseColumnsOfA = NULL;
    }

    if ( _sparseRowsOfA )
    {
        for ( unsigned i = 0; i < _m; ++i )
        {
            if ( _sparseRowsOfA[i] )
            {
                delete _sparseRowsOfA[i];
                _sparseRowsOfA[i] = NULL;
            }
        }

        delete[] _sparseRowsOfA;
        _sparseRowsOfA = NULL;
    }

    if ( _denseA )
    {
        delete[] _denseA;
        _denseA = NULL;
    }

    if ( _b )
    {
        delete[] _b;
        _b = NULL;
    }

    if ( _unitVector )
    {
        delete[] _unitVector;
        _unitVector = NULL;
    }

    if ( _multipliers )
    {
        delete[] _multipliers;
        _multipliers = NULL;
    }

    if ( _basicIndexToVariable )
    {
        delete[] _basicIndexToVariable;
        _basicIndexToVariable = NULL;
    }

    if ( _variableToIndex )
    {
        delete[] _variableToIndex;
        _variableToIndex = NULL;
    }

    if ( _nonBasicIndexToVariable )
    {
        delete[] _nonBasicIndexToVariable;
        _nonBasicIndexToVariable = NULL;
    }

    if ( _basisFactorization )
    {
        delete _basisFactorization;
        _basisFactorization = NULL;
    }

    if ( _workM )
    {
        delete[] _workM;
        _workM = NULL;
    }

    if ( _workN )
    {
        delete[] _workN;
        _workN = NULL;
    }
}

void Tableau::setDimensions( unsigned m, unsigned n )
{
    _m = m;
    _n = n;

    _A = new CSRMatrix();
    if ( !_A )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::A" );

    _sparseColumnsOfA = new SparseUnsortedList *[n];
    if ( !_sparseColumnsOfA )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::sparseColumnsOfA" );

    for ( unsigned i = 0; i < n; ++i )
    {
        _sparseColumnsOfA[i] = new SparseUnsortedList( _m );
        if ( !_sparseColumnsOfA[i] )
            throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::sparseColumnsOfA[i]" );
    }

    _sparseRowsOfA = new SparseUnsortedList *[m];
    if ( !_sparseRowsOfA )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::sparseRowOfA" );

    for ( unsigned i = 0; i < m; ++i )
    {
        _sparseRowsOfA[i] = new SparseUnsortedList( _n );
        if ( !_sparseRowsOfA[i] )
            throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::sparseRowOfA[i]" );
    }

    _denseA = new double[m*n];
    if ( !_denseA )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::denseA" );

    _b = new double[m];
    if ( !_b )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::b" );

    _unitVector = new double[m];
    if ( !_unitVector )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::unitVector" );

    _basicIndexToVariable = new unsigned[m];
    if ( !_basicIndexToVariable )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::basicIndexToVariable" );

    _variableToIndex = new unsigned[n];
    if ( !_variableToIndex )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::variableToIndex" );

    _nonBasicIndexToVariable = new unsigned[n-m];
    if ( !_nonBasicIndexToVariable )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::nonBasicIndexToVariable" );

    _basisFactorization = BasisFactorizationFactory::createBasisFactorization( _m, *this );
    if ( !_basisFactorization )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::basisFactorization" );
    _basisFactorization->setStatistics( _statistics );

    _workM = new double[m];
    if ( !_workM )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::work" );

    _workN = new double[n];
    if ( !_workN )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "Tableau::work" );
}

void Tableau::setConstraintMatrix( const double *A )
{
    _A->initialize( A, _m, _n );

    for ( unsigned column = 0; column < _n; ++column )
    {
        for ( unsigned row = 0; row < _m; ++row )
            _denseA[column*_m + row] = A[row*_n + column];

        _sparseColumnsOfA[column]->initialize( _denseA + ( column * _m ), _m );
    }

    for ( unsigned row = 0; row < _m; ++row )
        _sparseRowsOfA[row]->initialize( A + ( row * _n ), _n );
}

void Tableau::markAsBasic( unsigned variable )
{
    _basicVariables.insert( variable );
}

void Tableau::assignIndexToBasicVariable( unsigned variable, unsigned index )
{
    _basicIndexToVariable[index] = variable;
    _variableToIndex[variable] = index;
}

void Tableau::initializeTableau( const List<unsigned> &initialBasicVariables )
{
    _basicVariables.clear();

    // Assign the basic indices
    unsigned basicIndex = 0;
    for( unsigned basicVar : initialBasicVariables )
    {
        markAsBasic( basicVar );
        assignIndexToBasicVariable( basicVar, basicIndex );
        ++basicIndex;
    }

    // Assign the non-basic indices
    unsigned nonBasicIndex = 0;
    for ( unsigned i = 0; i < _n; ++i )
    {
        if ( !_basicVariables.exists( i ) )
        {
            _nonBasicIndexToVariable[nonBasicIndex] = i;
            _variableToIndex[i] = nonBasicIndex;
            ++nonBasicIndex;
        }
    }
    ASSERT( nonBasicIndex == _n - _m );

    // Factorize the basis
    _basisFactorization->obtainFreshBasis();

}

unsigned Tableau::basicIndexToVariable( unsigned index ) const
{
    return _basicIndexToVariable[index];
}

unsigned Tableau::nonBasicIndexToVariable( unsigned index ) const
{
    return _nonBasicIndexToVariable[index];
}

unsigned Tableau::variableToIndex( unsigned index ) const
{
    return _variableToIndex[index];
}

void Tableau::setRightHandSide( const double *b )
{
    memcpy( _b, b, sizeof(double) * _m );

    for ( unsigned i = 0; i < _m; ++i )
    {
        if ( !FloatUtils::isZero( _b[i] ) )
            _rhsIsAllZeros = false;
    }
}

void Tableau::setRightHandSide( unsigned index, double value )
{
    _b[index] = value;

    if ( !FloatUtils::isZero( value ) )
        _rhsIsAllZeros = false;
}

void Tableau::computeMultipliers( double *rowCoefficients )
{
    _basisFactorization->backwardTransformation( rowCoefficients, _multipliers );
}


bool Tableau::isBasic( unsigned variable ) const
{
    return _basicVariables.exists( variable );
}

unsigned Tableau::getM() const
{
    return _m;
}

unsigned Tableau::getN() const
{
    return _n;
}

void Tableau::getTableauRow( unsigned index, TableauRow *row )
{
    /*
      Let e denote a unit matrix with 1 in its *index* entry.
      A row is then computed by: e * inv(B) * -AN. e * inv(B) is
      solved by invoking BTRAN.
    */

    ASSERT( index < _m );

    std::fill( _unitVector, _unitVector + _m, 0.0 );
    _unitVector[index] = 1;
    computeMultipliers( _unitVector );

    for ( unsigned i = 0; i < _n - _m; ++i )
    {
        row->_row[i]._var = _nonBasicIndexToVariable[i];
        row->_row[i]._coefficient = 0;

        SparseUnsortedList *column = _sparseColumnsOfA[_nonBasicIndexToVariable[i]];

        for ( const auto &entry : *column )
            row->_row[i]._coefficient -= ( _multipliers[entry._index] * entry._value );
    }

    /*
      If the rhs vector is all zeros, the row's scalar will be 0. This is
      the common case. If the rhs vector is not zero, we need to compute
      the scalar directly.
    */
    if ( _rhsIsAllZeros )
        row->_scalar = 0;
    else
    {
        _basisFactorization->forwardTransformation( _b, _workM );
        row->_scalar = _workM[index];
    }

    row->_lhs = _basicIndexToVariable[index];
}

const SparseMatrix *Tableau::getSparseA() const
{
    return _A;
}

const double *Tableau::getAColumn( unsigned variable ) const
{
    return _denseA + ( variable * _m );
}

void Tableau::getSparseAColumn( unsigned variable, SparseUnsortedList *result ) const
{
    _sparseColumnsOfA[variable]->storeIntoOther( result );
}

const SparseUnsortedList *Tableau::getSparseAColumn( unsigned variable ) const
{
    return _sparseColumnsOfA[variable];
}

const SparseUnsortedList *Tableau::getSparseARow( unsigned row ) const
{
    return _sparseRowsOfA[row];
}

void Tableau::getSparseARow( unsigned row, SparseUnsortedList *result ) const
{
    _sparseRowsOfA[row]->storeIntoOther( result );
}

void Tableau::registerToWatchVariable( VariableWatcher *watcher, unsigned variable )
{
    _variableToWatchers[variable].append( watcher );
}

void Tableau::unregisterToWatchVariable( VariableWatcher *watcher, unsigned variable )
{
    _variableToWatchers[variable].erase( watcher );
}

void Tableau::registerToWatchAllVariables( VariableWatcher *watcher )
{
    _globalWatchers.append( watcher );
}

void Tableau::notifyLowerBound( unsigned variable, double bound )
{
    for ( auto &watcher : _globalWatchers )
        watcher->notifyLowerBound( variable, bound );

    if ( _variableToWatchers.exists( variable ) )
    {
        for ( auto &watcher : _variableToWatchers[variable] )
            watcher->notifyLowerBound( variable, bound );
    }
}

void Tableau::notifyUpperBound( unsigned variable, double bound )
{
    for ( auto &watcher : _globalWatchers )
        watcher->notifyUpperBound( variable, bound );

    if ( _variableToWatchers.exists( variable ) )
    {
        for ( auto &watcher : _variableToWatchers[variable] )
            watcher->notifyUpperBound( variable, bound );
    }
}

const double *Tableau::getRightHandSide() const
{
    return _b;
}

void Tableau::forwardTransformation( const double *y, double *x ) const
{
    _basisFactorization->forwardTransformation( y, x );
}

void Tableau::backwardTransformation( const double *y, double *x ) const
{
    _basisFactorization->backwardTransformation( y, x );
}

void Tableau::setStatistics( Statistics *statistics )
{
    _statistics = statistics;
}

void Tableau::verifyInvariants()
{
}

bool Tableau::basisMatrixAvailable() const
{
    return _basisFactorization->explicitBasisAvailable();
}

double *Tableau::getInverseBasisMatrix() const
{
    ASSERT( basisMatrixAvailable() );

    double *result = new double[_m * _m];
    _basisFactorization->invertBasis( result );
    return result;
}

void Tableau::getColumnOfBasis( unsigned column, double *result ) const
{
    ASSERT( column < _m );
    _sparseColumnsOfA[_basicIndexToVariable[column]]->toDense( result );
}

void Tableau::getSparseBasis( SparseColumnsOfBasis &basis ) const
{
    for ( unsigned i = 0; i < _m; ++i )
        basis._columns[i] = _sparseColumnsOfA[_basicIndexToVariable[i]];
}

void Tableau::getColumnOfBasis( unsigned column, SparseUnsortedList *result ) const
{
    ASSERT( column < _m );
    _sparseColumnsOfA[_basicIndexToVariable[column]]->storeIntoOther( result );
}


Set<unsigned> Tableau::getBasicVariables() const
{
    return _basicVariables;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
