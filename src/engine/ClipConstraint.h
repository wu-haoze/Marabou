/*********************                                                        */
/*! \file ClipConstraint.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 **/

#ifndef __ClipConstraint_h__
#define __ClipConstraint_h__

#include "PiecewiseLinearConstraint.h"

class ClipConstraint : public PiecewiseLinearConstraint
{

public:
    /*
      The f variable is the clip value of the b variable
    */
  ClipConstraint( unsigned b, unsigned f, double floor, double ceiling );
    ClipConstraint( const String &serializedAbs );

    /*
      Get the type of this constraint.
    */
    PiecewiseLinearFunctionType getType() const override;

    /*
      Return a clone of the constraint.
    */
    PiecewiseLinearConstraint *duplicateConstraint() const override;

    /*
      Restore the state of this constraint from the given one.
    */
    void restoreState( const PiecewiseLinearConstraint *state ) override;

    /*
      Register/unregister the constraint with a talbeau.
     */
    void registerAsWatcher( ITableau *tableau) override;
    void unregisterAsWatcher( ITableau *tableau) override;

    /*
      These callbacks are invoked when a watched variable's value
      changes, or when its bounds change.
    */
    void notifyLowerBound( unsigned variable, double bound ) override;
    void notifyUpperBound( unsigned variable, double bound ) override;

    /*
       Returns true iff the variable participates in this piecewise
       linear constraint.
     */
    bool participatingVariable( unsigned variable ) const override;

    /*
      Get the list of variables participating in this constraint.
    */
    List<unsigned> getParticipatingVariables() const override;

    /*
      Returns true iff the assignment satisfies the constraint
    */
    bool satisfied() const override;

    /*
      Returns a list of possible fixes for the violated constraint.
    */
    List<PiecewiseLinearConstraint::Fix> getPossibleFixes() const override;

    /*
      Return a list of smart fixes for violated constraint.
      Currently not implemented, just calls getPossibleFixes().
    */
    List<PiecewiseLinearConstraint::Fix> getSmartFixes( ITableau *tableau ) const override;

    /*
      Returns the list of case splits that this piecewise linear
      constraint breaks into:

      y = |x| <-->
         ( x <= 0 /\ y = -x ) \/ ( x >= 0 /\ y = x )
    */
    List<PiecewiseLinearCaseSplit> getCaseSplits() const override;

    /*
      If the constraint's phase has been fixed, get the (valid) case split.
    */
    PiecewiseLinearCaseSplit getValidCaseSplit() const override;

    /*
       Returns a list of all cases - { ABS_POSITIVE, ABS_NEGATIVE }
       The order of returned cases affects the search, and this method is where related
       heuristics should be implemented.
     */
    List<PhaseStatus> getAllCases() const override;

    /*
       Returns case split corresponding to the given phase/id
     */
    PiecewiseLinearCaseSplit getCaseSplit( PhaseStatus phase ) const override;

    /*
     * If the constraint's phase has been fixed, get the (valid) case split.
     */
    PiecewiseLinearCaseSplit getImpliedCaseSplit() const override;

    /*
      Check whether the constraint's phase has been fixed.
     */
    void fixPhaseIfNeeded();
    bool phaseFixed() const override;

    /*
      Preprocessing related functions, to inform that a variable has
      been eliminated completely because it was fixed to some value,
      or that a variable's index has changed (e.g., x4 is now called
      x2). constraintObsolete() returns true iff and the constraint
      has become obsolote as a result of variable eliminations.
     */
    void eliminateVariable( unsigned variable, double fixedValue ) override;
    void updateVariableIndex( unsigned oldIndex, unsigned newIndex ) override;
    bool constraintObsolete() const override;

    /*
      Get the tightenings entailed by the constraint.
    */
    void getEntailedTightenings( List<Tightening> &tightenings ) const override;


  virtual bool supportVariableElimination() const override
  {
    return false;
  }

    /*
      Dump the current state of the constraint.
    */
    void dump( String &output ) const override;

    /*
      For preprocessing: get any auxiliary equations that this constraint would
      like to add to the equation pool. This way, case splits will be bound
      update of the aux variables.
    */
    void transformToUseAuxVariables( InputQuery &inputQuery ) override;

    /*
      Whether the constraint can contribute the SoI cost function.
    */
    virtual inline bool supportSoI() const override
    {
        return true;
    }

    /*
      Returns string with shape: absoluteValue,_f,_b
     */
    String serializeToString() const override;

    inline unsigned getB() const { return _b; };

    inline unsigned getF() const { return _f; };

  inline unsigned getFloor() const { return _floor; };

  inline unsigned getCeiling() const { return _ceiling; };

private:
    unsigned _b, _f;
  double _floor, _ceiling;

    static String phaseToString( PhaseStatus phase );

    /*
      The two case splits.
    */
    PiecewiseLinearCaseSplit getPositiveSplit() const;
    PiecewiseLinearCaseSplit getNegativeSplit() const;

    /*
      Return true iff _b and _f are not both within bounds.
    */
    bool haveOutOfBoundVariables() const;
};

#endif // __ClipConstraint_h__
