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
    , _feasiblePhases( { CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING, CLIP_PHASE_MIDDLE } )
{
    if ( floor > ceiling )
        throw MarabouError( MarabouError::INVALID_PIECEWISE_LINEAR_CONSTRAINT,
                            "Floor cannot be larger than ceiling in the ClipConstraint!" );
}

ClipConstraint::ClipConstraint( const String &serializedClip )
    : _haveEliminatedVariables( false )
    , _feasiblePhases( { CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING, CLIP_PHASE_MIDDLE } )
{
    String constraintType = serializedClip.substring( 0, 4 );
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

    if ( _floor > _ceiling )
        throw MarabouError( MarabouError::INVALID_PIECEWISE_LINEAR_CONSTRAINT,
                            "Floor cannot be larger than ceiling in the ClipConstraint!" );

    if ( values.size() == 5 )
    {
        ++var;
        _aux = atoi( var->ascii() );
        _auxVarInUse = true;
    }
    else
    {
        _auxVarInUse = false;
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

void ClipConstraint::updateFeasiblePhaseWithLowerBound( unsigned variable, double bound )
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
            removeFeasiblePhase( CLIP_PHASE_MIDDLE );
        }
        else if ( FloatUtils::gt( bound, _floor ) )
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
    }
    else if ( _auxVarInUse && variable == _aux )
    {
        if ( FloatUtils::isPositive( bound ) )
        {
            // aux = f - b is positive, therefore we must be in CLIP_PHASE_FLOOR
            removeFeasiblePhase( CLIP_PHASE_MIDDLE );
            removeFeasiblePhase( CLIP_PHASE_CEILING );
        }
        else if ( FloatUtils::isZero( bound ) )
        {
            removeFeasiblePhase( CLIP_PHASE_CEILING );
        }
    }
    if ( _feasiblePhases.size() == 1 )
        setPhaseStatus( *_feasiblePhases.begin() );
}

void ClipConstraint::updateFeasiblePhaseWithUpperBound( unsigned variable, double bound )
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
            removeFeasiblePhase( CLIP_PHASE_MIDDLE );
        }
        else if ( FloatUtils::lt( bound, _ceiling ) )
            removeFeasiblePhase( CLIP_PHASE_CEILING );
    }
    else if ( _auxVarInUse && variable == _aux )
    {
        if ( FloatUtils::isNegative( bound ) )
        {
            // aux = f - b is negative, therefore we must be in CLIP_PHASE_CEILING
            removeFeasiblePhase( CLIP_PHASE_MIDDLE );
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
        }
        else if ( FloatUtils::isZero( bound ) )
        {
            removeFeasiblePhase( CLIP_PHASE_FLOOR );
        }
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
        updateFeasiblePhaseWithLowerBound( variable, newBound );
    }
    else if ( !phaseFixed() )
    {
        double bound = getLowerBound( variable );
        updateFeasiblePhaseWithLowerBound( variable, bound );
        if ( ( variable == _f || variable == _b ) &&
             FloatUtils::gte( bound, _floor ) &&
             FloatUtils::lte( bound, _floor ) )
        {
            // A lower bound between floor and ceiling is propagated between f and b
            unsigned partner = ( variable == _f ) ? _b : _f;
            _boundManager->tightenLowerBound( partner, bound );
        }
        else if ( variable == _b && FloatUtils::gte( bound, _ceiling ) )
        {
            // We must be in the ceiling phase
            _boundManager->tightenLowerBound( _f, _ceiling );
            _boundManager->tightenUpperBound( _aux, 0 );
        }
        else if ( variable == _aux && FloatUtils::isPositive( bound ) )
        {
            // We must be in the floor phase
            _boundManager->tightenUpperBound( _b, _floor );
            _boundManager->tightenUpperBound( _f, _floor );
        }
    }
}

