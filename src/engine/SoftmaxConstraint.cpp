/*********************                                                        */
/*! \file SoftmaxConstraint.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Haoze Wu
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** See the description of the class in SoftmaxConstraint.h.
 **/

#include "SoftmaxConstraint.h"

#include "ConstraintBoundTightener.h"
#include "TranscendentalConstraint.h"
#include "Debug.h"
#include "FloatUtils.h"
#include "GlobalConfiguration.h"
#include "ITableau.h"
#include "InputQuery.h"
#include "MStringf.h"
#include "MarabouError.h"
#include "Statistics.h"
#include "TableauRow.h"

#ifdef _WIN32
#define __attribute__(x)
#endif

SoftmaxConstraint::SoftmaxConstraint( const Vector<unsigned> &inputs,
                                      const Vector<unsigned> &outputs )
    : TranscendentalConstraint()
    , _inputs( inputs )
    , _outputs( outputs )
{
}

SoftmaxConstraint::SoftmaxConstraint( const String & )
{
    throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

TranscendentalFunctionType SoftmaxConstraint::getType() const
{
    return TranscendentalFunctionType::SOFTMAX;
}

TranscendentalConstraint *SoftmaxConstraint::duplicateConstraint() const
{
    SoftmaxConstraint *clone = new SoftmaxConstraint( _inputs, _outputs );
    *clone = *this;
    return clone;
}

void SoftmaxConstraint::restoreState( const TranscendentalConstraint *state )
{
    const SoftmaxConstraint *softmax = dynamic_cast<const SoftmaxConstraint *>( state );
    *this = *softmax;
}

void SoftmaxConstraint::registerAsWatcher( ITableau * )
{
}

void SoftmaxConstraint::unregisterAsWatcher( ITableau * )
{
}

void SoftmaxConstraint::notifyLowerBound( unsigned variable, double bound )
{
    if ( _statistics )
        _statistics->incLongAttribute( Statistics::NUM_BOUND_NOTIFICATIONS_TO_TRANSCENDENTAL_CONSTRAINTS );

    if ( existsLowerBound( variable ) && !FloatUtils::gt( bound, getLowerBound( variable ) ) )
        return;

    setLowerBound( variable, bound );

    if ( _constraintBoundTightener )
    {
    }
}

void SoftmaxConstraint::notifyUpperBound( unsigned variable, double bound )
{
    if ( _statistics )
        _statistics->incLongAttribute( Statistics::NUM_BOUND_NOTIFICATIONS_TO_TRANSCENDENTAL_CONSTRAINTS );

    if ( existsUpperBound( variable ) && !FloatUtils::lt( bound, getUpperBound( variable ) ) )
        return;

    setUpperBound( variable, bound );

    if ( _constraintBoundTightener )
    {
    }
}

bool SoftmaxConstraint::participatingVariable( unsigned variable ) const
{
    return ( _inputs.exists( variable ) || _outputs.exists( variable ) );
}

List<unsigned> SoftmaxConstraint::getParticipatingVariables() const
{
    List<unsigned> toReturn;
    for ( const auto &var : _inputs )
        toReturn.append( var );
    for ( const auto &var : _outputs )
        toReturn.append( var );
    return toReturn;
}

void SoftmaxConstraint::dump( String &output ) const
{
    output = Stringf( "Softmax constraint\n" );
}

void SoftmaxConstraint::updateVariableIndex( unsigned /*oldIndex*/, unsigned /*newIndex*/ )
{
    throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

void SoftmaxConstraint::eliminateVariable( __attribute__((unused)) unsigned variable,
                                        __attribute__((unused)) double fixedValue )
{
    throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

bool SoftmaxConstraint::constraintObsolete() const
{
    return false;
}

void SoftmaxConstraint::getEntailedTightenings( List<Tightening> & ) const
{
}

String SoftmaxConstraint::serializeToString() const
{
    throw MarabouError( MarabouError::FEATURE_NOT_YET_SUPPORTED );
}

const Vector<unsigned> &SoftmaxConstraint::getInputs() const
{
    return _inputs;
}

const Vector<unsigned> &SoftmaxConstraint::getOutputs() const
{
    return _outputs;
}
