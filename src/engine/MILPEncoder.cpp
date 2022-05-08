/*********************                                                        */
/*! \file MILPEncoder.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Wu, Teruhiro Tagomori
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief [[ Add one-line brief description here ]]
 **
 ** [[ Add lengthier description here ]]
 **/

#include "FloatUtils.h"
#include "GurobiWrapper.h"
#include "MILPEncoder.h"
#include "TimeUtils.h"

MILPEncoder::MILPEncoder( const ITableau &tableau )
    : _tableau( tableau )
    , _statistics( NULL )
{}

void MILPEncoder::reset()
{
    _variableToVariableName.clear();
    _binVarIndex = 0;
    _plVarIndex = 0;
}

void MILPEncoder::encodeInputQuery( GurobiWrapper &gurobi,
                                    const InputQuery &inputQuery,
                                    bool relax )
{
    struct timespec start = TimeUtils::sampleMicro();

    gurobi.resetModel();
    // Add variables
    for ( unsigned var = 0; var < inputQuery.getNumberOfVariables(); var++ )
    {
        double lb = _tableau.getLowerBound( var );
        double ub = _tableau.getUpperBound( var );
        String varName = Stringf( "x%u", var );
        gurobi.addVariable( varName, lb, ub );
        _variableToVariableName[var] = varName;
    }
    // Add equations
    for ( const auto &equation : inputQuery.getEquations() )
    {
        encodeEquation( gurobi, equation );
    }

    // Add Piecewise-linear Constraints
    for ( const auto &plConstraint : inputQuery.getPiecewiseLinearConstraints() )
    {
        if ( plConstraint->constraintObsolete() )
        {
            continue;
        }
        switch ( plConstraint->getType() )
        {
        case PiecewiseLinearFunctionType::RELU:
            encodeReLUConstraint( gurobi, (ReluConstraint *)plConstraint,
                                  relax );
            break;
        case PiecewiseLinearFunctionType::MAX:
            encodeMaxConstraint( gurobi, (MaxConstraint *)plConstraint,
                                 relax );
            break;
        case PiecewiseLinearFunctionType::SIGN:
            encodeSignConstraint( gurobi, (SignConstraint *)plConstraint,
                                  relax );
            break;
        case PiecewiseLinearFunctionType::ABSOLUTE_VALUE:
            encodeAbsoluteValueConstraint( gurobi,
                                           (AbsoluteValueConstraint *)plConstraint,
                                           relax );
            break;
        case PiecewiseLinearFunctionType::DISJUNCTION:
            encodeDisjunctionConstraint( gurobi,
                                         (DisjunctionConstraint *)plConstraint,
                                         relax );
            break;
        default:
            throw MarabouError( MarabouError::UNSUPPORTED_PIECEWISE_LINEAR_CONSTRAINT,
                                "GurobiWrapper::encodeInputQuery: "
                                "Unsupported piecewise-linear constraints\n" );
        }
    }

    // Add Transcendental Constraints
    for ( const auto &tsConstraint : inputQuery.getTranscendentalConstraints() )
    {
        switch ( tsConstraint->getType() )
        {
        case TranscendentalFunctionType::SIGMOID:
            encodeSigmoidConstraint( gurobi, (SigmoidConstraint *)tsConstraint );
            break;
        default:
            throw MarabouError( MarabouError::UNSUPPORTED_TRANSCENDENTAL_CONSTRAINT,
                                "GurobiWrapper::encodeInputQuery: "
                                "Only Sigmoid is supported\n" );
        }
        try
            {
                gurobi.updateModel();
            }
            catch ( GRBException e )
            {
                throw CommonError( CommonError::GUROBI_EXCEPTION,
                                   Stringf( "Gurobi exception. Gurobi Code: %u, message: %s\n",
                                            e.getErrorCode(),
                                            e.getMessage().c_str() ).ascii() );
            }

    }

    /*
    try
        {
            gurobi.updateModel();
        }
    catch ( GRBException e )
        {
            throw CommonError( CommonError::GUROBI_EXCEPTION,
                               Stringf( "Gurobi exception. Gurobi Code: %u, message: %s\n",
                                        e.getErrorCode(),
                                        e.getMessage().c_str() ).ascii() );
        }
    */
    gurobi.dumpModel( Stringf( "test.lp" ) );

    if ( _statistics )
    {
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->incLongAttribute
            ( Statistics::TIME_ADDING_CONSTRAINTS_TO_MILP_SOLVER_MICRO,
              TimeUtils::timePassed( start, end ) );
    }
}

