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
#include <memory>

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
    // 1.
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    // // Vector<Interval> variables( layer->getSize(), Interval( -DBL_MAX, DBL_MAX ) );
    // std::unique_ptr<Interval[]> variables( new Interval[layer->getSize()] ); // Use unique_ptr to
    //                                                                          // manage memory
    //                                                                          // automatically

    Vector<Interval> variables = Vector<Interval>( layer->getSize(), Interval( 0, 0 ) );
    for ( unsigned int neuronId = 0; neuronId < layer->getSize(); ++neuronId )
    {
        variables[neuronId].setLowerBound( layer->getLb( neuronId ) );
        variables[neuronId].setUpperBound( layer->getUb( neuronId ) );
    }

    // 2.
    // std::unique_ptr<bool[]> verificationResults(
    //     new bool[_postConditions.size()] ); // Use unique_ptr to manage memory automatically
    Vector<bool> verificationResults;
    Interval coefficient = Interval( 0, 0 );
    for ( unsigned int orIndex = 0; orIndex < _postConditions.size(); ++orIndex )
    {
        bool andResult = false;
        for ( unsigned int andIndex = 0; andIndex < _postConditions[orIndex][layerId].size();
              ++andIndex )
        {
            Interval result = Interval( 0, 0 );
            for ( unsigned int i = 0; i < _postConditions[orIndex][layerId][andIndex].size(); ++i )
            {
                double coef = _postConditions[orIndex][layerId][andIndex][i];
                coefficient.setLowerBound( coef );
                coefficient.setUpperBound( coef );
                if ( coef < 0.0 && variables[i].getUpperBound() < 0.0 )
                    continue;
                else
                    result = result + ( coefficient * variables[i] );
            }
            double tempBias = _biasVectors[orIndex][layerId][andIndex];
            result = result + Interval( tempBias, tempBias );

            if ( result.getLowerBound() >= 0.0 )
                andResult = true;
            else
            {
                andResult = false;
                break;
            }
        }
        // verificationResults[orIndex] = andResult;
        verificationResults.append( andResult );
    }

    // return true: SAT
    // return false: UNSAT
    // The verification results are represented as "satisfying the post-conditions";
    // Thus, if one of the verification results is false, it means that "it cannot satify the
    // post-conditions, there is at least one counter-example";
    if ( verificationResults.exists( false ) )
        return true;
    // for ( unsigned int i = 0; i < _postConditions.size(); ++i )
    // {
    //     if ( !verificationResults[i] )
    //         return true; // UNSAT
    // }

    return false;
}

void BackPropagation::freeMemoryIfNeeded()
{
    // _postConditions.clear();
    // _biasVectors.clear();

    // Free the memory of the post-conditions.
    // Step 1: Free the deepest structures first (bottom-up cleanup)
    for ( auto &mapEntry : _postConditions )
    {
        for ( unsigned int i = 0; i < mapEntry.second.size(); ++i )
        {
            for ( unsigned int j = 0; j < mapEntry.second[i].size(); ++j )
            {
                // Clear inner vectors
                mapEntry.second[i][j].clear();
                // Replace with empty vector to force capacity reduction
                mapEntry.second[i][j] = Vector<double>();
            }
            // Clear middle vectors
            mapEntry.second[i].clear();
            mapEntry.second[i] = Vector<Vector<double>>();
        }
        // Clear outer vectors
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<Vector<double>>>();
    }

    // Similar for bias vectors - clean from innermost to outermost
    for ( auto &mapEntry : _biasVectors )
    {
        for ( unsigned int i = 0; i < mapEntry.second.size(); ++i )
        {
            // Clear each vector
            mapEntry.second[i].clear();
            mapEntry.second[i] = Vector<double>();
        }
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<double>>();
    }

    // Step 2: Now clear the maps themselves
    _postConditions.clear();
    _biasVectors.clear();

    // Step 3: Replace with empty maps to force capacity reduction
    _postConditions = Map<unsigned int, Vector<Vector<Vector<double>>>>();
    _biasVectors = Map<unsigned int, Vector<Vector<double>>>();
}

