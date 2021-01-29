/*********************                                                        */
/*! \file Tableau.h
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

#ifndef __Tableau_h__
#define __Tableau_h__

#include "BoundManager.h"
#include "GurobiWrapper.h"
#include "IBasisFactorization.h"
#include "ITableau.h"
#include "MString.h"
#include "Map.h"
#include "Set.h"
#include "SparseColumnsOfBasis.h"
#include "SparseMatrix.h"
#include "SparseUnsortedList.h"
#include "Statistics.h"

#define TABLEAU_LOG( x, ... ) LOG( GlobalConfiguration::TABLEAU_LOGGING, "Tableau: %s\n", x )

class Equation;
class ICostFunctionManager;
class PiecewiseLinearCaseSplit;
class TableauState;

class Tableau : public ITableau, public IBasisFactorization::BasisColumnOracle
{
public:
    Tableau();
    ~Tableau();

    /*
      Allocate space for the various data structures
      n: total number of variables
      m: number of constraints (rows)
    */
    void setDimensions( unsigned m, unsigned n );

    /*
      Initialize the constraint matrix
    */
    void setConstraintMatrix( const double *A );

    /*
      Get the current set of basic variables
    */
    Set<unsigned> getBasicVariables() const;

    /*
      Set/get the values of the right hand side vector, b, of size m.
      Set either the whole vector or a specific entry
    */
    void setRightHandSide( const double *b );
    void setRightHandSide( unsigned index, double value );
    const double *getRightHandSide() const;

    /*
      Perform backward/forward transformations using the basis factorization.
    */
    void forwardTransformation( const double *y, double *x ) const;
    void backwardTransformation( const double *y, double *x ) const;

    /*
      Mark a variable as basic in the initial basis
     */
    void markAsBasic( unsigned variable );

    /*
      Initialize the tableau matrices (_B and _AN) according to the
      initial set of basic variables. Assign all non-basic variables
      to lower bounds and computes the assignment. Assign the initial basic
      indices according to the equations.
    */
    void initializeTableau( const List<unsigned> &initialBasicVariables );
    void assignIndexToBasicVariable( unsigned variable, unsigned index );

    /*
      Get the Tableau's dimensions.
    */
    unsigned getM() const;
    unsigned getN() const;

    /*
      Given an index of a non-basic variable in the range [0,n-m),
      return the original variable that it corresponds to.
    */
    unsigned nonBasicIndexToVariable( unsigned index ) const;

    /*
      Given an index of a basic variable in the range [0,m),
      return the original variable that it corresponds to.
    */
    unsigned basicIndexToVariable( unsigned index ) const;

    /*
      Given a variable, returns the index of that variable. The result
      is in range [0,m) if the variable is basic, or in range [0,n-m)
      if the variable is non-basic.
    */
    unsigned variableToIndex( unsigned index ) const;

    /*
      True iff the variable is basic
    */
    bool isBasic( unsigned variable ) const;

    /*
      Compute the multipliers for a given list of row coefficient.
    */
    void computeMultipliers( double *rowCoefficients );

    /*
      Extract a row from the tableau.
    */
    void getTableauRow( unsigned index, TableauRow *row );

    /*
      Get the original constraint matrix A or a column thereof,
      in dense form.
    */
    const SparseMatrix *getSparseA() const;
    const double *getAColumn( unsigned variable ) const;
    void getSparseAColumn( unsigned variable, SparseUnsortedList *result ) const;
    void getSparseARow( unsigned row, SparseUnsortedList *result ) const;
    const SparseUnsortedList *getSparseAColumn( unsigned variable ) const;
    const SparseUnsortedList *getSparseARow( unsigned row ) const;

    /*
      Register or unregister to watch a variable.
    */
    void registerToWatchAllVariables( VariableWatcher *watcher );
    void registerToWatchVariable( VariableWatcher *watcher, unsigned variable );
    void unregisterToWatchVariable( VariableWatcher *watcher, unsigned variable );

    /*
      Notify all watchers of the given variable of a value update,
      or of changes to its bounds.
    */
    void notifyLowerBound( unsigned variable, double bound );
    void notifyUpperBound( unsigned variable, double bound );

    /*
      Have the Tableau start reporting statistics.
     */
    void setStatistics( Statistics *statistics );

    /*
      Methods for accessing the basis matrix and extracting
      from it explicit equations. These operations may be
      costly if the explicit basis is not available - this
      also depends on the basis factorization in use.

      These equations correspond to: B * xB + An * xN = b

      Can also extract the inverse basis matrix.
    */
    bool basisMatrixAvailable() const;
    double *getInverseBasisMatrix() const;

    void getColumnOfBasis( unsigned column, double *result ) const;
    void getColumnOfBasis( unsigned column, SparseUnsortedList *result ) const;
    void getSparseBasis( SparseColumnsOfBasis &basis ) const;

private:
    /*
      Variable watchers
    */
    typedef List<VariableWatcher *> VariableWatchers;
    HashMap<unsigned, VariableWatchers> _variableToWatchers;
    List<VariableWatcher *> _globalWatchers;

    /*
      The dimensions of matrix A
    */
    unsigned _n;
    unsigned _m;

    /*
      The constraint matrix A, and a collection of its
      sparse columns. The matrix is also stored in dense
      form (column-major).
    */
    SparseMatrix *_A;
    SparseUnsortedList **_sparseColumnsOfA;
    SparseUnsortedList **_sparseRowsOfA;
    double *_denseA;

    /*
      The right hand side vector of Ax = b
    */
    double *_b;

    /*
      Working memory (of size m and n).
    */
    double *_workM;
    double *_workN;

    /*
      A unit vector of size m
    */
    double *_unitVector;

    /*
      The current factorization of the basis
    */
    IBasisFactorization *_basisFactorization;

    /*
      The multiplier vector
    */
    double *_multipliers;

    /*
      Mapping between basic variables and indices (length m)
    */
    unsigned *_basicIndexToVariable;

    /*
      Mapping between non-basic variables and indices (length n - m)
    */
    unsigned *_nonBasicIndexToVariable;

    /*
      Mapping from variable to index, either basic or non-basic
    */
    unsigned *_variableToIndex;

    /*
      The set of current basic variables
    */
    Set<unsigned> _basicVariables;

    /*
      Statistics collection
    */
    Statistics *_statistics;

    /*
      True if and only if the rhs vector _b is all zeros. This can
      simplify some of the computations.
     */
    bool _rhsIsAllZeros;

    /*
      Free all allocated memory.
    */
    void freeMemoryIfNeeded();

    /*
      For debugging purposes only
    */
    void verifyInvariants();

    static String basicStatusToString( unsigned status );
};

#endif // __Tableau_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
