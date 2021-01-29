/*********************                                                        */
/*! \file ITableau.h
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

#ifndef __ITableau_h__
#define __ITableau_h__

#include "FloatUtils.h"
#include "List.h"
#include "Set.h"

class BoundManager;
class EntrySelectionStrategy;
class Equation;
class GurobiWrapper;
class ICostFunctionManager;
class PiecewiseLinearCaseSplit;
class SparseMatrix;
class SparseUnsortedList;
class SparseVector;
class Statistics;
class TableauRow;
class TableauState;

class ITableau
{
public:

    /*
      A class for allowing objects (e.g., piecewise linear
      constraints) to register and receive updates regarding changes
      in variable assignments and variable bounds.
    */
    class VariableWatcher
    {
    public:
        /*
          This callback will be invoked when the variable's value
          changes.
        */
        virtual void notifyVariableValue( unsigned /* variable */, double /* value */ ) {}

        /*
          These callbacks will be invoked when the variable's
          lower/upper bounds change.
        */
        virtual void notifyLowerBound( unsigned /* variable */, double /* bound */ ) {}
        virtual void notifyUpperBound( unsigned /* variable */, double /* bound */ ) {}
    };

    virtual void registerToWatchAllVariables( VariableWatcher *watcher ) = 0;
    virtual void registerToWatchVariable( VariableWatcher *watcher, unsigned variable ) = 0;
    virtual void unregisterToWatchVariable( VariableWatcher *watcher, unsigned variable ) = 0;

    virtual ~ITableau() {};

    virtual void setDimensions( unsigned m, unsigned n ) = 0;
    virtual void setConstraintMatrix( const double *A ) = 0;
    virtual void setRightHandSide( const double *b ) = 0;
    virtual void setRightHandSide( unsigned index, double value ) = 0;
    virtual void markAsBasic( unsigned variable ) = 0;
    virtual void initializeTableau( const List<unsigned> &initialBasicVariables ) = 0;
    virtual Set<unsigned> getBasicVariables() const = 0;
    virtual bool isBasic( unsigned variable ) const = 0;
    virtual void computeMultipliers( double *rowCoefficients ) = 0;
    virtual void dump() const = 0;
    virtual void dumpEquations() = 0;
    virtual unsigned nonBasicIndexToVariable( unsigned index ) const = 0;
    virtual unsigned basicIndexToVariable( unsigned index ) const = 0;
    virtual void assignIndexToBasicVariable( unsigned variable, unsigned index ) = 0;
    virtual unsigned variableToIndex( unsigned index ) const = 0;
    virtual unsigned getM() const = 0;
    virtual unsigned getN() const = 0;
    virtual void getTableauRow( unsigned index, TableauRow *row ) = 0;
    virtual const double *getAColumn( unsigned variable ) const = 0;
    virtual void getSparseAColumn( unsigned variable, SparseUnsortedList *result ) const = 0;
    virtual void getSparseARow( unsigned row, SparseUnsortedList *result ) const = 0;
    virtual const SparseUnsortedList *getSparseAColumn( unsigned variable ) const = 0;
    virtual const SparseUnsortedList *getSparseARow( unsigned row ) const = 0;
    virtual const SparseMatrix *getSparseA() const = 0;
    virtual void setStatistics( Statistics *statistics ) = 0;
    virtual const double *getRightHandSide() const = 0;
    virtual void forwardTransformation( const double *y, double *x ) const = 0;
    virtual void backwardTransformation( const double *y, double *x ) const = 0;
    virtual void verifyInvariants() = 0;
    virtual bool basisMatrixAvailable() const = 0;
    virtual double *getInverseBasisMatrix() const = 0;
    void setGurobi( GurobiWrapper *gurobi ) { _gurobi = gurobi; }
    void setBoundManager( BoundManager *boundManager ){ _boundManager = boundManager; };

protected:
    GurobiWrapper *_gurobi = NULL;
    BoundManager *_boundManager = NULL;
};

#endif // __ITableau_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