void ClipConstraint::notifyUpperBound( unsigned variable, double newBound )
{
    if ( _statistics )
        _statistics->incLongAttribute( Statistics::NUM_BOUND_NOTIFICATIONS_TO_PL_CONSTRAINTS );

    if ( _boundManager == nullptr )
    {
        if ( existsLowerBound( variable ) && !FloatUtils::lt( newBound, getUpperBound( variable ) ) )
            return;
        setUpperBound( variable, newBound );
        updateFeasiblePhaseWithUpperBound( variable, newBound );
    }
    else if ( !phaseFixed() )
    {
        double bound = getUpperBound( variable );
        updateFeasiblePhaseWithUpperBound( variable, bound );
        if ( ( variable == _f || variable == _b ) &&
             FloatUtils::gte( bound, _floor ) &&
             FloatUtils::lte( bound, _floor ) )
        {
            // An upper bound between floor and ceiling is propagated between f and b
            unsigned partner = ( variable == _f ) ? _b : _f;
            _boundManager->tightenUpperBound( partner, bound );
        }
        else if ( variable == _b && FloatUtils::lte( bound, _floor ) )
        {
            // We must be in the floor phase
            _boundManager->tightenUpperBound( _f, _floor );
            _boundManager->tightenLowerBound( _aux, 0 );
        }
        else if ( variable == _aux && FloatUtils::isNegative( bound ) )
        {
            // We must be in the ceiling phase
            _boundManager->tightenLowerBound( _b, _ceiling );
            _boundManager->tightenLowerBound( _f, _ceiling );
        }
    }
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
    if ( !( existsAssignment( _b ) && existsAssignment( _f ) ) )
        throw MarabouError( MarabouError::PARTICIPATING_VARIABLE_MISSING_ASSIGNMENT );

    double bValue = getAssignment( _b );
    double fValue = getAssignment( _f );

    if ( FloatUtils::lte( bValue, _floor ) )
        return FloatUtils::areEqual( fValue, _floor, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE );
    else if ( FloatUtils::gte( bValue, _ceiling ) )
        return FloatUtils::areEqual( fValue, _ceiling, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE );
    else
        return FloatUtils::areEqual( bValue, fValue, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE );
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
    if ( _phaseStatus != PHASE_NOT_FIXED )
        throw MarabouError( MarabouError::REQUESTED_CASE_SPLITS_FROM_FIXED_CONSTRAINT );

    List<PiecewiseLinearCaseSplit> splits;

    // If we have existing knowledge about the assignment, use it to
    // influence the order of splits
    if ( existsAssignment( _b ) )
    {
        double bValue = getAssignment( _b );
        if ( FloatUtils::gte( bValue, _ceiling ) )
        {
            // Current assignment in the ceiling phase
            splits.append( getCeilingSplit() );
            splits.append( getMiddleSplit() );
            if ( _feasiblePhases.exists( CLIP_PHASE_FLOOR ) )
                splits.append( getFloorSplit() );
        }
        else if ( FloatUtils::lte( bValue, _floor ) )
        {
            // Current assignment in the floor phase
            splits.append( getFloorSplit() );
            splits.append( getMiddleSplit() );
            if ( _feasiblePhases.exists( CLIP_PHASE_CEILING ) )
                splits.append( getCeilingSplit() );
        }
        else
        {
            splits.append( getMiddleSplit() );
            if ( bValue - _floor < _ceiling - bValue )
            {
                // Current assignment closer to the floor
                if ( _feasiblePhases.exists( CLIP_PHASE_FLOOR ) )
                    splits.append( getFloorSplit() );
                if ( _feasiblePhases.exists( CLIP_PHASE_CEILING ) )
                    splits.append( getCeilingSplit() );
            }
            else
            {
                if ( _feasiblePhases.exists( CLIP_PHASE_CEILING ) )
                    splits.append( getCeilingSplit() );
                if ( _feasiblePhases.exists( CLIP_PHASE_FLOOR ) )
                    splits.append( getFloorSplit() );
            }
        }
    }
    else
    {
        if ( _feasiblePhases.exists( CLIP_PHASE_MIDDLE ) )
            splits.append( getMiddleSplit() );
        if ( _feasiblePhases.exists( CLIP_PHASE_CEILING ) )
            splits.append( getCeilingSplit() );
        if ( _feasiblePhases.exists( CLIP_PHASE_FLOOR ) )
            splits.append( getFloorSplit() );
    }
    return splits;
}