String MILPEncoder::getVariableNameFromVariable( unsigned variable )
{
    if ( !_variableToVariableName.exists( variable ) )
        throw CommonError( CommonError::KEY_DOESNT_EXIST_IN_MAP );
    return _variableToVariableName[variable];
}

void MILPEncoder::encodeEquation( GurobiWrapper &gurobi, const Equation &equation )
{
    List<GurobiWrapper::Term> terms;
    double scalar = equation._scalar;
    for ( const auto &term : equation._addends )
        terms.append( GurobiWrapper::Term
                      ( term._coefficient,
                        Stringf( "x%u", term._variable ) ) );
    switch ( equation._type )
    {
    case Equation::EQ:
        gurobi.addEqConstraint( terms, scalar );
        break;
    case Equation::LE:
        gurobi.addLeqConstraint( terms, scalar );
        break;
    case Equation::GE:
        gurobi.addGeqConstraint( terms, scalar );
        break;
    default:
        break;
    }
}

void MILPEncoder::encodeReLUConstraint( GurobiWrapper &gurobi,
                                        ReluConstraint *relu, bool relax )
{

    if ( !relu->isActive() || relu->phaseFixed() )
    {
        ASSERT( relu->auxVariableInUse() );
        ASSERT( ( FloatUtils::gte( _tableau.getLowerBound( relu->getB() ),  0 ) &&
                  FloatUtils::lte( _tableau.getLowerBound( relu->getAux() ), 0 ) )
                ||
                ( FloatUtils::lte( _tableau.getUpperBound( relu->getB() ), 0 ) &&
                  FloatUtils::lte( _tableau.getUpperBound( relu->getF() ), 0 ) ) );
        return;
    }

    /*
      We have added f - b >= 0 and f >= 0. Additionally, we add
      f - b <= (1 - a) * (- lb_b) and f <= a * ub_f.

      When a = 1, the constraints become:
          f - b <= 0, f <= ub_f.
      When a = 0, the constriants become:
          f - b <= - lb_b, f <= 0
    */
    gurobi.addVariable( Stringf( "a%u", _binVarIndex ),
                        0,
                        1,
                        relax ?
                        GurobiWrapper::CONTINUOUS : GurobiWrapper::BINARY );

    unsigned sourceVariable = relu->getB();
    unsigned targetVariable = relu->getF();
    double sourceLb = _tableau.getLowerBound( sourceVariable );
    double targetUb = _tableau.getUpperBound( targetVariable );

    List<GurobiWrapper::Term> terms;
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
    terms.append( GurobiWrapper::Term( -1, Stringf( "x%u", sourceVariable ) ) );
    terms.append( GurobiWrapper::Term( -sourceLb, Stringf( "a%u", _binVarIndex ) ) );
    gurobi.addLeqConstraint( terms, -sourceLb );

    terms.clear();
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
    terms.append( GurobiWrapper::Term( -targetUb, Stringf( "a%u", _binVarIndex++ ) ) );
    gurobi.addLeqConstraint( terms, 0 );
}

