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
    : PiecewiseLinearConstraint( 3 )
    , _b( b )
    , _f( f )
    , _floor( floor )
    , _ceiling( ceiling )
    , _auxVarInUse( false )
    , _haveEliminatedVariables( false )
    , _feasiblePhases( { CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING, CLIP_PHASE_BETWEEN } )
{

}

ClipConstraint::ClipConstraint( const String &serializedClip )
    : _haveEliminatedVariables( false )
    , _feasiblePhases( { CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING, CLIP_PHASE_BETWEEN } )
{
    String constraintType = serializedAbs.substring( 0, 4 );
    ASSERT( constraintType == String( "clip" ) );

    // Remove the constraint type in serialized form
    String serializedValues = serializedClip.substring( 5, serializedClip.length() - 5 );
    List<String> values = serializedValues.tokenize( "," );

    ASSERT( values.size() == 4 || values.size() == 5 );

    auto var = values.begin();
    _f = atoi( var->ascii() );
    ++var;
    _b = atoi( var->ascii() );
    ++var;
    _floor = atof( var->ascii() );
    ++var;
    _ceiling = atof( var->ascii() );

    if ( values.size() == 5 )
    {
        _aux = atoi( var->ascii() );
        _auxVarInUse = true;
    }
    else
    {
        _auxVarInUse
    }
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
    const ClipConstraint *clip = dynamic_cast<const ClipConstraint *>( state );

    CVC4::context::CDO<bool> *activeStatus = _cdConstraintActive;
    CVC4::context::CDO<PhaseStatus> *phaseStatus = _cdPhaseStatus;
    CVC4::context::CDList<PhaseStatus> *infeasibleCases = _cdInfeasibleCases;
    *this = *clip;
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

void ClipConstraint::checkIfLowerBoundUpdateFixesPhase( unsigned variable, double bound )
{
    if ( variable == _f )
    {
        if ( FloatUtils::gt( bound, _floor ) )
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
    }
    else if ( variable == _b )
    {
        if ( FloatUtils::gt( bound, _ceiling ) )
        {
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
            removeFeasiblePhase( CLIP_PHASE_BETWEEN );
        }
        else if ( FloatUtils::gt( bound, _floor ) )
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
    }
    else if ( _auxVarInUse && variable == _aux && FloatUtils::isPositive( bound ) )
        removeFeasiblePhase( CLIP_PHASE_BETWEEN );

    if ( _feasiblePhases.size() == 1 )
        setPhaseStatus( *_feasiblePhases.begin() );
}

void ReluConstraint::checkIfUpperBoundUpdateFixesPhase( unsigned variable, double bound )
{
    if ( variable == _f )
    {
        if ( FloatUtils::lt( bound, _ceiling ) )
            removeFeasiblePhase( CLIP_PHASE_CEILING );
    }
    else if ( variable == _b )
    {
        if ( FloatUtils::lt( bound, _floor ) )
        {
            removeFeasiblePhase( CLIP_PHASE_CEILING );
            removeFeasiblePhase( CLIP_PHASE_BETWEEN );
        }
        else if ( FloatUtils::lt( bound, _ceiling ) )
            removeFeasiblePhase( CLIP_PHASE_CEILING );
    }
    else if ( _auxVarInUse && variable == _aux )
    {
        if (  FloatUtils::isZero( bound ) )
        {
            removeFeasiblePhase( CLIP_PHASE_CEILING );
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
        }
        else if ( FloatUtils::isNegative( bound ) )
            removeFeasiblePhase( CLIP_PHASE_
    }
    if ( _feasiblePhases.size() == 1 )
        setPhaseStatus( *_feasiblePhases.begin() );
}

void ClipConstraint::notifyLowerBound( unsigned variable, double newBound )
{
    if ( _statistics )
        _statistics->incLongAttribute(
            Statistics::NUM_BOUND_NOTIFICATIONS_TO_PL_CONSTRAINTS );

    if ( _boundManager == nullptr )
    {
        if ( existsLowerBound( variable ) && !FloatUtils::gt( newBound, getLowerBound( variable ) ) )
            return;
        setLowerBound( variable, newBound );
        checkIfLowerBoundUpdateFixesPhase( variable, newBound );
    }
    else if ( !phaseFixed() )
    {
        double bound = getLowerBound( variable );
        checkIfLowerBoundUpdateFixesPhase( variable, bound );
        if ( ( variable == _f || variable == _b ) && bound > _ceiling && bound < _floor )
        {
            unsigned partner = ( variable == _f ) ? _b : _f;
            _boundManager->tightenLowerBound( partner, bound );
        }
        else if ( _auxVarInUse && _variable == _aux &&
    }

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
    return ( ( variable == _b ) || ( variable == _f ) || ( _auxVarInUse && variable == _aux ) );
}

List<unsigned> ClipConstraint::getParticipatingVariables() const
{
    if ( _auxVarInUse )
        return List<unsigned>( { _b, _f, _aux } );
    else
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
    return { CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING, CLIP_PHASE_BETWEEN, PHASE_NOT_FIXED };
}

PiecewiseLinearCaseSplit ClipConstraint::getCaseSplit( PhaseStatus phase ) const
{
    /*
      We added f - b = aux
      CLIP_PHASE_CEILING: aux <= 0, f = ceiling, b >= ceiling
      CLIP_PHASE_BETWEEN: aux = 0, b >= floor, b <= ceiling
      CLIP_PHASE_FLOOR: aux >= 0, f = floor, b <= floor
    */

    if ( phase == CLIP_PHASE_CEILING )
        return getCeilingSplit();
    else if ( phase == CLIP_PHASE_BETWEEN )
        return getBetweenSplit();
    else if ( phase == CLIP_PHASE_FLOOR )
        return getFloorSplit();
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

void ClipConstraint::transformToUseAuxVariables( InputQuery &inputQuery )
{
    /*
      We add f - b - aux = 0
    */
    if ( _auxVarInUse )
        return;

    // Create the aux variable
    _aux = inputQuery.getNumberOfVariables();
    inputQuery.setNumberOfVariables( _aux + 1 );

    // Create and add the equation
    Equation equation( Equation::EQ );
    equation.addAddend( 1.0, _f );
    equation.addAddend( -1.0, _b );
    equation.addAddend( -1.0, _aux );
    equation.setScalar( 0 );
    inputQuery.addEquation( equation );

    // We now care about the auxiliary variable, as well
    _auxVarInUse = true;
}

bool ClipConstraint::haveOutOfBoundVariables() const
{
  return true;
}

String ClipConstraint::serializeToString() const
{
  // Output format is: Clip,f,b,floor,ceiling
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
}

String ClipConstraint::removeFeasiblePhase( PhaseStatus phase )
{
    if ( _feasiblePhases.exists( phase ) )
        _feasiblePhases.erase( phase );
}