List<PhaseStatus> ClipConstraint::getAllCases() const
{
    return { CLIP_PHASE_MIDDLE, CLIP_PHASE_FLOOR, CLIP_PHASE_CEILING };
}

PiecewiseLinearCaseSplit ClipConstraint::getCaseSplit( PhaseStatus phase ) const
{
    if ( phase == CLIP_PHASE_CEILING )
        return getCeilingSplit();
    else if ( phase == CLIP_PHASE_MIDDLE )
        return getMiddleSplit();
    else if ( phase == CLIP_PHASE_FLOOR )
        return getFloorSplit();
    else
        throw MarabouError( MarabouError::REQUESTED_NONEXISTENT_CASE_SPLIT );
}

PiecewiseLinearCaseSplit ClipConstraint::getCeilingSplit() const
{
    ASSERT( _auxVarInUse );
    PiecewiseLinearCaseSplit split;
    split.storeBoundTightening( Tightening( _b, _ceiling, Tightening::LB ) );
    split.storeBoundTightening( Tightening( _f, _ceiling, Tightening::LB ) );
    // aux = f - b <= 0
    split.storeBoundTightening( Tightening( _aux, 0, Tightening::UB ) );
    return split;
}

PiecewiseLinearCaseSplit ClipConstraint::getFloorSplit() const
{
    ASSERT( _auxVarInUse );
    PiecewiseLinearCaseSplit split;
    split.storeBoundTightening( Tightening( _b, _floor, Tightening::UB ) );
    split.storeBoundTightening( Tightening( _f, _floor, Tightening::UB ) );
    // aux = f - b >= 0
    split.storeBoundTightening( Tightening( _aux, 0, Tightening::LB ) );
    return split;
}

PiecewiseLinearCaseSplit ClipConstraint::getMiddleSplit() const
{
    ASSERT( _auxVarInUse );
    PiecewiseLinearCaseSplit split;
    split.storeBoundTightening( Tightening( _b, _floor, Tightening::LB ) );
    split.storeBoundTightening( Tightening( _b, _ceiling, Tightening::UB ) );
    // aux = f - b = 0
    split.storeBoundTightening( Tightening( _aux, 0, Tightening::LB ) );
    split.storeBoundTightening( Tightening( _aux, 0, Tightening::UB ) );
    return split;
}

bool ClipConstraint::phaseFixed() const
{
  return _phaseStatus != PHASE_NOT_FIXED;
}

PiecewiseLinearCaseSplit ClipConstraint::getImpliedCaseSplit() const
{
    ASSERT( phaseFixed() );
    PhaseStatus phase = getPhaseStatus();
    ASSERT( phase == CLIP_PHASE_FLOOR ||
            phase == CLIP_PHASE_MIDDLE ||
            phase == CLIP_PHASE_CEILING );
    return getCaseSplit( phase );
}

PiecewiseLinearCaseSplit ClipConstraint::getValidCaseSplit() const
{
    return getImpliedCaseSplit();
}