void MILPEncoder::encodeMaxConstraint( GurobiWrapper &gurobi, MaxConstraint *max,
                                       bool relax )
{
    if ( !max->isActive() )
        return;

    List<GurobiWrapper::Term> terms;
    List<PhaseStatus> phases = max->getAllCases();
    for ( unsigned i = 0; i < phases.size(); ++i )
    {
        // add a binary variable for each disjunct
        gurobi.addVariable( Stringf( "a%u_%u", _binVarIndex, i ),
                            0,
                            1,
                            relax ?
                            GurobiWrapper::CONTINUOUS : GurobiWrapper::BINARY );

        terms.append( GurobiWrapper::Term( 1, Stringf( "a%u_%u", _binVarIndex, i ) ) );
    }

    // add constraint: a_1 + a_2 + ... + = 1
    gurobi.addEqConstraint( terms, 1 );

    terms.clear();
    unsigned index = 0;
    for ( const auto &phase : phases )
    {
        String binVarName = Stringf( "a%u_%u", _binVarIndex, index );
        PiecewiseLinearCaseSplit split = max->getCaseSplit( phase );
        if ( phase == MAX_PHASE_ELIMINATED )
        {
            /*
              We had y - eliminated value >= 0
              We add y - eliminated-value <= (1 - a) * (ub_y - eliminated-value),
              which becomes y + (ub_y - eliminated-value) * a <= ub_y
            */
            unsigned y = split.getBoundTightenings().begin()->_variable;
            double yUb = _tableau.getUpperBound( y );
            double eliminatedValue = split.getBoundTightenings().begin()->_value;

            terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", y ) ) );
            terms.append( GurobiWrapper::Term( yUb - eliminatedValue, binVarName ) );
            gurobi.addLeqConstraint( terms, yUb );
        }
        else
        {
            /*
              We added aux_i >= 0, for each x.
              We now add, aux_i <= (1 - a) * (ub_aux)
            */
            DEBUG({
                    ASSERT( split.getBoundTightenings().size() == 1 );
                    ASSERT( split.getEquations().size() == 0 );
                });
            unsigned aux = split.getBoundTightenings().begin()->_variable;
            double auxUb = _tableau.getUpperBound( aux );
            terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", aux ) ) );
            terms.append( GurobiWrapper::Term( auxUb, binVarName ) );
            gurobi.addLeqConstraint( terms, auxUb );
        }
        terms.clear();
        ++index;
    }

    _binVarIndex++;
}

void MILPEncoder::encodeAbsoluteValueConstraint( GurobiWrapper &gurobi,
                                                 AbsoluteValueConstraint *abs,
                                                 bool relax )
{
    ASSERT( abs->auxVariablesInUse() );

    if ( !abs->isActive() || abs->phaseFixed() )
    {
        ASSERT( ( FloatUtils::gte( _tableau.getLowerBound( abs->getB() ),  0 ) &&
                  FloatUtils::lte( _tableau.getUpperBound( abs->getPosAux() ), 0 ) )
                ||
                ( FloatUtils::lte( _tableau.getUpperBound( abs->getB() ), 0 ) &&
                  FloatUtils::lte( _tableau.getUpperBound( abs->getNegAux() ), 0 ) ) );
        return;
    }

    unsigned sourceVariable = abs->getB();
    unsigned targetVariable = abs->getF();
    double sourceLb = _tableau.getLowerBound( sourceVariable );
    double sourceUb = _tableau.getUpperBound( sourceVariable );
    double targetUb = _tableau.getUpperBound( targetVariable );

    ASSERT( FloatUtils::isPositive( sourceUb ) &&
            FloatUtils::isNegative( sourceLb ) );

    /*
      We have added f - b >= 0 and f + b >= 0. We add
      f - b <= (1 - a) * (ub_f - lb_b) and f + b <= a * (ub_f + ub_b)

      When a = 1, the constraints become:
      f - b <= 0, f + b <= ub_f + ub_b.
      When a = 0, the constriants become:
      f - b <= ub_f - lb_b, f + b <= 0
    */
    gurobi.addVariable( Stringf( "a%u", _binVarIndex ),
                        0,
                        1,
                        relax ?
                        GurobiWrapper::CONTINUOUS : GurobiWrapper::BINARY );

    List<GurobiWrapper::Term> terms;
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
    terms.append( GurobiWrapper::Term( -1, Stringf( "x%u", sourceVariable ) ) );
    terms.append( GurobiWrapper::Term( targetUb - sourceLb, Stringf( "a%u", _binVarIndex ) ) );
    gurobi.addLeqConstraint( terms, targetUb - sourceLb );

    terms.clear();
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", sourceVariable ) ) );
    terms.append( GurobiWrapper::Term( -( targetUb + sourceUb ),
                                       Stringf( "a%u", _binVarIndex ) ) );
    gurobi.addLeqConstraint( terms, 0 );
    ++_binVarIndex;
}

