/*********************                                                        */
/*! \file DeepPolySoftmaxElement.cpp
** \verbatim
** Top contributors (to current version):
**   Andrew Wu
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** [[ Add lengthier description here ]]

**/

#include "DeepPolySoftmaxElement.h"
#include "FloatUtils.h"
#include "Options.h"
#include "SoftmaxConstraint.h"

#include <string.h>

namespace NLR {

    DeepPolySoftmaxElement::DeepPolySoftmaxElement( Layer *layer )
        : _boundType( Options::get()->getSoftmaxBoundType() )
    {
        log( Stringf( "Softmax bound type: %s",
                      Options::get()->getString( Options::SOFTMAX_BOUND_TYPE ).ascii() ) );
        _layer = layer;
        _size = layer->getSize();
        _layerIndex = layer->getLayerIndex();
    }

    DeepPolySoftmaxElement::~DeepPolySoftmaxElement()
    {
        freeMemoryIfNeeded();
    }

    void DeepPolySoftmaxElement::execute
    ( const Map<unsigned, DeepPolyElement *> &deepPolyElementsBefore )
    {
        log( "Executing..." );
        ASSERT( hasPredecessor() );
        allocateMemory();
        getConcreteBounds();

        // Update the symbolic and concrete upper- and lower- bounds
        // of each neuron
        for ( unsigned i = 0; i < _size; ++i )
        {
            List<NeuronIndex> sources = _layer->getActivationSources( i );

            Vector<double> sourceLbs;
            Vector<double> sourceUbs;
            Vector<double> sourceMids;
            Vector<double> targetLbs;
            Vector<double> targetUbs;
            for ( const auto &sourceIndex : sources )
            {
                DeepPolyElement *predecessor =
                    deepPolyElementsBefore[sourceIndex._layer];
                double sourceLb = predecessor->getLowerBound
                    ( sourceIndex._neuron );
                sourceLbs.append( sourceLb );
                double sourceUb = predecessor->getUpperBound
                    ( sourceIndex._neuron );
                sourceUbs.append( sourceUb );
                sourceMids.append((sourceLb + sourceUb) / 2);
                targetLbs.append(_lb[i]);
                targetUbs.append(_ub[i]);
            }

            // Find the index of i in the softmax
            unsigned index = 0;
            for ( const auto &sourceIndex : sources ){
                if ( sourceIndex._neuron == i ) {
                    break;
                }
                ++index;
            }

            double lb = linearLowerBound( sourceLbs, sourceUbs, index );
            double ub = linearUpperBound( sourceLbs, sourceUbs, index );
            if ( lb > _lb[i])
                _lb[i] = lb;
            if ( ub < _ub[i])
                _ub[i] = ub;
            log( Stringf( "Current bounds of neuron %u: [%f, %f]\n", i,
                          _lb[i], _ub[i] ) );
            targetLbs[index] = _lb[i];
            targetUbs[index] = _ub[i];

            // Compute symbolic bound
            if ( _boundType == SoftmaxBoundType::LOG_SUM_EXP_DECOMPOSITION )
            {
                _symbolicLowerBias[i] = LSELowerBound( sourceMids, sourceLbs,
                                                       sourceUbs, index );
                unsigned inputIndex = 0;
                for ( const auto &sourceIndex : sources )
                {
                    double dldj = dLSELowerBound( sourceMids, sourceLbs,
                                                   sourceUbs, index, inputIndex );
                    _symbolicLb[_size * sourceIndex._neuron + i] = dldj;
                    _symbolicLowerBias[i] -= dldj * sourceMids[inputIndex];
                    ++inputIndex;
                }

                _symbolicUpperBias[i] = LSEUpperBound(sourceMids, targetLbs,
                                                      targetUbs, index);
                inputIndex = 0;
                for (const auto &sourceIndex : sources)
                {
                    double dudj = dLSEUpperbound( sourceMids, targetLbs,
                                                  targetUbs, index, inputIndex );
                    _symbolicUb[_size * sourceIndex._neuron + i] = dudj;
                    _symbolicUpperBias[i] -= dudj * sourceMids[inputIndex];
                    ++inputIndex;
                }
            }
            else if ( _boundType == SoftmaxBoundType::EXPONENTIAL_RECIPROCAL_DECOMPOSITION )
            {
                _symbolicLowerBias[i] = ERLowerBound( sourceMids, sourceLbs,
                                                      sourceUbs, index );
                unsigned inputIndex = 0;
                for (const auto &sourceIndex : sources)
                {
                    double dldj = dERLowerBound( sourceMids, sourceLbs,
                                                 sourceUbs, index, inputIndex );
                    _symbolicLb[_size * sourceIndex._neuron + i] = dldj;
                    _symbolicLowerBias[i] -= dldj * sourceMids[inputIndex];
                    ++inputIndex;
                }

                _symbolicUpperBias[i] = ERUpperBound( sourceMids, targetLbs,
                                                      targetUbs, index );
                inputIndex = 0;
                for ( const auto &sourceIndex : sources )
                {
                    double dudj = dERUpperBound( sourceMids, targetLbs,
                                                 targetUbs, index, inputIndex );
                    _symbolicUb[_size * sourceIndex._neuron + i] = dudj;
                    _symbolicUpperBias[i] -= dudj * sourceMids[inputIndex];
                    ++inputIndex;
                }
            }
        }
        log( "Executing - done" );
    }

