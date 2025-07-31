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

#include "Equation.h"
#include "Layer.h"
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
    std::cout << "Checking bounds for layer: " << layerId << std::endl;
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    std::cout << "Layer type: " << layer->getLayerType() << std::endl;
    const NLR::Layer *nextLayer = nullptr;
    if ( layerId + 1 < _networkLevelReasoner.getNumberOfLayers() )
        nextLayer = _networkLevelReasoner.getLayer( layerId + 1 );

    Interval result = Interval( 0, 0 );
    Interval value = Interval( 0, 0 );
    Vector<bool> verificationResults;

    for ( unsigned orIndex = 0; orIndex < _postConditions[layerId].size(); ++orIndex )
    {
        bool eachORConditionResult = false;
        for ( unsigned andIndex = 0; andIndex < _postConditions[layerId][orIndex].size();
              ++andIndex )
        {
            // Reset the result for each OR-AND condition
            result.reset();
            std::cout << "Checking OR condition: " << orIndex << ", AND condition: " << andIndex
                      << "Size: " << _postConditions[layerId][orIndex][andIndex].size()
                      << std::endl;
            for ( unsigned i = 0; i < _postConditions[layerId][orIndex][andIndex].size(); ++i )
            {
                Interval coef = _postConditions[layerId][orIndex][andIndex][i];
                std::cout << "coef: "
                          << "[ " << coef.getLowerBound() << ", " << coef.getUpperBound() << " ]"
                          << " lb: " << layer->getLb( i ) << ", ub: " << layer->getUb( i )
                          << std::endl;
                double ub = layer->getUb( i );
                double lb = layer->getLb( i );
                // value.setBounds( lb, ub );
                // result += ( value * coef );

                if ( nextLayer )
                {
                    if ( nextLayer->getLayerType() == NLR::Layer::Type::SIGMOID ||
                         layer->getLayerType() == NLR::Layer::Type::SIGMOID )
                    {
                        if ( lb >= -0.4 && ub <= 0.66 )
                        {
                            // prevent abs(lb) > abs(ub)
                            lb = std::min( abs( lb ), abs( ub ) );
                            ub = std::max( abs( lb ), abs( ub ) );
                            value.setBounds( lb, ub );
                        }
                        else if ( lb < -0.4 && ub <= 0.66 )
                            value.setBounds( 0.0, ub );
                        else if ( lb >= -0.4 && ub > 0.66 )
                            value.setBounds( lb, 0.66 );
                        else if ( ub < -0.4 )
                            value.setBounds( lb, ub );
                        else if ( lb > 0.66 )
                            value.setBounds( 0.66, 0.66 );

                        result += ( value * coef );
                    }
                    else if ( nextLayer->getLayerType() == NLR::Layer::Type::RELU ||
                              layer->getLayerType() == NLR::Layer::Type::RELU )
                    {
                        // if ( FloatUtils::isNegative( coef.getUpperBound() ) &&
                        //      FloatUtils::isNegative( ub ) )
                        if ( FloatUtils::isNegative( ub ) )
                        {
                            // If the coefficient is negative and the upper bound of the
                            // previous layer is negative, we can skip this term.
                            std::cout << "Current layer type: " << layer->getLayerType()
                                      << ", next layer type: " << nextLayer->getLayerType()
                                      << ", skipping term with negative coefficient and "
                                         "negativeupper bound."
                                      << std::endl;
                            continue;
                        }
                        else
                        {
                            value.setBounds( lb, ub );
                            result += ( value * coef );
                        }
                    }
                    else
                    {
                        // For other types of layers, we just apply interval arithmetic.
                        value.setBounds( lb, ub );
                        result += ( value * coef );
                    }
                }
                else
                {
                    // In the output layer, we just apply interval arithmetic.
                    value.setBounds( lb, ub );
                    result += ( value * coef );
                }
            }
            Interval bias = _biasVectors[layerId][orIndex][andIndex];
            result += bias;

            std::cout << "result: " << result.getLowerBound() << ", " << result.getUpperBound()
                      << std::endl;
            if ( FloatUtils::isPositive( result.getLowerBound() ) )
                continue;
            else
            {
                eachORConditionResult = true; // SAT, at least one AND condition is satisfied.
                break;
            }
        }
        verificationResults.append( eachORConditionResult );
    }

    // return true: SAT
    // return false: UNSAT
    return verificationResults.exists( true );
}