void MILPEncoder::encodeDisjunctionConstraint( GurobiWrapper &gurobi,
                                               DisjunctionConstraint *disj,
                                               bool relax )
{
    if ( !disj->isActive() )
        return;

    // terms for Gurobi
    List<GurobiWrapper::Term> terms;
    List<PiecewiseLinearCaseSplit> disjuncts = disj->getCaseSplits();
    for ( unsigned i = 0; i < disjuncts.size(); ++i )
    {
        // add a binary variable for each disjunct
        gurobi.addVariable( Stringf( "a%u_%u", _binVarIndex, i ),
                            0,
                            1,
                            relax ?
                            GurobiWrapper::CONTINUOUS : GurobiWrapper::BINARY );

        terms.append( GurobiWrapper::Term( 1, Stringf( "a%u_%u", _binVarIndex, i ) ) );
    }

    // add constraint: a_1 + a_2 + ... + >= 1
    gurobi.addGeqConstraint( terms, 1 );

    // Add each disjunct as indicator constraints
    terms.clear();
    unsigned index = 0;
    for ( const auto &disjunct : disjuncts )
    {
        String binVarName = Stringf( "a%u_%u", _binVarIndex, index );
        for ( const auto &tightening : disjunct.getBoundTightenings() )
        {
            // add indicator constraint: a_1 => disjunct1, etc.
            terms.append( GurobiWrapper::Term
                          ( 1, getVariableNameFromVariable
                            ( tightening._variable ) ) );
            if ( tightening._type == Tightening::UB )
                gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, tightening._value );
            else
                gurobi.addGeqIndicatorConstraint( binVarName, 1, terms, tightening._value );
            terms.clear();
        }
        ++index;
    }

    _binVarIndex++;
}

void MILPEncoder::encodeSignConstraint( GurobiWrapper &gurobi,
                                        SignConstraint *sign,
                                        bool relax )
{
    ASSERT( GlobalConfiguration::PL_CONSTRAINTS_ADD_AUX_EQUATIONS_AFTER_PREPROCESSING );

    if ( !sign->isActive() || sign->phaseFixed() )
    {
        ASSERT( ( FloatUtils::gte( _tableau.getLowerBound( sign->getB() ),  0 ) &&
                  FloatUtils::areEqual( _tableau.getLowerBound( sign->getF() ), 1 ) )
                ||
                ( FloatUtils::lt( _tableau.getUpperBound( sign->getB() ), 0 ) &&
                  FloatUtils::areEqual( _tableau.getLowerBound( sign->getF() ), -1 ) ) );
        return;
    }

    unsigned targetVariable = sign->getF();
    DEBUG({
            unsigned sourceVariable = sign->getB();

            double sourceLb = _tableau.getLowerBound( sourceVariable );
            double sourceUb = _tableau.getUpperBound( sourceVariable );
            ASSERT( !FloatUtils::isNegative( sourceUb ) &&
                    FloatUtils::isNegative( sourceLb ) );
        });

    /*
      We have added f <= -2/lb b + 1 and f >= 2/ub * b - 1. We just need to specify
      f is either -1 or 1. That is f = 2 * (a - 0.5)

      f is 1 if a is 1 and -1 if a is 0.
      Moreover, when f is 1, 1 <= -2 / lb_b * b + 1, thus, b >= 0.
      When f is -1, -1 >= 2/ub_b * b - 1, thus, b <= 0.
    */
    gurobi.addVariable( Stringf( "a%u", _binVarIndex ),
                        0,
                        1,
                        relax ?
                        GurobiWrapper::CONTINUOUS : GurobiWrapper::BINARY );

    List<GurobiWrapper::Term> terms;
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
    terms.append( GurobiWrapper::Term( -2, Stringf( "a%u", _binVarIndex ) ) );
    gurobi.addEqConstraint( terms, -1 );

    ++_binVarIndex;
}