    void DeepPolySoftmaxElement::symbolicBoundInTermsOfPredecessor
    ( const double *symbolicLb, const double*symbolicUb, double
      *symbolicLowerBias, double *symbolicUpperBias, double
      *symbolicLbInTermsOfPredecessor, double *symbolicUbInTermsOfPredecessor,
      unsigned targetLayerSize, DeepPolyElement *predecessor )
    {
        log( Stringf( "Computing symbolic bounds with respect to layer %u...",
                      predecessor->getLayerIndex() ) );

        unsigned predecessorSize = predecessor->getSize();
        ASSERT(predecessorSize == _size);

        /*
          We have the symbolic bound of the target layer in terms of the
          MaxPool outputs, the goal is to compute the symbolic bound of the target
          layer in terms of the MaxPool inputs.
        */
        for ( unsigned i = 0; i < targetLayerSize; ++i )
        {
            for ( unsigned j = 0; j < _size; ++j )
            {
                {
                    double weightLb = symbolicLb[j * targetLayerSize + i];
                    if ( weightLb >= 0 )
                    {
                        for ( unsigned k = 0; k < predecessorSize; ++k )
                        {
                            symbolicLbInTermsOfPredecessor[k * targetLayerSize + i] +=
                                weightLb * _symbolicLb[k * _size + j];
                        }
                        symbolicLowerBias[i] += _symbolicLowerBias[j] * weightLb;
                    }
                    else
                    {
                        for ( unsigned k = 0; k < predecessorSize; ++k )
                        {
                            symbolicLbInTermsOfPredecessor[k * targetLayerSize + i] +=
                                weightLb * _symbolicUb[k * _size + j];
                        }
                        symbolicLowerBias[i] += _symbolicUpperBias[j] * weightLb;
                    }
                }

                {
                    double weightUb = symbolicUb[j * targetLayerSize + i];
                    if ( weightUb >= 0 )
                    {
                        for ( unsigned k = 0; k < predecessorSize; ++k )
                        {
                            symbolicUbInTermsOfPredecessor[k * targetLayerSize + i] +=
                                weightUb * _symbolicUb[k * _size + j];
                        }
                        symbolicUpperBias[i] += _symbolicUpperBias[j] * weightUb;
                    } else
                    {
                        for ( unsigned k = 0; k < predecessorSize; ++k )
                        {
                            symbolicUbInTermsOfPredecessor[k * targetLayerSize + i] +=
                                weightUb * _symbolicLb[k * _size + j];
                        }
                        symbolicUpperBias[i] += _symbolicLowerBias[j] * weightUb;
                    }
                }
            }
        }
    }