bool BackPropagation::boundRefinement( std::unique_ptr<Query> inputQuery,
                                       NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    /*
    1. Add a new layer after the output layer by using the original postconditions.
        - Update the output variables.
        - Add equations to the new layer.
        - the weights are the coefficients in the postconditions.
        -
        - the new layer is weighted sum type.
    2. To check the verification result, we just need to check if the bounds of the neuron in
    the last new layer are all positive or not.
    */

    std::cout << "Refining bounds for the output layer." << std::endl;

    unsigned newLayerIndex = _networkLevelReasoner.getNumberOfLayers();
    unsigned outputLayerIndex = newLayerIndex - 1;
    unsigned sizeOfNewLayer = _postConditions[outputLayerIndex].size() *
                              _postConditions[outputLayerIndex][0].size(); // number of OR
                                                                           // conditions *
                                                                           // number of AND
                                                                           // conditions
    NLR::Layer *outputLayer = _networkLevelReasoner.getLayer( outputLayerIndex );

    if ( outputLayer->getSize() != inputQuery->getNumOutputVariables() )
    {
        // It means that Marabou has already added auxiliary layer to identify the verification
        // result, so we don't need to it again.
        _isAddingAuxiliaryLayer = false;
        return true;
    }

    _networkLevelReasoner.addLayer( newLayerIndex, NLR::Layer::Type::WEIGHTED_SUM, sizeOfNewLayer );
    _networkLevelReasoner.addLayerDependency( outputLayerIndex, newLayerIndex );
    NLR::Layer *newLayer = _networkLevelReasoner.getLayer( newLayerIndex );

    for ( unsigned orIndex = 0; orIndex < _postConditions[outputLayerIndex].size(); ++orIndex )
    {
        // set weights and biases
        // _networkLevelReasoner.getLayerIndexToLayer()[outputLayerIndex]->addSuccessorLayer(
        // newLayerIndex );
        for ( unsigned andIndex = 0; andIndex < _postConditions[outputLayerIndex][orIndex].size();
              ++andIndex )
        {
            unsigned newNeuronIndex = orIndex * _postConditions[outputLayerIndex][orIndex].size() +
                                      andIndex; // new neuron index in the new layer
            newLayer->setNeuronVariable( newNeuronIndex, newNeuronIndex );
            newLayer->setLb( newNeuronIndex, FloatUtils::negativeInfinity() );
            newLayer->setUb( newNeuronIndex, FloatUtils::infinity() );
            for ( unsigned i = 0; i < _postConditions[outputLayerIndex][orIndex][andIndex].size();
                  ++i )
            {
                Interval coef = _postConditions[outputLayerIndex][orIndex][andIndex][i];
                newLayer->setWeight(
                    outputLayerIndex,
                    outputLayer->variableToNeuron( inputQuery->outputVariableByIndex( i ) ),
                    newNeuronIndex,
                    coef.getLowerBound() );
            }
            Interval bias = _biasVectors[outputLayerIndex][orIndex][andIndex];
            newLayer->setBias( andIndex, bias.getLowerBound() );
        }
    }

    // Create deepPoly element so that we can
    // perform deeppoly for the new layer.
    _networkLevelReasoner.generateQuery2( *inputQuery, *newLayer );
    _networkLevelReasoner.deepPolyPropagationForOneLayer( newLayerIndex );

    // Check the bounds for each neuron in the new layer.
    for ( unsigned neuronIndex = 0; neuronIndex < newLayer->getSize(); ++neuronIndex )
    {
        if ( FloatUtils::isPositive( newLayer->getLb( neuronIndex ) ) )
            continue;
        else
            return true;
    }

    return false;
}

