/*********************                                                        */
/*! \file BackwardAnalysis.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Yi-Nung Tsao
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2024 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "BackwardAnalysis.h"

#include "NetworkLevelReasoner.h"

#include <float.h>
#include <malloc.h>
#include <memory>

namespace BP {
BackPropagation::BackPropagation()
{
}

BackPropagation::~BackPropagation()
{
    freeMemoryIfNeeded();
}

bool BackPropagation::boundChecking( const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                     const unsigned int layerId )
{
    /*

    Doing bound checking to check if the bounds can satisfy the post-conditions;
    Iterate through the set of post-conditions with the set of Interval objects.

    */
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    Interval result = Interval( 0, 0 );
    Interval value = Interval( 0, 0 );
    for ( unsigned int orIndex = 0; orIndex < _postConditions[layerId].size(); ++orIndex )
    {
        result.reset();
        for ( unsigned int i = 0; i < _postConditions[layerId][orIndex].size(); ++i )
        {
            double coef = _postConditions[layerId][orIndex][i];
            if ( coef < 0.0 && layer->getUb( i ) < 0.0 )
                continue;
            else
            {
                value.setBounds( layer->getLb( i ), layer->getUb( i ) );
                result += ( value * coef );
            }
        }
        double bias = _biasVectors[layerId][orIndex];
        result += bias;

        if ( result.getLowerBound() >= 0.0 )
            continue;
        else
            return true; // SAT
    }

    // return true: SAT
    // return false: UNSAT
    return false; // UNSAT
}

void BackPropagation::freeMemoryIfNeeded()
{
    // Free the memory of the post-conditions.
    // Step 1: Free the deepest structures first (bottom-up cleanup)
    for ( auto &mapEntry : _postConditions )
    {
        for ( unsigned int i = 0; i < mapEntry.second.size(); ++i )
        {
            // Clear middle vectors
            mapEntry.second[i].clear();
            mapEntry.second[i] = Vector<double>();
        }
        // Clear outer vectors
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<double>>();
    }

    // Similar for bias vectors - clean from innermost to outermost
    for ( auto &mapEntry : _biasVectors )
    {
        mapEntry.second.clear();
        mapEntry.second = Vector<double>();
    }

    // Step 2: Now clear the maps themselves
    _postConditions.clear();
    _biasVectors.clear();

    // Step 3: Replace with empty maps to force capacity reduction
    _postConditions = Map<unsigned int, Vector<Vector<double>>>();
    _biasVectors = Map<unsigned int, Vector<double>>();

#ifdef __linux__
    malloc_trim( 0 ); // Attempt to release unused memory back to the system
#endif
}

void BackPropagation::build( const Query &inputQuery,
                             const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                             const Preprocessor &preprocessor )
{
    _initPostConditions( inputQuery, _networkLevelReasoner, preprocessor );
    _generateNewPostConditions( inputQuery, _networkLevelReasoner );
    std::reverse( _hasPostConditions.begin(), _hasPostConditions.end() );
}


void BackPropagation::_initPostConditions( const Query &inputQuery,
                                           const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                           const Preprocessor &preprocessor )
{
    // We can find the origin post-conditions from inputQuery.equations (which is defined in vnnlib
    // or text file).
    // extract the information from inputQuery, for easy to use later.
    const List<unsigned int> &givenInputVariables = inputQuery.getInputVariables();
    const List<unsigned int> &givenOutputVariables = inputQuery.getOutputVariables();

    // create variables in the output layer.
    unsigned int numberOfLayers = _networkLevelReasoner.getNumberOfLayers();
    const NLR::Layer &outputLayer = _networkLevelReasoner.getLayer( numberOfLayers - 1 );
    unsigned int numberOfOutputNeurons = outputLayer.getSize();

    // create the post-conditions
    // load given post-conditions that is defined in vnnlib/text file.
    const double emptyBias = 0.0;
    for ( const auto &disCon : inputQuery.getPiecewiseLinearConstraints() )
    {
        unsigned int numberOfParticipatingOutputDisjunctiveVariables = 0;
        for ( const unsigned int &pdv : disCon->getParticipatingVariables() )
        {
            if ( givenOutputVariables.exists( pdv ) )
                numberOfParticipatingOutputDisjunctiveVariables++;
        }
        if ( numberOfParticipatingOutputDisjunctiveVariables !=
             disCon->getParticipatingVariables().size() )
            continue;

        for ( auto &split : disCon->getCaseSplits() )
        {
            // Vector<Vector<double>> outputLayerPostCondition;
            for ( const Equation &eq : split.getEquations() )
            {
                Vector<double> postCondition( numberOfOutputNeurons, 0 );
                unsigned int numberOfOutputVariables = 0;
                for ( const unsigned int &pv : eq.getListParticipatingVariables() )
                {
                    if ( givenOutputVariables.exists( pv ) )
                        numberOfOutputVariables++;
                }
                if ( numberOfOutputVariables != eq.getListParticipatingVariables().size() )
                    continue;

                for ( const unsigned int &pv : eq.getListParticipatingVariables() )
                {
                    // lhs
                    unsigned int variableIndex = inputQuery._variableToOutputIndex[pv];
                    double coefficient = 1 * eq.getCoefficient( pv );
                    postCondition[variableIndex] = coefficient;

                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.
                }

                // outputLayerPostCondition.append( std::move( postCondition ) );
                _postConditions[numberOfLayers - 1].append( std::move( postCondition ) );
                _biasVectors[numberOfLayers - 1].append( emptyBias );
            }
            _numberOfOrConditions++;
        }
    }

    // update the flag to indicate that the post-conditions are disjunctive in the given property
    // file.
    _isDisjunctivePostCondition = _numberOfOrConditions > 0 ? true : false;
    _hasPostConditions.push_back( true ); // for the output layer, we have at least one
                                          // post-condition.
    return;
}