void BackPropagation::build( const Query &inputQuery,
                             const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                             const Preprocessor &preprocessor )
{
    _initPostConditions( inputQuery, _networkLevelReasoner, preprocessor );
    _generateNewPostConditions( inputQuery, _networkLevelReasoner );
    std::reverse( _hasPostConditions.begin(), _hasPostConditions.end() );
    // create array of Interval objects to store the value later.
    // _variables = Vector<Interval>( _networkLevelReasoner.getMaxLayerSize(),
    //                                Interval( 0, 0 ) ); // Initialize with
    //                                                    // 0 and 0
    //                                                    // to represent all
    //                                                    // possible values of
    //                                                    // the neurons in the
    //                                                    // network.
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
    Vector<double> emptyBiasVector( 1, 0 );
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
            Vector<Vector<double>> outputLayerPostCondition;
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

                outputLayerPostCondition.append( std::move( postCondition ) );
            }
            // this disjunctive constraint is a post-condition;
            _postConditions[_numberOfOrConditions].append( std::move( outputLayerPostCondition ) );
            _biasVectors[_numberOfOrConditions].append( std::move( emptyBiasVector ) );
            _numberOfOrConditions++;
        }
    }

    // update the flag to indicate that the post-conditions are disjunctive in the given property
    // file.
    _isDisjunctivePostCondition = _numberOfOrConditions > 0 ? true : false;
    if ( _isDisjunctivePostCondition )
        // When we generate the post-conditions, we have to +1 in the for...loop.
        _numberOfOrConditions--;
    else
    {
        Vector<Vector<double>> outputLayerPostCondition;
        for ( const auto &eq : inputQuery.getEquations() )
        {
            List<unsigned int> participatingVariables = eq.getListParticipatingVariables();
            Vector<double> postCondition( numberOfOutputNeurons, 0 );
            unsigned int numberOfOutputVariables = 0;
            for ( const unsigned int &pv : participatingVariables )
            {
                if ( givenOutputVariables.exists( pv ) )
                    numberOfOutputVariables++;
            }
            if ( numberOfOutputVariables != participatingVariables.size() )
                continue;

            for ( const unsigned int &pv : participatingVariables )
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
        _biasVectors[_numberOfOrConditions].append( std::move( emptyBiasVector ) );
    }

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
    const Map<unsigned int, NLR::Layer *> _layerIndexToLayer =
        _networkLevelReasoner.getLayerIndexToLayer();

    Vector<Vector<double>> newPostConditions;
    Vector<double> newPostCondition;
    Vector<double> newBiasVector;
    Vector<Vector<double>> emptyPostConditions( 1, Vector<double>() );
    Vector<double> emptyBiasVector( 1, 0 );
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
        for ( int index = numberOfLayers - 1; index > 0; --index )
        {
            NLR::Layer *currentLayer = _layerIndexToLayer[index];
            // const NLR::Layer &currentLayer = _networkLevelReasoner.getLayer( index );
            const NLR::Layer::Type layerType = currentLayer->getLayerType();

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
                //
                // The postconditions for the input layer will be generated from
                // NLR::Layer::Type::WEIGHTED_SUM
                //
                // Vector<Vector<double>> theLastPostConditions = _postConditions[i][0];
                // Vector<double> theLastBiasVector = _biasVectors[i][0];
                // _postConditions[i].insertHead( theLastPostConditions );
                // _biasVectors[i].insertHead( theLastBiasVector );

                // if ( i == 0 )
                // {
                //     if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                //         _hasPostConditions.push_back( true );
                //     else
                //         _hasPostConditions.push_back( false );
                // }

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
                Vector<Vector<double>> theLastPostConditions = _postConditions[i][0];
                Vector<double> theLastBiasVector = _biasVectors[i][0];
                _postConditions[i].insertHead( theLastPostConditions );
                _biasVectors[i].insertHead( theLastBiasVector );

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
                    newPostConditions = Vector<Vector<double>>();
                    newBiasVector = Vector<double>();

                    Vector<Vector<double>> theLastPostConditions = _postConditions[i][0];
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
                                    newPostCondition = Vector<double>( sourceLayerSize, 0 );

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
                        newPostConditions.append( newPostCondition );
                        newBiasVector.append( bias );
                    }
                    // Add the new post-condition to the list of post-conditions.
                    _postConditions[i].insertHead( newPostConditions );
                    _biasVectors[i].insertHead( newBiasVector );
                    countAddedPostConditions++;

                    if ( i == 0 )
                        _hasPostConditions.push_back( true );
                }
                else
                {
                    _postConditions[i].insertHead( emptyPostConditions );
                    _biasVectors[i].insertHead( emptyBiasVector );

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