    void DeepPolySoftmaxElement::allocateMemory()
    {
        freeMemoryIfNeeded();

        DeepPolyElement::allocateMemory();

        unsigned size = _size * _size;
        _symbolicLb = new double[size];
        _symbolicUb = new double[size];

        std::fill_n( _symbolicLb, size, 0 );
        std::fill_n( _symbolicUb, size, 0 );

        _symbolicLowerBias = new double[_size];
        _symbolicUpperBias = new double[_size];

        std::fill_n( _symbolicLowerBias, _size, 0 );
        std::fill_n( _symbolicUpperBias, _size, 0 );
    }

    void DeepPolySoftmaxElement::freeMemoryIfNeeded()
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

    double DeepPolySoftmaxElement::LSELowerBound( const Vector<double> &input,
                                                   const Vector<double> &inputLb,
                                                   const Vector<double> &inputUb,
                                                   unsigned i )
    {
        double sum = 0;
        for (unsigned j = 0; j < input.size(); ++j) {
            double lj = inputLb[j];
            double uj = inputUb[j];
            double xj = input[j];

            sum += (uj - xj) / (uj - lj) * std::exp(lj) + (xj - lj)/(uj - lj) * std::exp(uj);
        }

        return std::exp(input[i]) / sum;
    }

    double DeepPolySoftmaxElement::dLSELowerBound( const Vector<double> &c,
                                                    const Vector<double> &inputLb,
                                                    const Vector<double> &inputUb,
                                                    unsigned i, unsigned di )
    {
        double val = 0;
        if (i == di)
            val += LSELowerBound(c, inputLb, inputUb, i);

        double ldi = inputLb[di];
        double udi = inputUb[di];

        double sum = 0;
        for (unsigned j = 0; j < c.size(); ++j) {
            double lj = inputLb[j];
            double uj = inputUb[j];
            double xj = c[j];

            sum += (uj - xj) / (uj - lj) * std::exp(lj) + (xj - lj)/(uj - lj) * std::exp(uj);
        }

        val -= std::exp(c[i]) / (sum * sum) * (std::exp(udi) - std::exp(ldi)) / (udi - ldi);

        return val;
    }


    double DeepPolySoftmaxElement::LSEUpperBound( const Vector<double> &input,
                                                  const Vector<double> &outputLb,
                                                  const Vector<double> &outputUb,
                                                  unsigned i )
    {
        double li = outputLb[i];
        double ui = outputUb[i];

        Vector<double> inputTilda;
        SoftmaxConstraint::xTilda( input, input[i], inputTilda );

        return ((li * std::log( ui ) - ui * std::log( li ) ) /
                ( std::log( ui ) - std::log( li ) ) -
                ( ui - li ) / ( std::log( ui ) - std::log( li ) )
                * SoftmaxConstraint::logSumOfExponential( inputTilda ) );
    }

    double DeepPolySoftmaxElement::dLSEUpperbound( const Vector<double> &c,
                                                   const Vector<double> &outputLb,
                                                   const Vector<double> &outputUb,
                                                   unsigned i, unsigned di )
    {
        double li = outputLb[i];
        double ui = outputUb[i];

        double val = -(ui - li) / (std::log(ui) - std::log(li));

        double val2 = std::exp(c[di]) / SoftmaxConstraint::sumOfExponential(c);
        if (i == di)
            val2 -= 1;

        return val * val2;
    }

