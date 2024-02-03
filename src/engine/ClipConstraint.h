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
 **
 ** ClipConstraint implements the following constraint:
 ** f = Clip( b, floor, ceiling ) = ( b <= floor -> f = floor )
 **                              /\ ( b >= ceiling -> f = ceiling )
 **                              /\ ( othewise -> f = b )
 **
 ** It distinguishes three relevant phases for search:
 ** CLIP_PHASE_FLOOR
 ** CLIP_PHASE_CEILING
 ** CLIP_PHASE_MIDDLE
 **
 **/

#ifndef __ClipConstraint_h__
#define __ClipConstraint_h__

#include "PiecewiseLinearConstraint.h"

class ClipConstraint : public PiecewiseLinearConstraint
{
public:
    /*
      The f variable is the clip output on the b variable:
      f = clip( b, floor, ceiling )
    */
    ClipConstraint( unsigned b, unsigned f, double floor, double ceiling );
    ClipConstraint( const String &serializedClip );

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
    void registerAsWatcher( ITableau *tableau ) override;
    void unregisterAsWatcher( ITableau *tableau ) override;

    /*
      These callbacks are invoked when a watched variable's value
      changes, or when its bounds change.
    */
    void notifyLowerBound( unsigned variable, double bound ) override;
    void notifyUpperBound( unsigned variable, double bound ) override;

    /*
      Returns true iff the variable participates in this piecewise
      linear constraint
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
    */
    List<PiecewiseLinearConstraint::Fix> getSmartFixes( ITableau *tableau ) const override;

    List<PiecewiseLinearCaseSplit> getCaseSplits() const override;

    /*
      If the constraint's phase has been fixed, get the (valid) case split.
    */
    PiecewiseLinearCaseSplit getValidCaseSplit() const override;

    List<PhaseStatus> getAllCases() const override;

    /*
       Returns case split corresponding to the given phase/id
     */
    PiecewiseLinearCaseSplit getCaseSplit( PhaseStatus phase ) const override;

    /*
      If the constraint's phase has been fixed, get the (valid) case split.
    */
    PiecewiseLinearCaseSplit getImpliedCaseSplit() const override;

    /*
      Check if the constraint's phase has been fixed.
    */
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

    /*
      Dump the current state of the constraint.
    */
    void dump( String &output ) const override;

    /*
      For preprocessing: get any auxiliary equations that this
      constraint would like to add to the equation pool. In the Clip
      case, this is an equation of the form aux = f - b.
      This way, case splits will be bound update of the aux variables.
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
      Ask the piecewise linear constraint to add its cost term corresponding to
      the given phase to the cost function. The cost term for Clip is:
        _f - floor      for the floor phase
        ceiling - _f    for the ceiling phase
        undefined       for the middle phase
    */
    virtual void getCostFunctionComponent( LinearExpression &cost,
                                           PhaseStatus phase ) const override;

    /*
      Return the phase status corresponding to the values of the *input*
      variables in the given assignment.
    */
    virtual PhaseStatus
    getPhaseStatusInAssignment( const Map<unsigned, double> &assignment ) const override;

    /*
      For serialization into the input query file
    */
    String serializeToString() const override;

    /*
      Get the index of the B and F variables.
    */
    unsigned getB() const;
    unsigned getF() const;

    double getFloor() const;
    double getCeiling() const;

    /*
      Check if the aux variable is in use and retrieve it
    */
    bool auxVariableInUse() const;
    unsigned getAux() const;

    bool supportPolarity() const override;

private:
    unsigned _b, _f;
    double _floor, _ceiling;
    bool _auxVarInUse;
    unsigned _aux;
    bool _haveEliminatedVariables;

    List<PhaseStatus> _feasiblePhases;

    PiecewiseLinearCaseSplit getFloorSplit() const;
    PiecewiseLinearCaseSplit getMiddleSplit() const;
    PiecewiseLinearCaseSplit getCeilingSplit() const;

    static String phaseToString( PhaseStatus phase );

    /*
      Return true iff b or f are out of bounds.
    */
    bool haveOutOfBoundVariables() const;

    /*
      Update the feasible phases based on the given bound
    */
    void updateFeasiblePhaseWithLowerBound( unsigned variable, double bound );
    void updateFeasiblePhaseWithUpperBound( unsigned variable, double bound );

    /*
      Mark that the phase is infeasible
    */
    void removeFeasiblePhase( PhaseStatus phase );
};

#endif // __ClipConstraint_h__
