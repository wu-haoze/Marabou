/*********************                                                        */
/*! \file ClipConstraint.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** See the description of the class in ClipConstraint.h.
 **/

#include "ClipConstraint.h"

#include "Debug.h"
#include "FloatUtils.h"
#include "ITableau.h"
#include "InputQuery.h"
#include "MStringf.h"
#include "MarabouError.h"
#include "PiecewiseLinearCaseSplit.h"
#include "Statistics.h"

ClipConstraint::ClipConstraint( unsigned b, unsigned f, double floor, double ceiling )
    : PiecewiseLinearConstraint( TWO_PHASE_PIECEWISE_LINEAR_CONSTRAINT )
    , _b( b )
    , _f( f )
    , _floor( floor )
    , _ceiling( ceiling )
{
}

ClipConstraint::ClipConstraint( const String &/*serializedAbs*/ )
{
  throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

PiecewiseLinearFunctionType ClipConstraint::getType() const
{
    return PiecewiseLinearFunctionType::CLIP;
}

PiecewiseLinearConstraint *ClipConstraint::duplicateConstraint() const
{
  ClipConstraint *clone = new ClipConstraint( _b, _f, _floor, _ceiling );
    *clone = *this;
    this->initializeDuplicateCDOs( clone );
    return clone;
}

void ClipConstraint::restoreState( const PiecewiseLinearConstraint *state )
{
    const ClipConstraint *abs = dynamic_cast<const ClipConstraint *>( state );

    CVC4::context::CDO<bool> *activeStatus = _cdConstraintActive;
    CVC4::context::CDO<PhaseStatus> *phaseStatus = _cdPhaseStatus;
    CVC4::context::CDList<PhaseStatus> *infeasibleCases = _cdInfeasibleCases;
    *this = *abs;
    _cdConstraintActive = activeStatus;
    _cdPhaseStatus = phaseStatus;
    _cdInfeasibleCases = infeasibleCases;
}

void ClipConstraint::registerAsWatcher( ITableau *tableau )
{
    tableau->registerToWatchVariable( this, _b );
    tableau->registerToWatchVariable( this, _f );
}

void ClipConstraint::unregisterAsWatcher( ITableau *tableau )
{
    tableau->unregisterToWatchVariable( this, _b );
    tableau->unregisterToWatchVariable( this, _f );
}

void ClipConstraint::notifyLowerBound( unsigned variable, double bound )
{
    if ( _statistics )
        _statistics->incLongAttribute(
            Statistics::NUM_BOUND_NOTIFICATIONS_TO_PL_CONSTRAINTS );

    if ( _boundManager == nullptr && existsLowerBound( variable ) &&
         !FloatUtils::gt( bound, getLowerBound( variable ) ) )
      return;

    setLowerBound( variable, bound );
}

void ClipConstraint::notifyUpperBound( unsigned variable, double bound )
{
    if ( _statistics )
        _statistics->incLongAttribute( Statistics::NUM_BOUND_NOTIFICATIONS_TO_PL_CONSTRAINTS );

    if ( _boundManager == nullptr && existsUpperBound( variable ) &&
         !FloatUtils::lt( bound, getUpperBound( variable ) ) )
        return;

     setUpperBound( variable, bound );
}

bool ClipConstraint::participatingVariable( unsigned variable ) const
{
    return ( variable == _b ) || ( variable == _f );
}

List<unsigned> ClipConstraint::getParticipatingVariables() const
{
  return List<unsigned>( { _b, _f } );
}

bool ClipConstraint::satisfied() const
{
  return false;
}

List<PiecewiseLinearConstraint::Fix> ClipConstraint::getPossibleFixes() const
{
  return List<PiecewiseLinearConstraint::Fix>();
}

List<PiecewiseLinearConstraint::Fix> ClipConstraint::getSmartFixes( ITableau */* tableau */ ) const
{
    return getPossibleFixes();
}

List<PiecewiseLinearCaseSplit> ClipConstraint::getCaseSplits() const
{
    List<PiecewiseLinearCaseSplit> splits;
    return splits;
}

List<PhaseStatus> ClipConstraint::getAllCases() const
{
  return { CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING, PHASE_NOT_FIXED };
}

PiecewiseLinearCaseSplit ClipConstraint::getCaseSplit( PhaseStatus phase ) const
{
    if ( phase == ABS_PHASE_NEGATIVE )
        return getNegativeSplit();
    else if ( phase == ABS_PHASE_POSITIVE )
        return getPositiveSplit();
    else
        throw MarabouError( MarabouError::REQUESTED_NONEXISTENT_CASE_SPLIT );
}

PiecewiseLinearCaseSplit ClipConstraint::getNegativeSplit() const
{
  throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

PiecewiseLinearCaseSplit ClipConstraint::getPositiveSplit() const
{
  throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

bool ClipConstraint::phaseFixed() const
{
  return false;
}

PiecewiseLinearCaseSplit ClipConstraint::getImpliedCaseSplit() const
{
  throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

PiecewiseLinearCaseSplit ClipConstraint::getValidCaseSplit() const
{
    return getImpliedCaseSplit();
}

void ClipConstraint::eliminateVariable( unsigned /*variable*/, double /* fixedValue */ )
{
  throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

void ClipConstraint::dump( String &/*output*/ ) const
{
  throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

void ClipConstraint::updateVariableIndex( unsigned oldIndex, unsigned newIndex )
{
    // Variable reindexing can only occur in preprocessing before Gurobi is
    // registered.
    ASSERT( _gurobi == NULL );

    ASSERT( oldIndex == _b || oldIndex == _f );

    ASSERT( !existsLowerBound( newIndex ) &&
            !existsUpperBound( newIndex ) &&
            newIndex != _b && newIndex != _f );

    if ( existsLowerBound( oldIndex ) )
    {
        _lowerBounds[newIndex] = _lowerBounds.get( oldIndex );
        _lowerBounds.erase( oldIndex );
    }

    if ( existsUpperBound( oldIndex ) )
    {
        _upperBounds[newIndex] = _upperBounds.get( oldIndex );
        _upperBounds.erase( oldIndex );
    }

    if ( oldIndex == _b )
        _b = newIndex;
    else if ( oldIndex == _f )
        _f = newIndex;
}

bool ClipConstraint::constraintObsolete() const
{
    return false;
}

void ClipConstraint::getEntailedTightenings( List<Tightening> &tightenings ) const
{
  if ( _lowerBounds.exists(_b) )
    tightenings.append(Tightening( _b, getLowerBound(_b), Tightening::LB) );
  if ( _upperBounds.exists(_b) )
    tightenings.append(Tightening( _b, getUpperBound(_b), Tightening::UB) );

  if ( _lowerBounds.exists(_f) )
    tightenings.append(Tightening( _f, getLowerBound(_f), Tightening::LB) );
  if ( _upperBounds.exists(_f) )
    tightenings.append(Tightening( _f, getUpperBound(_f), Tightening::UB) );

  tightenings.append(Tightening( _f, _floor, Tightening::LB) );
  tightenings.append(Tightening( _f, _ceiling, Tightening::UB) );
}

void ClipConstraint::transformToUseAuxVariables( InputQuery &/*inputQuery*/ )
{

}

bool ClipConstraint::haveOutOfBoundVariables() const
{
  return true;
}

String ClipConstraint::serializeToString() const
{
  // Output format is: Abs,f,b,posAux,NegAux
  return Stringf( "clip,%u,%u,%.8f,%.8f", _f, _b, _floor, _ceiling );
}

void ClipConstraint::fixPhaseIfNeeded()
{
  return;
}

String ClipConstraint::phaseToString( PhaseStatus phase )
{
    switch ( phase )
    {
    case PHASE_NOT_FIXED:
        return "PHASE_NOT_FIXED";

    case CLIP_PHASE_FLOOR:
      return "CLIP_PHASE_FLOOR";

    case CLIP_PHASE_CEILING:
        return "CLIP_PHASE_CEILING";

    default:
        return "UNKNOWN";
    }
};
