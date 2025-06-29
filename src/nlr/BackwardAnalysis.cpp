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

namespace BP {
BackPropagation::BackPropagation()
{
}

BackPropagation::~BackPropagation()
{
    freeMemoryIfNeeded();
}

bool BackPropagation::boundChecking( const Query &inputQuery,
                                     const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                     const unsigned int layerId )
{
    /*
    Doing bound checking to check if the bounds can satisfy the post-conditions;

    1. Transfer the set of variables (lb, ub) to be the set of Interval objects.
    2. Iterate through the set of post-conditions with the set of Interval objects.

    */
    // Map<std::string, Interval> variables;
    Map<unsigned int, Interval> variables;

    // 1.
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    for ( unsigned int neuronId = 0; neuronId < layer->getSize(); ++neuronId )
    {
        double lb = layer->getLb( neuronId );
        double ub = layer->getUb( neuronId );
        Interval interval = Interval( lb, ub );
        variables[neuronId] = interval;
    }

    // 2.
    Vector<bool> verificationResults;
    // for ( auto &orCondition : _postConditions )
    for ( unsigned int orIndex = 0; orIndex < _postConditions.size(); ++orIndex )
    {
        bool andResult = false;
        Vector<Vector<double>> andConstraints = _postConditions[orIndex][layerId];
        for ( unsigned int andIndex = 0; andIndex < andConstraints.size(); ++andIndex )
        {
            Interval result = Interval( 0, 0 );
            for ( unsigned int i = 0; i < andConstraints[andIndex].size(); ++i )
            {
                Interval coefficient =
                    Interval( andConstraints[andIndex][i], andConstraints[andIndex][i] );
                if ( coefficient.getUpperBound() < 0 && variables[i].getUpperBound() < 0 )
                    continue;
                else
                    result += coefficient * variables[i];
            }
            result += Interval( _biasVectors[orIndex][layerId][andIndex],
                                _biasVectors[orIndex][layerId][andIndex] );

            if ( result.getLowerBound() >= 0.0 )
            {
                andResult = true;
            }
            else
            {
                andResult = false;
                break;
            }
        }
        verificationResults.append( andResult );
    }

    // return true: SAT
    // return false: UNSAT
    // The verification results are represented as "satisfying the post-conditions";
    // Thus, if one of the verification results is false, it means that "it cannot satify the
    // post-conditions, there is at least one counter-example";
    bool result = false;
    if ( verificationResults.exists( false ) )
        result = true;

    return result;
}

void BackPropagation::freeMemoryIfNeeded()
{
    _postConditions.clear();
    _biasVectors.clear();
}

void BackPropagation::build( const Query &inputQuery,
                             const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                             const Preprocessor &preprocessor )
{
    _initPostConditions( inputQuery, _networkLevelReasoner, preprocessor );
    _generateNewPostConditions( inputQuery, _networkLevelReasoner );
}


void BackPropagation::_initPostConditions( const Query &inputQuery,
                                           const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                           const Preprocessor &preprocessor )
{
    // We can find the origin post-conditions from inputQuery.equations (which is defined in vnnlib
    // or text file).
    // extract the information from inputQuery, for easy to use later.
    List<unsigned int> givenInputVariables = inputQuery.getInputVariables();
    List<unsigned int> givenOutputVariables = inputQuery.getOutputVariables();

    // create variables in the output layer.
    unsigned int numberOfLayers = _networkLevelReasoner.getNumberOfLayers();
    NLR::Layer outputLayer = _networkLevelReasoner.getLayer( numberOfLayers - 1 );
    unsigned int numberOfOutputNeurons = outputLayer.getSize();

    // create the post-conditions
    // load given post-conditions that is defined in vnnlib/text file.
    List<Equation> equations = inputQuery.getEquations();
    List<PiecewiseLinearConstraint *> disjunctiveConstraints =
        inputQuery.getPiecewiseLinearConstraints();

    for ( auto &disCon : disjunctiveConstraints )
    {
        List<unsigned int> participatingDisjunctiveVariables = disCon->getParticipatingVariables();
        unsigned int numberOfParticipatingOutputDisjunctiveVariables = 0;
        for ( unsigned int &pdv : participatingDisjunctiveVariables )
        {
            if ( givenOutputVariables.exists( pdv ) )
                numberOfParticipatingOutputDisjunctiveVariables++;
        }
        if ( numberOfParticipatingOutputDisjunctiveVariables !=
             participatingDisjunctiveVariables.size() )
            continue;

        List<PiecewiseLinearCaseSplit> splits = disCon->getCaseSplits();
        for ( auto &split : splits )
        {
            List<Equation> splitEquations = split.getEquations();
            Vector<Vector<double>> outputLayerPostCondition;
            for ( Equation &eq : splitEquations )
            {
                List<unsigned int> participatingVariables = eq.getListParticipatingVariables();
                Vector<double> postCondition( numberOfOutputNeurons, 0 );
                unsigned int numberOfOutputVariables = 0;
                for ( unsigned int &pv : participatingVariables )
                {
                    if ( givenOutputVariables.exists( pv ) )
                        numberOfOutputVariables++;
                }
                if ( numberOfOutputVariables != participatingVariables.size() )
                    continue;

                for ( auto &pv : participatingVariables )
                {
                    // lhs
                    unsigned int variableIndex = inputQuery._variableToOutputIndex[pv];
                    double coefficient = 1 * eq.getCoefficient( pv );
                    postCondition[variableIndex] = coefficient;

                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.
                }

                outputLayerPostCondition.append( std::move( postCondition ) );
            }
            // this disjunctive constraint is a post-condition;
            _postConditions[_numberOfOrConditions].append( std::move( outputLayerPostCondition ) );
            _biasVectors[_numberOfOrConditions].append( Vector<double>( 1, 0 ) );
            _numberOfOrConditions++;
        }
    }

    // update the flag to indicate that the post-conditions are disjunctive in the given property
    // file.
    _isDisjunctivePostCondition = _numberOfOrConditions > 0 ? true : false;
    if ( _isDisjunctivePostCondition )
        _numberOfOrConditions--;
    else
    {
        Vector<Vector<double>> outputLayerPostCondition;
        for ( auto &eq : equations )
        {
            List<unsigned int> participatingVariables = eq.getListParticipatingVariables();
            Vector<double> postCondition( numberOfOutputNeurons, 0 );
            unsigned int numberOfOutputVariables = 0;
            for ( unsigned int &pv : participatingVariables )
            {
                if ( givenOutputVariables.exists( pv ) )
                    numberOfOutputVariables++;
            }
            if ( numberOfOutputVariables != participatingVariables.size() )
                continue;

            for ( unsigned int &pv : participatingVariables )
            {
                // lhs
                unsigned int variableIndex = inputQuery._variableToOutputIndex[pv];
                double coefficient = 1 * eq.getCoefficient( pv );
                postCondition[variableIndex] = coefficient;

                // rhs
                // their storage moves all the terms to the left hand side of the
                // equation/inequality.
            }
            outputLayerPostCondition.append( std::move( postCondition ) );
        }
        _postConditions[_numberOfOrConditions].append( std::move( outputLayerPostCondition ) );
        _biasVectors[_numberOfOrConditions].append( Vector<double>( 1, 0 ) );
    }

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
    Map<unsigned int, NLR::Layer *> _layerIndexToLayer =
        _networkLevelReasoner.getLayerIndexToLayer();

    Vector<Vector<double>> newPostConditions;
    Vector<double> newPostCondition;
    Vector<double> newBiasVector;
    for ( unsigned int i = 0; i < _numberOfOrConditions + 1; ++i )
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
        for ( int index = numberOfLayers - 2; index >= 0; --index )
        {
            NLR::Layer *currentLayer = _layerIndexToLayer[index];
            NLR::Layer::Type layerType = currentLayer->getLayerType();

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
                const Vector<Vector<double>> &theLastPostConditions = _postConditions[i][0];
                _postConditions[i].insertHead( std::move( theLastPostConditions ) );
                _biasVectors[i].insertHead( Vector<double>( 1, 0 ) );
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
                const Vector<Vector<double>> &theLastPostConditions = _postConditions[i][0];
                _postConditions[i].insertHead( std::move( theLastPostConditions ) );
                _biasVectors[i].insertHead( Vector<double>( 1, 0 ) );
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
                    newPostConditions = Vector<Vector<double>>();
                    newBiasVector = Vector<double>();

                    const Vector<Vector<double>> &theLastPostConditions = _postConditions[i][0];
                    for ( auto &postCondition : theLastPostConditions )
                    {
                        newPostCondition = Vector<double>();
                        double bias = 0;
                        for ( unsigned int targetNeuronIndex = 0;
                              targetNeuronIndex < postCondition.size();
                              ++targetNeuronIndex )
                        {
                            // if the coefficient of the target neuron is 0, we skip it.
                            if ( postCondition[targetNeuronIndex] == 0 )
                                continue;

                            for ( const auto &sourceLayerPair : currentLayer->getSourceLayers() )
                            {
                                unsigned int sourceLayerSize = sourceLayerPair.second;

                                // create the size of newPostCondition
                                if ( newPostCondition.size() == 0 )
                                {
                                    for ( unsigned int _ = 0; _ < sourceLayerSize; ++_ )
                                    {
                                        newPostCondition.append( 0 );
                                    }
                                }

                                for ( unsigned int sourceNeuronIndex = 0;
                                      sourceNeuronIndex < sourceLayerSize;
                                      ++sourceNeuronIndex )
                                {
                                    double weight = currentLayer->getWeight( sourceLayerPair.first,
                                                                             sourceNeuronIndex,
                                                                             targetNeuronIndex );
                                    newPostCondition[sourceNeuronIndex] +=
                                        postCondition[targetNeuronIndex] * weight;
                                }
                            }
                            double tempBias = currentLayer->getBias( targetNeuronIndex );
                            bias += tempBias;
                        }

                        // Add the new postCondition to the list of postConditions
                        newPostConditions.append( std::move( newPostCondition ) );
                        newBiasVector.append( bias );
                    }
                    // Add the new post-condition to the list of post-conditions.
                    _postConditions[i].insertHead( std::move( newPostConditions ) );
                    _biasVectors[i].insertHead( std::move( newBiasVector ) );
                    countAddedPostConditions++;
                }
                else
                {
                    const Vector<Vector<double>> &theLastPostConditions = _postConditions[i][0];
                    _postConditions[i].insertHead( std::move( theLastPostConditions ) );
                    _biasVectors[i].insertHead( Vector<double>( 1, 0 ) );
                }
                continue;
            }
        }
    }
    return;
}
} // namespace BP