void ClipConstraint::dump( String &output ) const
{
    output = Stringf( "ClipConstraint: x%u = Clip( x%u, %.2f, %.2f ). Active? %s. PhaseStatus = %u (%s).\n",
                      _f, _b, _floor, _ceiling,
                      _constraintActive ? "Yes" : "No",
                      _phaseStatus, phaseToString( _phaseStatus ).ascii()
        );

    output += Stringf( "b in [%s, %s], ",
                       existsLowerBound( _b ) ? Stringf( "%lf", getLowerBound( _b ) ).ascii() : "-inf",
                       existsUpperBound( _b ) ? Stringf( "%lf", getUpperBound( _b ) ).ascii() : "inf" );

    output += Stringf( "f in [%s, %s]",
                       existsLowerBound( _f ) ? Stringf( "%lf", getLowerBound( _f ) ).ascii() : "-inf",
                       existsUpperBound( _f ) ? Stringf( "%lf", getUpperBound( _f ) ).ascii() : "inf" );

    if ( _auxVarInUse )
    {
        output += Stringf( ". Aux var: %u. Range: [%s, %s]\n",
                           _aux,
                           existsLowerBound( _aux ) ? Stringf( "%lf", getLowerBound( _aux ) ).ascii() : "-inf",
                           existsUpperBound( _aux ) ? Stringf( "%lf", getUpperBound( _aux ) ).ascii() : "inf" );
    }
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

    case CLIP_PHASE_MIDDLE:
        return "CLIP_PHASE_MIDDLE";

    default:
        return "UNKNOWN";
    }
};

void ClipConstraint::updateVariableIndex( unsigned oldIndex, unsigned newIndex )
{
    // Variable reindexing can only occur in preprocessing before Gurobi is
    // registered.
    ASSERT( _gurobi == NULL );

    ASSERT( oldIndex == _b || oldIndex == _f || ( _auxVarInUse && oldIndex == _aux ) );
    ASSERT( !_lowerBounds.exists( newIndex ) &&
            !_upperBounds.exists( newIndex ) &&
            newIndex != _b && newIndex != _f && ( !_auxVarInUse || newIndex != _aux ) );

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
    else
        _aux = newIndex;
}

void ClipConstraint::eliminateVariable( __attribute__((unused)) unsigned variable,
                                        __attribute__((unused)) double fixedValue )
{
    ASSERT( variable == _b || variable == _f || ( _auxVarInUse && variable == _aux ) );

    DEBUG({
            if ( variable == _f )
            {
                ASSERT( FloatUtils::gte( fixedValue, _floor ) );
                ASSERT( FloatUtils::lte( fixedValue, _ceiling ) );
            }

            if ( variable == _b )
            {
                if ( FloatUtils::lt( fixedValue, _floor ) )
                {
                    ASSERT( _phaseStatus != CLIP_PHASE_CEILING &&
                            _phaseStatus != CLIP_PHASE_MIDDLE );
                }
                else if ( FloatUtils::gt( fixedValue, _ceiling ) )
                {
                    ASSERT( _phaseStatus != CLIP_PHASE_FLOOR &&
                            _phaseStatus != CLIP_PHASE_MIDDLE );
                }
            }
            else
            {
                // This is the aux variable
                if ( FloatUtils::isPositive( fixedValue ) )
                {
                    // aux = f - b >= 0
                    ASSERT( _phaseStatus != CLIP_PHASE_CEILING );
                }
                else if ( FloatUtils::isNegative( fixedValue ) )
                {
                    ASSERT( _phaseStatus != CLIP_PHASE_FLOOR );
                }
            }
        });

    // In a Clip constraint, if a variable is removed the entire constraint can be discarded.
    _haveEliminatedVariables = true;
}

bool ClipConstraint::constraintObsolete() const
{
    return false;
}