void MILPEncoder::encodeSigmoidConstraint( GurobiWrapper &gurobi,
                                           SigmoidConstraint *sigmoid )
{

    unsigned sourceVariable = sigmoid->getB();  // x_b
    unsigned targetVariable = sigmoid->getF();  // x_f
    double sourceLb = _tableau.getLowerBound( sourceVariable );
    double sourceUb = _tableau.getUpperBound( sourceVariable );
    double targetLb = _tableau.getLowerBound( targetVariable );
    double targetUb = _tableau.getUpperBound( targetVariable );

    if ( sigmoid->phaseFixed() )
    {
        List<GurobiWrapper::Term> terms;
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        gurobi.addLeqConstraint( terms, _tableau.getLowerBound( targetVariable ) );
        return;
    }


    double lambda = ( sigmoid->sigmoid(sourceUb) - sigmoid->sigmoid( sourceLb ) ) /
        ( sourceUb - sourceLb );
    double lambdaPrime = std::min( sigmoid->sigmoidDerivative( sourceLb ),
                                   sigmoid->sigmoidDerivative( sourceUb ) );
    double upperBias = 0;
    double lowerBias = 0;

    if ( !FloatUtils::isNegative( sourceLb ) )
    {
        List<GurobiWrapper::Term> terms;
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        terms.append( GurobiWrapper::Term( -lambda, Stringf( "x%u", sourceVariable ) ) );
        lowerBias = targetLb - lambda * sourceLb;
        gurobi.addGeqConstraint( terms, lowerBias );
    }
    else
    {
        List<GurobiWrapper::Term> terms;
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        terms.append( GurobiWrapper::Term( -lambdaPrime, Stringf( "x%u", sourceVariable ) ) );
        lowerBias = targetLb - lambdaPrime * sourceLb;
        gurobi.addGeqConstraint( terms, lowerBias );
    }

    if ( !FloatUtils::isPositive( sourceUb ) )
    {
        List<GurobiWrapper::Term> terms;
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        terms.append( GurobiWrapper::Term( -lambda, Stringf( "x%u", sourceVariable ) ) );
        upperBias = targetUb - lambda * sourceUb;
        gurobi.addLeqConstraint( terms, upperBias );
    }
    else
    {
        List<GurobiWrapper::Term> terms;
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        terms.append( GurobiWrapper::Term( -lambdaPrime, Stringf( "x%u", sourceVariable ) ) );
        upperBias = targetUb - lambdaPrime * sourceUb;
        gurobi.addLeqConstraint( terms, upperBias );
    }

    for ( const auto &point : sigmoid->getCutPoints() )
    {
        double x = point._x;
        double y = sigmoid->sigmoid( x );
        if ( FloatUtils::lte( point._x, 0 ) && point._above )
        {
            // Top left.
            double slopeLeft = ( y - targetLb ) / ( x - sourceLb );
            double scalarLeft = y - slopeLeft * x;

            double slopeRight = 0;
            double scalarRight = 0;
            if ( FloatUtils::lte( sourceUb, 0 ) )
                slopeRight =  ( y - targetUb ) / ( x - sourceUb );
            else
                slopeRight =  ( y - upperBias ) / ( x );
            scalarRight = y - slopeRight * x;

            addCutConstraint( gurobi, true, slopeLeft, scalarLeft, slopeRight, scalarRight );
        }
        else if ( FloatUtils::gte( point._x, 0 ) && !point._above )
        {
            // Bottom right
            double slopeRight = ( y - targetUb ) / ( x - sourceUb );
            double scalarRight = y - slopeRight * x;

            double slopeLeft = 0;
            double scalarLeft = 0;
            if ( FloatUtils::gte( sourceLb, 0 ) )
                slopeLeft =  ( y - targetLb ) / ( x - sourceLb );
            else
                slopeLeft =  ( y - lowerBias ) / ( x );
            scalarLeft = y - slopeLeft * x;

            addCutConstraint( gurobi, false, slopeLeft, scalarLeft, slopeRight, scalarRight );
        }
        else if ( FloatUtils::gte( point._x, 0 ) && !point._above )
        {
            // Bottom left.
            double slopeLeft = sigmoid->sigmoidDerivative( x );
            double scalarLeft = y - slopeLeft * x;

            double slopeRight = 0;
            double scalarRight = 0;
            if ( FloatUtils::lte( sourceUb, 0 ) )
            {
                slopeRight =  slopeLeft;
                scalarRight = scalarLeft;
            }
            else
            {
                slopeRight = std::min( sigmoid->sigmoidDerivative( sourceUb ),
                                       sigmoid->sigmoidDerivative( x ) );
                scalarRight = y - slopeRight * x;
            }
            addCutConstraint( gurobi, false, slopeLeft, scalarLeft, slopeRight, scalarRight );
        }
        else if ( FloatUtils::gte( point._x, 0 ) && !point._above )
        {
            // Top right.
            double slopeRight = sigmoid->sigmoidDerivative( x );
            double scalarRight = y - slopeLeft * x;

            double slopeLeft = 0;
            double scalarLeft = 0;
            if ( FloatUtils::gte( sourceLb, 0 ) )
            {
                slopeLeft =  slopeRight;
                scalarLeft = scalarRight;
            }
            else
            {
                slopeLeft = std::min( sigmoid->sigmoidDerivative( sourceLb ),
                                      sigmoid->sigmoidDerivative( x ) );
                scalarLeft = y - slopeLeft * x;
            }
            addCutConstraint( gurobi, true, slopeLeft, scalarLeft, slopeRight, scalarRight );
        }
    }

    return;
    /*
    else if ( FloatUtils::lt( sourceLb, 0 ) && FloatUtils::gt( sourceUb, 0 ) )
    {
        // The sigmoid convexity changes.

        // set binVarName
        String binVarName = Stringf( "a%u", _binVarIndex );
        gurobi.addVariable( binVarName, 0, 1, GurobiWrapper::BINARY );
        _binVarIndex++;

        sigmoid->setBinVarName( binVarName );

        // a = 1 -> the case where x_b >= 0, otherwise where x_b <= 0
        List<GurobiWrapper::Term> terms;
        // set lower bound of x_b
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", sourceVariable ) ) );
        gurobi.addGeqIndicatorConstraint( binVarName, 1, terms, 0 );
        terms.clear();
        // set lower bound of x_f
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        gurobi.addGeqIndicatorConstraint( binVarName, 1, terms, 0.5 );
        terms.clear(); 
        // upper bound of x_b
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", sourceVariable ) ) );
        gurobi.addLeqIndicatorConstraint( binVarName, 0, terms, 0 );
        terms.clear();

        // upper bound of x_f
        terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
        gurobi.addLeqIndicatorConstraint( binVarName, 0, terms, 0.5 );
        terms.clear();

        if ( sigmoid->getTangentPoints().size() == 0 )
        {
            sigmoid->addTangentPoint( sourceLb / 2 );
            sigmoid->addTangentPoint( sourceUb / 2 );
        }
        for ( const auto &x : sigmoid->getTangentPoints() )
            addTangentLineOnSigmoid( gurobi, sigmoid, x );

        if ( sigmoid->getSecantPoints().size() == 0 )
        {
            sigmoid->addSecantPoint( sourceLb );
            sigmoid->addSecantPoint( 0 );
            sigmoid->addSecantPoint( sourceUb );
        }

        addSecantLinesOnSigmoid( gurobi, sigmoid, sigmoid->getSecantPoints() );
    }
    else
    {
        // One sided
        if ( sigmoid->getTangentPoints().size() == 0 )
        {
            sigmoid->addTangentPoint( ( sourceLb + sourceUb ) / 2 );
        }
        for ( const auto &x : sigmoid->getTangentPoints() )
            addTangentLineOnSigmoid( gurobi, sigmoid, x );

        if ( sigmoid->getSecantPoints().size() == 0 )
        {
            sigmoid->addSecantPoint( sourceLb );
            sigmoid->addSecantPoint( sourceUb );
        }
        addSecantLinesOnSigmoid( gurobi, sigmoid, sigmoid->getSecantPoints() );
    }
    */
}