void BackPropagation::_generateNewPostConditions(
    const Query &inputQuery,
    const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    /*
     * New implementation, we'll create the same size of _postConditions list as Layers.
     * When we meet activation function, we create the empty list add into _postConditions.
     * Otherwise, we follow the same rule as before.
     *
     * The benefit for this new implementation is that we don't need to care pcId,
     * we can just use layerId to access the _postConditions.
     * Then, we don't need to worrty if the network topology is changed.
     */
    int numLayersWithAdditionalPostConditions =
        Options::get()->getInt( Options::IntOptions::NUM_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS );
    unsigned int numberOfLayers = _networkLevelReasoner.getNumberOfLayers();

    Vector<double> newPostCondition;
    double newBias;
    const Vector<double> emptyPostConditions( 1, 0.0 );
    const double emptyBias = 0.0;
    for ( unsigned int i = 0; i < _numberOfOrConditions; ++i )
    {
        unsigned int countAddedPostConditions = 1; // It should be 1, because there is an onriginal
                                                   // post-condition in the output layer by default.

        /*
         * We skip the output layer here, since we have already added the origin post-conditions in
         * the output layer in initPostConditions. Therefore, we don't need to take the type of last
         * layer into account.
         *
         * When we meet the activation layer, we just skip it to add the empty post-condition into
         * _postConditions.
         */
        for ( int index = numberOfLayers - 1; index > 0; --index )
        {
            const NLR::Layer &currentLayer = _networkLevelReasoner.getLayer( index );
            const NLR::Layer::Type layerType = currentLayer.getLayerType();

            if ( layerType == NLR::Layer::Type::ABSOLUTE_VALUE )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::BILINEAR )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::INPUT )
            {
                // The postconditions for the input layer will be generated from
                // NLR::Layer::Type::WEIGHTED_SUM
                continue;
            }
            else if ( layerType == NLR::Layer::Type::LEAKY_RELU )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::MAX )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::RELU )
            {
                // It implies that removing the ReLU function from each neuron in the layer.
                if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                {
                    const Vector<double> &theLastPostConditions = _postConditions[index][i];
                    const double theLastBias = _biasVectors[index][i];
                    _postConditions[index - 1].append( std::move( theLastPostConditions ) );
                    _biasVectors[index - 1].append( theLastBias );
                }

                if ( i == 0 )
                {
                    if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                        _hasPostConditions.push_back( true );
                    else
                        _hasPostConditions.push_back( false );
                }

                continue;
            }
            else if ( layerType == NLR::Layer::Type::SIGMOID )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::SIGN )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::SOFTMAX )
            {
                continue;
            }
            else if ( layerType == NLR::Layer::Type::WEIGHTED_SUM )
            {
                if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                {
                    const Vector<double> &theLastPostCondition = _postConditions[index][i];

                    newPostCondition.clear();
                    newPostCondition = Vector<double>();
                    newBias = 0.0;
                    for ( unsigned int targetNeuronIndex = 0;
                          targetNeuronIndex < theLastPostCondition.size();
                          ++targetNeuronIndex )
                    {
                        // if the coefficient of the target neuron is 0, we skip it.
                        if ( theLastPostCondition[targetNeuronIndex] == 0 )
                            continue;

                        for ( const auto &sourceLayerPair : currentLayer.getSourceLayers() )
                        {
                            unsigned int sourceLayerSize = sourceLayerPair.second;

                            // create the size of newPostCondition
                            if ( newPostCondition.size() == 0 )
                                newPostCondition = Vector<double>( sourceLayerSize, 0 );

                            for ( unsigned int sourceNeuronIndex = 0;
                                  sourceNeuronIndex < sourceLayerSize;
                                  ++sourceNeuronIndex )
                            {
                                double weight = currentLayer.getWeight(
                                    sourceLayerPair.first, sourceNeuronIndex, targetNeuronIndex );
                                newPostCondition[sourceNeuronIndex] +=
                                    theLastPostCondition[targetNeuronIndex] * weight;
                            }
                        }
                        newBias += currentLayer.getBias( targetNeuronIndex );

                        // Add the new postCondition to the list of postConditions
                    }
                    // Add the new post-condition to the list of post-conditions.
                    _postConditions[index - 1].append( std::move( newPostCondition ) );
                    _biasVectors[index - 1].append( newBias );
                    countAddedPostConditions++;

                    if ( i == 0 )
                        _hasPostConditions.push_back( true );
                }
                else // We don't add the post-condition for this layer, since the number of
                     // post-conditions has reached the limit.
                {
                    if ( i == 0 )
                        _hasPostConditions.push_back( false );
                }
                continue;
            }
        }
    }
    return;
}
} // namespace BP
