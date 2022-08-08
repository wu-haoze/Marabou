 /*********************                                                        */
/*! \file DeepPolyPosReciprocalElement.cpp
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

#include "DeepPolyPosReciprocalElement.h"
#include "FloatUtils.h"

namespace NLR {

DeepPolyPosReciprocalElement::DeepPolyPosReciprocalElement( Layer *layer )
{
    _layer = layer;
    _size = layer->getSize();
    _layerIndex = layer->getLayerIndex();
}

DeepPolyPosReciprocalElement::~DeepPolyPosReciprocalElement()
{
    freeMemoryIfNeeded();
}

void DeepPolyPosReciprocalElement::execute( const Map<unsigned, DeepPolyElement *>
                               &deepPolyElementsBefore )
{
    log( "Executing..." );
    ASSERT( hasPredecessor() );
    allocateMemory();

    // Update the symbolic and concrete upper- and lower- bounds
    // of each neuron
    for ( unsigned i = 0; i < _size; ++i )
    {
        NeuronIndex sourceIndex = *( _layer->getActivationSources( i ).begin() );
        DeepPolyElement *predecessor =
            deepPolyElementsBefore[sourceIndex._layer];
        double sourceLb = predecessor->getLowerBound
            ( sourceIndex._neuron );
        double sourceUb = predecessor->getUpperBound
            ( sourceIndex._neuron );

        _ub[i] = reciprocal( sourceLb );
        _lb[i] = reciprocal( sourceUb );

        if ( FloatUtils::areEqual( sourceUb, sourceLb ) )
        {
            _symbolicUb[i] = 0;
            _symbolicUpperBias[i] = _lb[i];
            _symbolicLb[i] = 0;
            _symbolicLowerBias[i] = _lb[i];
        }
        else
        {
            double lambda = ( _ub[i] - _lb[i] ) / ( sourceLb - sourceUb );

            // f = lambda (b - sourceLb) + recp(sourceLb)
            // update upper bound
            ASSERT( FloatUtils::isPositive( sourceLb ) );
            _symbolicUb[i] = lambda;
            _symbolicUpperBias[i] = reciprocal( sourceLb ) - lambda * sourceLb;

            double midPoint = ( sourceUb + sourceLb ) / 2;
            double lambdaPrime = reciprocalDerivative( midPoint );
            _symbolicLb[i] = lambdaPrime;
            _symbolicLowerBias[i] = reciprocal( midPoint ) - lambdaPrime * midPoint;
        }

        log( Stringf( "Neuron%u LB: %f b + %f, UB: %f b + %f",
                      i, _symbolicLb[i], _symbolicLowerBias[i],
                      _symbolicUb[i], _symbolicUpperBias[i] ) );
        log( Stringf( "Neuron%u LB: %f, UB: %f", i, _lb[i], _ub[i] ) );
    }
    log( "Executing - done" );
}

void DeepPolyPosReciprocalElement::symbolicBoundInTermsOfPredecessor
( const double *symbolicLb, const double*symbolicUb, double
  *symbolicLowerBias, double *symbolicUpperBias, double
  *symbolicLbInTermsOfPredecessor, double *symbolicUbInTermsOfPredecessor,
  unsigned targetLayerSize, DeepPolyElement *predecessor )
{
    log( Stringf( "Computing symbolic bounds with respect to layer %u...",
                  predecessor->getLayerIndex() ) );

    /*
      We have the symbolic bound of the target layer in terms of the
      PosReciprocal outputs, the goal is to compute the symbolic bound of the target
      layer in terms of the PosReciprocal inputs.
    */
    for ( unsigned i = 0; i < _size; ++i )
    {
        NeuronIndex sourceIndex = *( _layer->
                                     getActivationSources( i ).begin() );
        unsigned sourceNeuronIndex = sourceIndex._neuron;
        DEBUG({
                ASSERT( predecessor->getLayerIndex() == sourceIndex._layer );
            });

        /*
          Take symbolic upper bound as an example.
          Suppose the symbolic upper bound of the j-th neuron in the
          target layer is ... + a_i * f_i + ...,
          and the symbolic bounds of f_i in terms of b_i is
          m * b_i + n <= f_i <= p * b_i + q.
          If a_i >= 0, replace f_i with p * b_i + q, otherwise,
          replace f_i with m * b_i + n
        */

        // Symbolic bounds of the PosReciprocal output in terms of the PosReciprocal input
        // coeffLb * b_i + lowerBias <= f_i <= coeffUb * b_i + upperBias
        double coeffLb = _symbolicLb[i];
        double coeffUb = _symbolicUb[i];
        double lowerBias = _symbolicLowerBias[i];
        double upperBias = _symbolicUpperBias[i];

        // Substitute the PosReciprocal input for the PosReciprocal output
        for ( unsigned j = 0; j < targetLayerSize; ++j )
        {
            // The symbolic lower- and upper- bounds of the j-th neuron in the
            // target layer are ... + weightLb * f_i + ...
            // and ... + weightUb * f_i + ..., respectively.
            unsigned newIndex = sourceNeuronIndex * targetLayerSize + j;
            unsigned oldIndex = i * targetLayerSize + j;

            // Update the symbolic lower bound
            double weightLb = symbolicLb[oldIndex];
            if ( weightLb >= 0 )
            {
                symbolicLbInTermsOfPredecessor[newIndex] += weightLb * coeffLb;
                symbolicLowerBias[j] += weightLb * lowerBias;
            } else
            {
                symbolicLbInTermsOfPredecessor[newIndex] += weightLb * coeffUb;
                symbolicLowerBias[j] += weightLb * upperBias;
            }

            // Update the symbolic upper bound
            double weightUb = symbolicUb[oldIndex];
            if ( weightUb >= 0 )
            {
                symbolicUbInTermsOfPredecessor[newIndex] += weightUb * coeffUb;
                symbolicUpperBias[j] += weightUb * upperBias;
            } else
            {
                symbolicUbInTermsOfPredecessor[newIndex] += weightUb * coeffLb;
                symbolicUpperBias[j] += weightUb * lowerBias;
            }
        }
    }
}