void ClipConstraint::getEntailedTightenings( List<Tightening> &tightenings ) const
{
    ASSERT( existsLowerBound( _b ) && existsLowerBound( _f ) &&
            existsUpperBound( _b ) && existsUpperBound( _f ) );

    ASSERT( !_auxVarInUse || ( existsLowerBound( _aux ) && existsUpperBound( _aux ) ) );

    double bLowerBound = getLowerBound( _b );
    double fLowerBound = getLowerBound( _f );

    double bUpperBound = getUpperBound( _b );
    double fUpperBound = getUpperBound( _f );

    double auxLowerBound = 0;
    double auxUpperBound = 0;

    if ( _auxVarInUse )
    {
        auxLowerBound = getLowerBound( _aux );
        auxUpperBound = getUpperBound( _aux );
    }

    // It is important to ensure in this method that when the phase status is
    // fixed, bounds are added so that the Clip constriant can be soundly removed.
    if ( FloatUtils::lte( bUpperBound, _floor ) ||
         FloatUtils::areEqual( fUpperBound, _floor ) ||
         ( _auxVarInUse && FloatUtils::isPositive( auxLowerBound ) ) )
    {
        // Floor case
        tightenings.append( Tightening( _b, bLowerBound, Tightening::LB ) );
        tightenings.append( Tightening( _f, _floor, Tightening::LB ) );

        tightenings.append( Tightening( _b, _floor, Tightening::UB ) );
        tightenings.append( Tightening( _f, _floor, Tightening::UB ) );

        // Aux is positive
        if ( _auxVarInUse )
        {
            tightenings.append( Tightening( _aux, 0, Tightening::LB ) );
            tightenings.append( Tightening( _aux, fUpperBound - bLowerBound, Tightening::UB ) );
            tightenings.append( Tightening( _aux, auxLowerBound, Tightening::LB ) );
            tightenings.append( Tightening( _aux, auxUpperBound, Tightening::UB ) );
        }
    }
    else if ( ( FloatUtils::gte( bLowerBound, _floor ) &&
                FloatUtils::lte( bUpperBound, _ceiling ) ) ||
              ( FloatUtils::gt( fLowerBound, _floor ) &&
                FloatUtils::lt( fUpperBound, _ceiling ) ) ||
              ( _auxVarInUse &&
                FloatUtils::isZero( auxLowerBound ) &&
                FloatUtils::isZero( auxUpperBound ) ) )
    {
        // Middle case
        // All bounds are propagated between b and f
        tightenings.append( Tightening( _b, fLowerBound, Tightening::LB ) );
        tightenings.append( Tightening( _f, bLowerBound, Tightening::LB ) );

        tightenings.append( Tightening( _b, fUpperBound, Tightening::UB ) );
        tightenings.append( Tightening( _f, bUpperBound, Tightening::UB ) );

        // Aux is zero
        if ( _auxVarInUse )
        {
            tightenings.append( Tightening( _aux, 0, Tightening::UB ) );
            tightenings.append( Tightening( _aux, 0, Tightening::LB ) );
        }
    }
    else if ( FloatUtils::gte( bLowerBound, _ceiling ) ||
              FloatUtils::areEqual( fLowerBound, _ceiling ) ||
              ( _auxVarInUse && FloatUtils::isNegative( auxUpperBound ) ) )
    {
        // Ceiling case
        tightenings.append( Tightening( _b, _ceiling, Tightening::LB ) );
        tightenings.append( Tightening( _f, _ceiling, Tightening::LB ) );

        tightenings.append( Tightening( _b, bUpperBound, Tightening::UB ) );
        tightenings.append( Tightening( _f, _ceiling, Tightening::UB ) );

        // Aux is negative
        if ( _auxVarInUse )
        {
            tightenings.append( Tightening( _aux, fLowerBound - bUpperBound, Tightening::LB ) );
            tightenings.append( Tightening( _aux, 0, Tightening::UB ) );
            tightenings.append( Tightening( _aux, auxLowerBound, Tightening::LB ) );
            tightenings.append( Tightening( _aux, auxUpperBound, Tightening::UB ) );
        }
    }
    else
    {
        tightenings.append( Tightening( _b, bLowerBound, Tightening::LB ) );
        tightenings.append( Tightening( _b, bUpperBound, Tightening::UB ) );
        tightenings.append( Tightening( _f, fLowerBound, Tightening::LB ) );
        tightenings.append( Tightening( _f, fUpperBound, Tightening::UB ) );
        tightenings.append( Tightening( _f, _floor, Tightening::LB ) );
        tightenings.append( Tightening( _f, _ceiling, Tightening::UB ) );
        if ( _auxVarInUse )
        {
            tightenings.append( Tightening( _aux, fLowerBound - bUpperBound, Tightening::LB ) );
            tightenings.append( Tightening( _aux, fUpperBound - bLowerBound, Tightening::UB ) );
            tightenings.append( Tightening( _aux, auxLowerBound, Tightening::LB ) );
            tightenings.append( Tightening( _aux, auxUpperBound, Tightening::UB ) );
        }
    }
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

void ClipConstraint::getCostFunctionComponent( LinearExpression &cost,
                                               PhaseStatus phase ) const
{
    // If the constraint is not active or is fixed, it contributes nothing
    if( !isActive() || phaseFixed() )
        return;

    // This should not be called when the linear constraints have
    // not been satisfied
    ASSERT( !haveOutOfBoundVariables() );

    ASSERT( phase == CLIP_PHASE_FLOOR || phase == CLIP_PHASE_CEILING ||
            phase == CLIP_PHASE_MIDDLE );

    if ( phase == CLIP_PHASE_FLOOR )
    {
        // The cost term corresponding to the floor phase is f - floor,
        // since the Clip is in the floor phase and satisfied only if f - floor is 0 and minimal.
        if ( !cost._addends.exists( _f ) )
            cost._addends[_f] = 0;
        cost._addends[_f] = cost._addends[_f] + 1;
        cost._constant -= _floor;
    }
    else if ( phase == CLIP_PHASE_CEILING )
    {
        // The cost term corresponding to the floor phase is ceiling - f,
        // since the Clip is in the ceiling phase and satisfied only if ceiling - f is 0 and minimal.
        if ( !cost._addends.exists( _f ) )
            cost._addends[_f] = 0;
        cost._addends[_f] = cost._addends[_f] - 1;
        cost._constant += _ceiling;
    }
    else
    {
        // We cannot find a cost function such that it is 0 when the constraint is satisfied
    }
}

PhaseStatus ClipConstraint::getPhaseStatusInAssignment( const Map<unsigned, double>
                                                        &assignment ) const
{
    ASSERT( assignment.exists( _b ) );
    double bAssignment = assignment[_b];
    if ( FloatUtils::lte( bAssignment, _floor ) )
        return CLIP_PHASE_FLOOR;
    else if ( FloatUtils::gte( bAssignment, _ceiling ) )
        return CLIP_PHASE_CEILING;
    else
        return CLIP_PHASE_MIDDLE;
}

bool ClipConstraint::haveOutOfBoundVariables() const
{
    double bValue = getAssignment( _b );
    double fValue = getAssignment( _f );

    if ( FloatUtils::gt( getLowerBound( _b ), bValue, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE )
         || FloatUtils::lt( getUpperBound( _b ), bValue, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE ) )
        return true;

    if ( FloatUtils::gt( getLowerBound( _f ), fValue, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE )
         || FloatUtils::lt( getUpperBound( _f ), fValue, GlobalConfiguration::CONSTRAINT_COMPARISON_TOLERANCE ) )
        return true;

    return false;
}

unsigned ClipConstraint::getB() const
{
    return _b;
}

unsigned ClipConstraint::getF() const
{
    return _f;
}

double ClipConstraint::getFloor() const
{
    return _floor;
}

double ClipConstraint::getCeiling() const
{
    return _ceiling;
}

bool ClipConstraint::supportPolarity() const
{
    return false;
}

bool ClipConstraint::auxVariableInUse() const
{
    return _auxVarInUse;
}

unsigned ClipConstraint::getAux() const
{
    return _aux;
}

String ClipConstraint::serializeToString() const
{
  // Output format is: Clip,f,b,floor,ceiling
    if ( _auxVarInUse )
        return Stringf( "clip,%u,%u,%.8f,%.8f,%u", _f, _b, _floor, _ceiling, _aux );
    else
        return Stringf( "clip,%u,%u,%.8f,%.8f", _f, _b, _floor, _ceiling );
}

void ClipConstraint::removeFeasiblePhase( PhaseStatus phase )
{
    if ( _feasiblePhases.exists( phase ) )
        _feasiblePhases.erase( phase );
}