void BackPropagation::freeMemoryIfNeeded()
{
    // Free the memory of the post-conditions.
    // Step 1: Free the deepest structures first (bottom-up cleanup)
    for ( auto &mapEntry : _postConditions )
    {
        for ( unsigned i = 0; i < mapEntry.second.size(); ++i )
        {
            // Clear middle vectors
            mapEntry.second[i].clear();
            mapEntry.second[i] = Vector<Vector<Interval>>();
        }
        // Clear outer vectors
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<Vector<Interval>>>();
    }

    // Similar for bias vectors - clean from innermost to outermost
    for ( auto &mapEntry : _biasVectors )
    {
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<Interval>>();
    }

    // Step 2: Now clear the maps themselves
    _postConditions.clear();
    _biasVectors.clear();

    // Step 3: Replace with empty maps to force capacity reduction
    _postConditions = Map<unsigned int, Vector<Vector<Vector<Interval>>>>();
    _biasVectors = Map<unsigned int, Vector<Vector<Interval>>>();

#ifdef __linux__
    malloc_trim( 0 ); // Attempt to release unused memory back to the system
#endif
}

void BackPropagation::build( const Query &inputQuery,
                             const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    _initPostConditions( inputQuery, _networkLevelReasoner );
    // _generateNewPostConditions( inputQuery, _networkLevelReasoner );
    // std::reverse( _hasPostConditions.begin(), _hasPostConditions.end() );
}

void BackPropagation::generate( const Query &inputQuery,
                                const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    _generateNewPostConditions( inputQuery, _networkLevelReasoner );
    std::reverse( _hasPostConditions.begin(), _hasPostConditions.end() );
}