/*
void MILPEncoder::addTangentLineOnSigmoid( GurobiWrapper &gurobi,
                                           SigmoidConstraint *sigmoid,
                                           double x )
{
    unsigned sourceVariable = sigmoid->getB();  // x_b
    unsigned targetVariable = sigmoid->getF();  // x_f
    double slope = sigmoid->sigmoidDerivative( x );
    double y = sigmoid->sigmoid( x );
    String binVarName = sigmoid->getBinVarName();

    // tangent line: x_f ? slope * (x_b - x) + y
    List<GurobiWrapper::Term> terms;
    terms.append( GurobiWrapper::Term( 1, Stringf( "x%u", targetVariable ) ) );
    terms.append( GurobiWrapper::Term( -slope, Stringf( "x%u", sourceVariable ) ) );
    double rhs = y - slope * x;

    if ( FloatUtils::gte( x, 0 ) )
    {
        if ( binVarName == "NONE" )
        {
            // Directly add an upper bound:
            gurobi.addLeqConstraint( terms, rhs );
        }
        else
        {
            // If binary variable exists, then add indicator constraint.
            gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, rhs );
        }
    }
    else
    {
        if ( binVarName == "NONE" )
        {
            // Directly add an lower bound:
            gurobi.addGeqConstraint( terms, rhs );
        }
        else
        {
            // If binary variable exists, then add indicator constraint.
            gurobi.addGeqIndicatorConstraint( binVarName, 0, terms, rhs );
        }
    }
}

void MILPEncoder::addSecantLinesOnSigmoid( GurobiWrapper &gurobi,
                                           SigmoidConstraint *sigmoid,
                                           const std::set<double> &points )
{
    if ( points.size() < 2 )
    {
        std::cout << "Something's wrong." << std::endl;
        return;
    }

    unsigned sourceVariable = sigmoid->getB();  // x_b
    unsigned targetVariable = sigmoid->getF();  // x_f
    String xVarName = Stringf( "x%u", sourceVariable );
    String yVarName = Stringf( "x%u", targetVariable );
    String binVarName = sigmoid->getBinVarName();
    unsigned numOfPts = points.size();

    // add a piece-wise linear constraint
    double xpts[numOfPts];
    double ypts[numOfPts];
    unsigned i = 0;

    for ( const auto &point : points )
    {
        xpts[i] = point;
        ypts[i] = sigmoid->sigmoid( point );
        ++i;
    }

    List<GurobiWrapper::Term> termsOneHot;
    for ( unsigned i = 1; i < numOfPts; ++i )
    {
        // x_f ? slope * x_b + scalar
        List<GurobiWrapper::Term> terms;

        String binVarName = Stringf( "a%u_aux", _binVarIndex );
        termsOneHot.append( GurobiWrapper::Term( 1, binVarName ) );
        gurobi.addVariable( binVarName, 0, 1, GurobiWrapper::BINARY );
        _binVarIndex++;

        // a = 1 -> x_b >= xpts[i-1] , x_b <= xpts[i]
        terms.append( GurobiWrapper::Term( 1, xVarName ) );
        gurobi.addGeqIndicatorConstraint( binVarName, 1, terms, xpts[i-1] );
        terms.clear();
        terms.append( GurobiWrapper::Term( 1, xVarName ) );
        gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, xpts[i] );
        terms.clear();
        terms.append( GurobiWrapper::Term( 1, yVarName ) );
        gurobi.addGeqIndicatorConstraint( binVarName, 1, terms, ypts[i-1] );
        terms.clear();
        terms.append( GurobiWrapper::Term( 1, yVarName ) );
        gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, ypts[i] );
        terms.clear();

        double slope = (ypts[i] - ypts[i - 1]) / (xpts[i] - xpts[i - 1]);
        double scalar = ypts[i] - slope * xpts[i];
        terms.append( GurobiWrapper::Term( 1, yVarName ) );
        terms.append( GurobiWrapper::Term( -slope, xVarName ) );

        if ( FloatUtils::gte( xpts[i-1], 0 ) )
        {
            gurobi.addGeqIndicatorConstraint( binVarName, 1, terms, scalar );
        }
        else
        {
            gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, scalar );
        }
    }

    gurobi.addEqConstraint( termsOneHot, 1 );

    /*
        if ( FloatUtils::gte( xpts[0], 0 ) )
        {
            if ( binVarName == "NONE" )
            {
                gurobi.addLeqConstraint( terms, 0 );
            }
            else
            {
                // If binary variable exists, then add indicator constraint.
                gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, 0 );
            }
        }
    }

    String yplVarName = Stringf( "pl_%s_%u", yVarName.ascii(), _plVarIndex++ );
    gurobi.addVariable( yplVarName, ypts[0], ypts[numOfPts - 1] );
    gurobi.addPiecewiseLinearConstraint( xVarName, yplVarName, numOfPts, xpts, ypts );

    if ( FloatUtils::gte( xpts[0], 0 ) )
        {
            if ( binVarName == "NONE" )
                {
                    // y_pl <= y
                    gurobi.addLeqConstraint( terms, 0 );
                }
            else
                {
                    // If binary variable exists, then add indicator constraint.
                    gurobi.addLeqIndicatorConstraint( binVarName, 1, terms, 0 );
                }
        }

    else
    {
        if ( binVarName == "NONE" )
        {
            // y_pl >= y
            gurobi.addGeqConstraint( terms, 0 );
        }
        else
        {
            // If binary variable exists, then add indicator constraint.
            gurobi.addGeqIndicatorConstraint( binVarName, 0, terms, 0 );
        }
    }
    */
}


void MILPEncoder::encodeCostFunction( GurobiWrapper &gurobi,
                                      const LinearExpression &cost )
{
    List<GurobiWrapper::Term> terms;
    for ( const auto &pair : cost._addends )
    {
        terms.append( GurobiWrapper::Term
                      ( pair.second,
                        Stringf( "x%u", pair.first ) ) );
    }
    gurobi.setCost( terms, cost._constant );
}