    double DeepPolySoftmaxElement::ERLowerBound( const Vector<double> &input,
                                                 const Vector<double> &inputLb,
                                                 const Vector<double> &inputUb,
                                                 unsigned i )
    {
        Vector<double> inputTilda;
        SoftmaxConstraint::xTilda(input, input[i], inputTilda);

        double sum = 0;
        for (unsigned j = 0; j < input.size(); ++j) {
            if ( i == j )
                sum += 1;
            else
            {
                double ljTilda = inputLb[j] - inputUb[i];
                double ujTilda = inputUb[j] - inputLb[i];
                double xjTilda = inputTilda[j];

                sum += (ujTilda - xjTilda) / (ujTilda - ljTilda) * std::exp(ljTilda) +
                    (xjTilda - ljTilda)/(ujTilda - ljTilda) * std::exp(ujTilda);
            }
        }

        return 1 / sum;
    }

    double DeepPolySoftmaxElement::dERLowerBound( const Vector<double> &c,
                                                  const Vector<double> &inputLb,
                                                  const Vector<double> &inputUb,
                                                  unsigned i, unsigned di)
    {
        double val = ERLowerBound(c, inputLb, inputUb, i);

        if ( i != di )
        {
            double ldiTilda = inputLb[di] - inputUb[i];
            double udiTilda = inputUb[di] - inputLb[i];
            return -val * val * (std::exp(udiTilda) - std::exp(ldiTilda)) / (udiTilda - ldiTilda);
        }
        else {
            double val2 = 0;
            for ( unsigned j = 0; j < c.size(); ++j ) {
                if ( j != i )
                {
                    double ljTilda = inputLb[j] - inputUb[i];
                    double ujTilda = inputUb[j] - inputLb[i];
                    val2 += (std::exp(ujTilda) - std::exp(ljTilda)) / (ujTilda - ljTilda);
                }
            }
            return val * val * val2;
        }
    }

    double DeepPolySoftmaxElement::ERUpperBound( const Vector<double> &input,
                                                 const Vector<double> &outputLb,
                                                 const Vector<double> &outputUb,
                                                 unsigned i )
    {
        double li = outputLb[i];
        double ui = outputUb[i];

        Vector<double> inputTilda;
        SoftmaxConstraint::xTilda(input, input[i], inputTilda);

        return ui + li - ui * li * SoftmaxConstraint::sumOfExponential(inputTilda);
    }

    double DeepPolySoftmaxElement::dERUpperBound( const Vector<double> &c,
                                                  const Vector<double> &outputLb,
                                                  const Vector<double> &outputUb,
                                                  unsigned i, unsigned  di)
    {
        double li = outputLb[i];
        double ui = outputUb[i];


        if (i == di)
        {
            double val2 = -1;
            for ( unsigned j = 0; j < c.size(); ++j )
                val2 += std::exp(c[j] - c[i]);
            return li * ui * val2;
        }
        else
            return -li * ui * std::exp(c[di] - c[i]);
    }

    double DeepPolySoftmaxElement::linearLowerBound( const Vector<double> &inputLb,
                                                     const Vector<double> &inputUb,
                                                     unsigned i )
    {
        Vector<double> uTilda;
        SoftmaxConstraint::xTilda( inputUb, inputLb[i], uTilda );
        uTilda[i] = 0;
        return 1 / SoftmaxConstraint::sumOfExponential( uTilda );
    }

    double DeepPolySoftmaxElement::linearUpperBound( const Vector<double> &inputLb,
                                                     const Vector<double> &inputUb,
                                                     unsigned i )
    {
        Vector<double> lTilda;
        SoftmaxConstraint::xTilda( inputLb, inputUb[i], lTilda );
        lTilda[i] = 0;
        return 1 / SoftmaxConstraint::sumOfExponential( lTilda );
    }

    void DeepPolySoftmaxElement::log( const String &message )
    {
        if ( GlobalConfiguration::NETWORK_LEVEL_REASONER_LOGGING )
            printf( "DeepPolySoftmaxElement: %s\n", message.ascii() );
    }

} // namespace NLR