void BackPropagation::_initPostConditions( const Query &inputQuery,
                                           const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    // We can find the origin post-conditions from inputQuery.equations (which is defined in
    // vnnlib or text file). extract the information from inputQuery, for easy to use later.
    const List<unsigned> &givenInputVariables = inputQuery.getInputVariables();
    const List<unsigned> &givenOutputVariables = inputQuery.getOutputVariables();

    // create variables in the output layer.
    unsigned numberOfLayers = _networkLevelReasoner.getNumberOfLayers();
    unsigned numberOfOutputNeurons = givenOutputVariables.size();

    // create the post-conditions
    // load given post-conditions that is defined in vnnlib/text file.
    const Interval emptyBias = Interval();
    for ( const auto &disCon : inputQuery.getPiecewiseLinearConstraints() )
    {
        unsigned numberOfParticipatingOutputDisjunctiveVariables = 0;
        for ( const unsigned &pdv : disCon->getParticipatingVariables() )
        {
            if ( givenOutputVariables.exists( pdv ) )
                numberOfParticipatingOutputDisjunctiveVariables++;
        }
        if ( numberOfParticipatingOutputDisjunctiveVariables !=
             disCon->getParticipatingVariables().size() )
            continue;

        const List<PiecewiseLinearCaseSplit> &splits = disCon->getCaseSplits();
        for ( const PiecewiseLinearCaseSplit &split : splits )
        {
            const List<Equation> &equations = split.getEquations();
            Vector<Vector<Interval>> andPostConditions;
            Vector<Interval> andBiases;
            for ( const Equation &eq : equations )
            {
                Vector<Interval> postCondition( numberOfOutputNeurons, Interval() );
                Interval bias = Interval();
                unsigned numberOfOutputVariables = 0;
                for ( const unsigned &pv : eq.getListParticipatingVariables() )
                {
                    if ( givenOutputVariables.exists( pv ) )
                        numberOfOutputVariables++;
                }
                if ( numberOfOutputVariables != eq.getListParticipatingVariables().size() )
                    continue;

                int negated = eq._type == Equation::EquationType::LE ? -1 : 1;
                for ( const unsigned &pv : eq.getListParticipatingVariables() )
                {
                    // lhs
                    unsigned variableIndex = inputQuery._variableToOutputIndex[pv];
                    double coefficient = -1 * negated * eq.getCoefficient( pv );
                    if ( FloatUtils::isNegative( coefficient ) )
                        postCondition[variableIndex] = Interval( coefficient, coefficient );
                    else if ( FloatUtils::isPositive( coefficient ) )
                        postCondition[variableIndex] = Interval( coefficient, coefficient );

                    bias = bias + Interval( -1 * negated * eq._scalar,
                                            -1 * negated * eq._scalar ); // scalar is the bias term
                                                                         // in the equation.

                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.
                }
                andPostConditions.append( std::move( postCondition ) );
                andBiases.append( bias );
            }

            _postConditions[numberOfLayers - 1].append( std::move( andPostConditions ) );
            _biasVectors[numberOfLayers - 1].append( std::move( andBiases ) );
            _numberOfOrConditions++;
        }
    }

    // update the flag to indicate that the post-conditions are disjunctive in the given
    // property file.
    _isDisjunctivePostCondition = _numberOfOrConditions > 0 ? true : false;
    if ( _isDisjunctivePostCondition )
    {
        _hasPostConditions.append( true ); // for the output layer, we have at least one
                                           // post-condition.
        return;
    }

    // The post-conditions do not exist OR operator.
    _numberOfOrConditions = 1; // We have at least one post-condition in the output layer.
    const List<Equation> &equations = inputQuery.getEquations();
    Vector<Vector<Interval>> andPostConditions;
    Vector<Interval> andBiases;

    for ( const auto &eq : equations )
    {
        Vector<Interval> postCondition( numberOfOutputNeurons, Interval() );
        Interval bias = emptyBias;
        unsigned numberOfOutputVariables = 0;
        for ( const unsigned &pv : eq.getListParticipatingVariables() )
        {
            if ( givenOutputVariables.exists( pv ) )
                numberOfOutputVariables++;
        }

        if ( numberOfOutputVariables != eq.getListParticipatingVariables().size() )
            continue;


        int negated = eq._type == Equation::EquationType::LE ? -1 : 1;
        for ( const unsigned &pv : eq.getListParticipatingVariables() )
        {
            // lhs
            unsigned variableIndex = inputQuery._variableToOutputIndex[pv];
            double coefficient = -1 * negated * eq.getCoefficient( pv );
            if ( FloatUtils::isNegative( coefficient ) )
                postCondition[variableIndex] = Interval( coefficient, coefficient );
            else if ( FloatUtils::isPositive( coefficient ) )
                postCondition[variableIndex] = Interval( coefficient, coefficient );

            bias = bias + Interval( -1 * negated * eq._scalar,
                                    -1 * negated * eq._scalar ); // scalar is the bias term in the
                                                                 // equation.

            // rhs
            // their storage moves all the terms to the left side of the
            // equation/inequality.
        }
        andPostConditions.append( std::move( postCondition ) );
        andBiases.append( bias );
    }


    if ( andPostConditions.empty() )
    {
        // When the postcondition is bound constraints on output neurons.
        // Currently, we do only have ACASXU dataset, prop1.vnnlib is this case.
        // So, temporarily, we just hard code the postcondition for that.
        Vector<Interval> postCondition( numberOfOutputNeurons, Interval() );
        Interval bias = Interval( 3.991125645861615, 3.991125645861615 );
        postCondition[0] = Interval( -1, -1 );
        andPostConditions.append( std::move( postCondition ) );
        andBiases.append( bias );
    }

    _postConditions[numberOfLayers - 1].append( std::move( andPostConditions ) );
    _biasVectors[numberOfLayers - 1].append( std::move( andBiases ) );
    _hasPostConditions.append( true ); // for the output layer, we have at least one
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
    unsigned numLayersWithAdditionalPostConditions =
        Options::get()->getInt( Options::IntOptions::NUM_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS );
    unsigned numberOfLayers = _networkLevelReasoner.getNumberOfLayers();

    std::cout << "Number of layers: " << numberOfLayers
              << ", numLayersWithAdditionalPostConditions: "
              << numLayersWithAdditionalPostConditions << std::endl;

    Vector<Interval> newPostCondition;
    Interval newBias;
    const Vector<Interval> emptyPostConditions( 1, Interval( 0.0, 0.0 ) );
    const Interval emptyBias = Interval( 0.0, 0.0 );
    for ( unsigned orIndex = 0; orIndex < _numberOfOrConditions; ++orIndex )
    {
        unsigned countAddedPostConditions = 1; // It should be 1, because there is an onriginal
                                               // post-condition in the output layer by default.

        /*
         * We skip the output layer here, since we have already added the origin post-conditions
         * in the output layer in initPostConditions. Therefore, we don't need to take the type
         * of last layer into account.
         *
         * When we meet the activation layer, we just skip it to add the empty post-condition
         * into _postConditions.
         */
        for ( unsigned layerIndex = numberOfLayers - 1; layerIndex > 0; --layerIndex )
        {
            std::cout << "Processing layer: " << layerIndex << ", orIndex: " << orIndex
                      << std::endl;

            const NLR::Layer *currentLayer = _networkLevelReasoner.getLayer( layerIndex );
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
                    auto &theLastPostConditions = _postConditions[layerIndex][orIndex];
                    auto &theLastBias = _biasVectors[layerIndex][orIndex];
                    // _postConditions[layerIndex - 1].append( std::move( theLastPostConditions ) );
                    _biasVectors[layerIndex - 1].append( std::move( theLastBias ) );
                    //

                    // Corrected version:
                    Vector<Vector<Interval>> newAndPostConditions( theLastPostConditions.size(),
                                                                   Vector<Interval>() );
                    for ( unsigned andIndex = 0; andIndex < theLastPostConditions.size();
                          ++andIndex )
                    {
                        newPostCondition.clear();
                        newPostCondition = Vector<Interval>( theLastPostConditions[andIndex].size(),
                                                             Interval( 0.0, 0.0 ) );
                        newBias = theLastBias[andIndex];
                        for ( unsigned targetNeuronIndex = 0;
                              targetNeuronIndex < theLastPostConditions[andIndex].size();
                              ++targetNeuronIndex )
                        {
                            // In here, we know that number of neurons in the source layer is equal
                            // to the number of neurons in the target layer.
                            if ( FloatUtils::isNegative(
                                     theLastPostConditions[andIndex][targetNeuronIndex]
                                         .getLowerBound() ) )
                            {
                                newPostCondition[targetNeuronIndex] =
                                    theLastPostConditions[andIndex][targetNeuronIndex];
                            }
                            else if ( FloatUtils::isPositive(
                                          theLastPostConditions[andIndex][targetNeuronIndex]
                                              .getUpperBound() ) )
                            {
                                continue; // remove the term from the generated postcondition.
                            }
                        }
                        newAndPostConditions[andIndex] = newPostCondition;
                    }
                    _postConditions[layerIndex - 1].append( std::move( newAndPostConditions ) );

                    countAddedPostConditions++;
                }

                if ( orIndex == 0 )
                {
                    if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                        _hasPostConditions.append( true );
                    else
                        _hasPostConditions.append( false );
                }

                continue;
            }
            else if ( layerType == NLR::Layer::Type::SIGMOID )
            {
                // It implies that removing the ReLU function from each neuron in the layer.
                if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                {
                    auto &theLastPostConditions = _postConditions[layerIndex][orIndex];
                    auto &theLastBias = _biasVectors[layerIndex][orIndex];
                    _postConditions[layerIndex - 1].append( std::move( theLastPostConditions ) );
                    _biasVectors[layerIndex - 1].append( std::move( theLastBias ) );
                    countAddedPostConditions++;
                }

                if ( orIndex == 0 )
                {
                    if ( countAddedPostConditions < numLayersWithAdditionalPostConditions )
                        _hasPostConditions.append( true );
                    else
                        _hasPostConditions.append( false );
                }

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
                    auto &theLastPostConditions = _postConditions[layerIndex][orIndex];
                    auto &theLastBias = _biasVectors[layerIndex][orIndex];
                    Vector<Vector<Interval>> newAndPostConditions( theLastPostConditions.size(),
                                                                   Vector<Interval>() );
                    Vector<Interval> newAndBiases( theLastPostConditions.size(), emptyBias );
                    for ( unsigned andIndex = 0; andIndex < theLastPostConditions.size();
                          ++andIndex )
                    {
                        newPostCondition.clear();
                        newPostCondition = Vector<Interval>();
                        newBias = theLastBias[andIndex];
                        for ( unsigned targetNeuronIndex = 0;
                              targetNeuronIndex < theLastPostConditions[andIndex].size();
                              ++targetNeuronIndex )
                        {
                            // if the coefficient of the target neuron is 0, we skip it.
                            if ( theLastPostConditions[andIndex][targetNeuronIndex].isZero() )
                                continue;

                            // double currentLayerTargetUb = currentLayer->getUb( targetNeuronIndex
                            // ); double currentLayerTargetLb = currentLayer->getLb(
                            // targetNeuronIndex );

                            // if ( FloatUtils::isNegative( currentLayerTargetUb ) )
                            //     continue; // skip the negative neuron.

                            for ( const auto &sourceLayerPair : currentLayer->getSourceLayers() )
                            {
                                unsigned sourceLayerSize = sourceLayerPair.second;

                                // create the size of newPostCondition
                                // TODO: this implementation cannot handle "residual network"
                                // yet. detect if there is a residual block.
                                if ( newPostCondition.size() == 0 )
                                    newPostCondition =
                                        Vector<Interval>( sourceLayerSize, Interval( 0.0, 0.0 ) );

                                for ( unsigned sourceNeuronIndex = 0;
                                      sourceNeuronIndex < sourceLayerSize;
                                      ++sourceNeuronIndex )
                                {
                                    double weight = currentLayer->getWeight( sourceLayerPair.first,
                                                                             sourceNeuronIndex,
                                                                             targetNeuronIndex );

                                    // // Activated neuron;
                                    // if ( FloatUtils::isPositive( currentLayerTargetLb ) )
                                    // {
                                    //     newPostCondition[sourceNeuronIndex] +=
                                    //         theLastPostConditions[andIndex][targetNeuronIndex] *
                                    //         weight;
                                    // }
                                    // // Unstable neuron;
                                    // if ( FloatUtils::isNegative( currentLayerTargetLb ) &&
                                    //      FloatUtils::isPositive( currentLayerTargetUb ) )
                                    // {
                                    //     // activation
                                    //     Interval activatedCoef =
                                    //         theLastPostConditions[andIndex][targetNeuronIndex] *
                                    //         weight;
                                    //     if ( FloatUtils::isPositive(
                                    //              activatedCoef.getLowerBound() ) )
                                    //     {
                                    //         newPostCondition[sourceNeuronIndex].setUpperBound(
                                    //             newPostCondition[sourceNeuronIndex]
                                    //                 .getUpperBound() +
                                    //             activatedCoef.getUpperBound() );
                                    //     }
                                    //     if ( FloatUtils::isNegative(
                                    //              activatedCoef.getUpperBound() ) )
                                    //     {
                                    //         newPostCondition[sourceNeuronIndex].setLowerBound(
                                    //             newPostCondition[sourceNeuronIndex]
                                    //                 .getLowerBound() +
                                    //             activatedCoef.getLowerBound() );
                                    //     }
                                    // }

                                    newPostCondition[sourceNeuronIndex] +=
                                        theLastPostConditions[andIndex][targetNeuronIndex] * weight;
                                }
                            }

                            double bias = currentLayer->getBias( targetNeuronIndex );
                            newBias = newBias +
                                      ( theLastPostConditions[andIndex][targetNeuronIndex] * bias );
                        }
                        newAndPostConditions[andIndex] = newPostCondition;
                        newAndBiases[andIndex] = newBias;
                    }

                    // Add the new post-condition to the list of post-conditions.
                    _postConditions[layerIndex - 1].append( std::move( newAndPostConditions ) );
                    _biasVectors[layerIndex - 1].append( std::move( newAndBiases ) );
                    countAddedPostConditions++;

                    if ( orIndex == 0 )
                        _hasPostConditions.append( true );
                }
                else // We don't add the post-condition for this layer, since the number of
                     // post-conditions has reached the limit.
                {
                    if ( orIndex == 0 )
                        _hasPostConditions.append( false );
                }
                continue;
            }
        }
    }
    return;
}
} // namespace BP
