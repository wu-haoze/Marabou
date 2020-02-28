/*********************                                                        */
/*! \file IntervalConstraint.cpp
** \verbatim
** Top contributors (to current version):
**   Haoze Wu
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** [[ Add lengthier description here ]]
**/

#include "DivideStrategy.h"
#include "GlobalConfiguration.h"
#include "IntervalConstraint.h"
#include "Debug.h"
#include "FloatUtils.h"
#include "MStringf.h"
#include "MarabouError.h"
#include "Statistics.h"

IntervalConstraint::IntervalConstraint( unsigned var, double lowerBound,
                                        double upperBound )
    : _var( var )
    , _lowerBound( lowerBound )
    , _upperBound( upperBound )
{
}

PiecewiseLinearConstraint *IntervalConstraint::duplicateConstraint() const
{
    PiecewiseLinearConstraint *clone = new IntervalConstraint( _var, _lowerBound,
                                                               _upperBound );
    return clone;
}

void IntervalConstraint::restoreState( const PiecewiseLinearConstraint * state )
{
    const IntervalConstraint *bound = dynamic_cast<const IntervalConstraint*>( state );
    *this = *bound;
}

void IntervalConstraint::registerAsWatcher( ITableau * /* tableau */ )
{
}

void IntervalConstraint::unregisterAsWatcher( ITableau * /* tableau */ )
{
}

void IntervalConstraint::notifyVariableValue( unsigned /* variable */,
                                           double /* value */ )
{
}

void IntervalConstraint::notifyLowerBound( unsigned variable, double bound )
{
    if ( _var == variable )
    {
        if ( _statistics )
            _statistics->incNumBoundNotificationsPlConstraints();

        if ( FloatUtils::gt( bound, _lowerBound ) )
            _lowerBound = bound;
    }
}

void IntervalConstraint::notifyUpperBound( unsigned variable, double bound )
{
    if ( _var == variable )
    {

        if ( _statistics )
            _statistics->incNumBoundNotificationsPlConstraints();

        if ( FloatUtils::lt( bound, _upperBound ) )
            _upperBound = bound;
    }
}

bool IntervalConstraint::participatingVariable( unsigned variable ) const
{
    return _var == variable;
}

List <unsigned> IntervalConstraint::getParticipatingVariables() const
{
    List<unsigned> variables;
    variables.append( _var );

    return variables;
}

bool IntervalConstraint::satisfied() const
{
    return false;
}

List <PiecewiseLinearConstraint::Fix> IntervalConstraint::getPossibleFixes() const
{
    return List<PiecewiseLinearConstraint::Fix>();
}

List <PiecewiseLinearCaseSplit> IntervalConstraint::getCaseSplits() const
{
    List <PiecewiseLinearCaseSplit> splits;
    double mid = (_lowerBound + _upperBound ) / 2;
    PiecewiseLinearCaseSplit lowerSplit;
    lowerSplit.storeBoundTightening( Tightening( _var, _lowerBound,
                                             Tightening::LB ) );
    lowerSplit.storeBoundTightening( Tightening( _var, mid,
                                             Tightening::UB ) );
    PiecewiseLinearCaseSplit upperSplit;
    upperSplit.storeBoundTightening( Tightening( _var, mid,
                                                  Tightening::LB ) );
    upperSplit.storeBoundTightening( Tightening( _var, _upperBound,
                                                  Tightening::UB ) );
    splits.append( lowerSplit );
    splits.append( upperSplit );
    return splits;
}

bool IntervalConstraint::phaseFixed() const
{
    return false;
}

PiecewiseLinearCaseSplit IntervalConstraint::getValidCaseSplit() const
{
    PiecewiseLinearCaseSplit emptySplit;
    return emptySplit;
}

List <PiecewiseLinearConstraint::Fix> IntervalConstraint::getSmartFixes
( ITableau * /* tableau */ ) const
{
    return getPossibleFixes();
}

void IntervalConstraint::eliminateVariable( unsigned /* variable */,
                                         double /* fixedValue */ )
{
    throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED,
                        "Eliminate variable from a DisjunctionConstraint" );

}

void IntervalConstraint::updateVariableIndex( unsigned oldIndex, unsigned newIndex )
{
    if ( _var == oldIndex )
        _var = newIndex;
}

bool IntervalConstraint::constraintObsolete() const
{
    return false;
}

void IntervalConstraint::getEntailedTightenings( List <Tightening> &/* tightenings */ ) const
{
}

String IntervalConstraint::serializeToString() const
{
    throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED,
                        "Serialize DisjunctionConstraint to String" );
}

void IntervalConstraint::updateScore()
{
    if ( GlobalConfiguration::SPLITTING_HEURISTICS ==
         DivideStrategy::LargestInterval )
    {
        _score = _upperBound - _lowerBound;
    }
}