void DeepPolyPosReciprocalElement::allocateMemory()
{
    freeMemoryIfNeeded();

    DeepPolyElement::allocateMemory();

    _symbolicLb = new double[_size];
    _symbolicUb = new double[_size];

    std::fill_n( _symbolicLb, _size, 0 );
    std::fill_n( _symbolicUb, _size, 0 );

    _symbolicLowerBias = new double[_size];
    _symbolicUpperBias = new double[_size];

    std::fill_n( _symbolicLowerBias, _size, 0 );
    std::fill_n( _symbolicUpperBias, _size, 0 );
}

void DeepPolyPosReciprocalElement::freeMemoryIfNeeded()
{
    DeepPolyElement::freeMemoryIfNeeded();
    if ( _symbolicLb )
    {
        delete[] _symbolicLb;
        _symbolicLb = NULL;
    }
    if ( _symbolicUb )
    {
        delete[] _symbolicUb;
        _symbolicUb = NULL;
    }
    if ( _symbolicLowerBias )
    {
        delete[] _symbolicLowerBias;
        _symbolicLowerBias = NULL;
    }
    if ( _symbolicUpperBias )
    {
        delete[] _symbolicUpperBias;
        _symbolicUpperBias = NULL;
    }
}

void DeepPolyPosReciprocalElement::log( const String &message )
{
    if ( GlobalConfiguration::NETWORK_LEVEL_REASONER_LOGGING )
        printf( "DeepPolyPosReciprocalElement: %s\n", message.ascii() );
}

double DeepPolyPosReciprocalElement::reciprocal( double x )
{
  ASSERT( x >= 0 );
  if ( !FloatUtils::isFinite(x) )
    return 0;
  else if ( FloatUtils::isZero(x) )
    return FloatUtils::infinity();
  else
    return 1/x;
}

double DeepPolyPosReciprocalElement::reciprocalDerivative( double x )
{
  ASSERT( x >= 0 );
  if ( !FloatUtils::isFinite(x) )
    return 0;
  else if ( FloatUtils::isZero(x) )
    return FloatUtils::infinity();
  else
    return -1/(x * x);
}

} // namespace NLR