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
#include "FloatUtils.h"
#include "GurobiWrapper.h"
#include "Layer.h"
#include "NetworkLevelReasoner.h"

#include <exception>
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
    Vector<bool> verificationResults;

    std::cout << "Bound checking at layer: " << layerId << std::endl;
    std::cout << "Layer type: " << layer->getLayerType() << std::endl;
    std::cout << _postConditions[layerId].size() << " OR conditions, "
              << _postConditions[layerId][0].size() << " AND conditions." << std::endl;
    for ( unsigned orIndex = 0; orIndex < _postConditions[layerId].size(); ++orIndex )
    {
        bool eachORConditionResult = false;
        for ( unsigned andIndex = 0; andIndex < _postConditions[layerId][orIndex].size();
              ++andIndex )
        {
            // Reset the result for each OR-AND condition
            result.reset();
            for ( unsigned i = 0; i < _postConditions[layerId][orIndex][andIndex].size(); ++i )
            {
                double coef = _postConditions[layerId][orIndex][andIndex][i];
                double ub = layer->getUb( i );
                double lb = layer->getLb( i );
                value.reset();
                // std::cout << "variable index: " << i << std::endl;
                // std::cout << "lb: " << lb << ", ub: " << ub << ", coef: " << coef << std::endl;

                if ( FloatUtils::isNegative( ub ) && FloatUtils::isNegative( coef ) )
                {
                    // If the coefficient is negative and the upper bound of the
                    // previous layer is negative, we can skip this term.
                    continue;
                }
                else
                {
                    value.setBounds( lb, ub );
                    result += ( value * coef );
                }

                // if ( layerId == 0 &&
                //      ( FloatUtils::isNegative( lb ) || FloatUtils::isNegative( ub ) ) )
                //     return true; // If input layer indcludes negative bounds, we don't do bound
                //                  // checking.

                // value.setBounds( lb, ub );
                // result += ( value * coef );
            }
            double bias = _biasVectors[layerId][orIndex][andIndex];
            result += bias;

            std::cout << "result: " << result.getLowerBound() << ", " << result.getUpperBound()
                      << std::endl;
            if ( FloatUtils::isPositive( result.getLowerBound() ) &&
                 !FloatUtils::isZero( result.getUpperBound() ) )
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

bool BackPropagation::lpBoundChecking( const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                       const unsigned layerId )
{
    /*
     * Build a LP problem with extra postconditions.
     * Try to solve a feasibility problem, to tighten the bounds.
     * */

    // Step 1. build a lp problem in gurobi with extra postconditions.
    GurobiWrapper gurobi = GurobiWrapper();
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    Vector<bool> verificationResults;

    std::cout << "Bound checking at layer: " << layerId << std::endl;
    std::cout << "Layer type: " << layer->getLayerType() << std::endl;

    // create postconditions.
    for ( unsigned orIndex = 0; orIndex < _postConditions[layerId].size(); ++orIndex )
    {
        gurobi.reset();

        // create set of decision variables.
        for ( unsigned neuronIndex = 0; neuronIndex < layer->getSize(); ++neuronIndex )
        {
            double lb = layer->getLb( neuronIndex );
            double ub = layer->getUb( neuronIndex );
            gurobi.addVariable( Stringf( "a%u", neuronIndex ), lb, ub, GurobiWrapper::CONTINUOUS );
        }

        // List<GurobiWrapper::Term> terms; // empty objective function
        double totalBiase = 0.0;
        for ( unsigned andIndex = 0; andIndex < _postConditions[layerId][orIndex].size();
              ++andIndex )
        {
            List<GurobiWrapper::Term> terms;
            for ( unsigned i = 0; i < _postConditions[layerId][orIndex][andIndex].size(); ++i )
            {
                terms.append( GurobiWrapper::Term( _postConditions[layerId][orIndex][andIndex][i],
                                                   Stringf( "a%u", i ) ) ); // coefficient, variable
                                                                            // name
            }
            // bias
            gurobi.addGeqConstraint( terms, -_biasVectors[layerId][orIndex][andIndex] );
            // totalBiase += _biasVectors[layerId][orIndex][andIndex];
        }

        // Step 2. solve a feasibility problem.
        // gurobi.setCost( terms, totalBiase );
        gurobi.setTimeLimit( 60.0 );
        gurobi.solve();

        // Step 3. identify the result.
        double objectiveBound = gurobi.getObjectiveBound();
        int statusCode = gurobi.getStatusCode();
        std::cout << "Objective bound: " << objectiveBound << std::endl;
        std::cout << "status code: " << statusCode << std::endl;
        if ( gurobi.optimal() && ( FloatUtils::isZero( objectiveBound ) ||
                                   FloatUtils::isPositive( objectiveBound ) ) ) // feasible, and lb
                                                                                // >= 0;
        {
            verificationResults.append( false );
        }
        else if ( gurobi.optimal() && FloatUtils::isNegative( objectiveBound ) ) // feasible, and lb
                                                                                 // < 0;
        {
            verificationResults.append( true );
        }
        else if ( gurobi.infeasible() ) // infeasible
        {
            verificationResults.append( true );
        }
        else // timeout
        {
            verificationResults.append( true );
        }
    }

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
                double coef = _postConditions[outputLayerIndex][orIndex][andIndex][i];
                newLayer->setWeight(
                    outputLayerIndex,
                    outputLayer->variableToNeuron( inputQuery->outputVariableByIndex( i ) ),
                    newNeuronIndex,
                    coef );
            }
            double bias = _biasVectors[outputLayerIndex][orIndex][andIndex];
            newLayer->setBias( andIndex, bias );
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
            mapEntry.second[i] = Vector<Vector<double>>();
        }
        // Clear outer vectors
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<Vector<double>>>();
    }

    // Similar for bias vectors - clean from innermost to outermost
    for ( auto &mapEntry : _biasVectors )
    {
        mapEntry.second.clear();
        mapEntry.second = Vector<Vector<double>>();
    }

    // Step 2: Now clear the maps themselves
    _postConditions.clear();
    _biasVectors.clear();

    // Step 3: Replace with empty maps to force capacity reduction
    _postConditions = Map<unsigned int, Vector<Vector<Vector<double>>>>();
    _biasVectors = Map<unsigned int, Vector<Vector<double>>>();

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
    const double emptyBias = 0.0;
    Vector<Vector<double>> andPostConditions;
    Vector<double> andBiases;
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
            // Vector<Vector<double>> andPostConditions;
            // Vector<double> andBiases;
            for ( const Equation &eq : equations )
            {
                Vector<double> postCondition( numberOfOutputNeurons, 0.0 );
                double bias = 0.0;
                unsigned numberOfOutputVariables = 0;
                for ( const unsigned &pv : eq.getListParticipatingVariables() )
                {
                    if ( givenOutputVariables.exists( pv ) )
                        numberOfOutputVariables++;
                }
                if ( numberOfOutputVariables != eq.getListParticipatingVariables().size() )
                    continue;

                for ( const unsigned &pv : eq.getListParticipatingVariables() )
                {
                    // lhs
                    unsigned variableIndex = inputQuery._variableToOutputIndex[pv];
                    double coefficient = eq.getCoefficient( pv );
                    postCondition[variableIndex] = coefficient;
                    bias = bias + eq._scalar; // scalar is the bias term
                                              // in the equation.

                    std::cout << "Varaible index: " << variableIndex
                              << ", coefficient: " << coefficient << std::endl;

                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.
                }
                andPostConditions.append( std::move( postCondition ) );
                andBiases.append( bias );
            }
            // _numberOfOrConditions++;
        }
    }
    if ( !andPostConditions.empty() )
    {
        _numberOfOrConditions = 1;
        _isDisjunctivePostCondition = true;
        _postConditions[numberOfLayers - 1].append( std::move( andPostConditions ) );
        _biasVectors[numberOfLayers - 1].append( std::move( andBiases ) );
        std::cout << "It's disjuctive property." << std::endl;
        _hasPostConditions.append( true ); // for the output layer, we have at least one
                                           // post-condition.
        return;
    }


    // update the flag to indicate that the post-conditions are disjunctive in the given
    // property file.
    // The post-conditions do not exist OR operator.
    _isDisjunctivePostCondition = false;
    _numberOfOrConditions = 0; // We have at least one post-condition in the output layer.
    const List<Equation> &equations = inputQuery.getEquations();
    std::cout << "This example is AND property. So, we have to transfer to OR property."
              << std::endl;
    for ( const auto &eq : equations )
    {
        Vector<double> postCondition( numberOfOutputNeurons, 0.0 );
        double bias = emptyBias;
        unsigned numberOfOutputVariables = 0;
        for ( const unsigned &pv : eq.getListParticipatingVariables() )
        {
            if ( givenOutputVariables.exists( pv ) )
                numberOfOutputVariables++;
        }

        if ( numberOfOutputVariables != eq.getListParticipatingVariables().size() )
            continue;

        double negation = eq._type == Equation::EquationType::LE ? 1 : 1;
        std::cout << "equation type: " << eq._type << std::endl;
        for ( const unsigned &pv : eq.getListParticipatingVariables() )
        {
            // lhs
            unsigned variableIndex = inputQuery._variableToOutputIndex[pv];
            double coefficient = negation * eq.getCoefficient( pv );
            postCondition[variableIndex] = coefficient;
            bias = bias + negation * eq._scalar; // scalar is the bias term in the
                                                 // equation.
            std::cout << "Varaible index: " << variableIndex << ", coefficient: " << coefficient
                      << std::endl;

            // rhs
            // their storage moves all the terms to the left side of the
            // equation/inequality.
        }
        std::cout << "Adding postcondition!!!" << std::endl;
        andPostConditions.append( std::move( postCondition ) );
        andBiases.append( bias );
        _postConditions[numberOfLayers - 1].append( std::move( andPostConditions ) );
        _biasVectors[numberOfLayers - 1].append( std::move( andBiases ) );
        andPostConditions.clear();
        andBiases.clear();

        _numberOfOrConditions++;
    }

    if ( _postConditions[numberOfLayers - 1].empty() )
    {
        // When the postcondition is bound constraints on output neurons.
        // Currently, we do only have ACASXU dataset, prop1.vnnlib is this case.
        // So, temporarily, we just hard code the postcondition for that.
        _numberOfOrConditions = 1; // We have at least one post-condition in the output layer.
        Vector<double> postCondition( numberOfOutputNeurons, 0.0 );
        double bias = 3.991125645861615;
        postCondition[0] = -1;
        andPostConditions.append( std::move( postCondition ) );
        andBiases.append( bias );
        _postConditions[numberOfLayers - 1].append( std::move( andPostConditions ) );
        _biasVectors[numberOfLayers - 1].append( std::move( andBiases ) );
    }

    // _postConditions[numberOfLayers - 1].append( std::move( andPostConditions ) );
    // _biasVectors[numberOfLayers - 1].append( std::move( andBiases ) );
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

    Vector<double> newPostCondition;
    double newBias;
    const Vector<double> emptyPostConditions;
    const double emptyBias = 0.0;
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
        for ( unsigned layerIndex = numberOfLayers - 1 - _isAddingAuxiliaryLayer; layerIndex > 0;
              --layerIndex )
        {
            std::cout << "layer id : " << layerIndex << std::endl;
            const NLR::Layer *currentLayer = _networkLevelReasoner.getLayer( layerIndex );
            const NLR::Layer *sourceLayer =
                _networkLevelReasoner.getLayer( layerIndex - 1 ); // Assume feedforward network.
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
                    const auto &theLastPostConditions = _postConditions[layerIndex][orIndex];
                    auto theLastBias = _biasVectors[layerIndex][orIndex];
                    // _postConditions[layerIndex - 1].append( std::move( theLastPostConditions ) );
                    _biasVectors[layerIndex - 1].append( std::move( theLastBias ) );

                    Vector<Vector<double>> newAndPostConditions;
                    for ( unsigned andIndex = 0; andIndex < theLastPostConditions.size();
                          ++andIndex )
                    {
                        newPostCondition =
                            Vector<double>( theLastPostConditions[andIndex].size(), 0.0 );
                        int countUnstableNeurons = 0;
                        for ( unsigned neuronIndex = 0;
                              neuronIndex < theLastPostConditions[andIndex].size();
                              ++neuronIndex )
                        {
                            double coef = theLastPostConditions[andIndex][neuronIndex];
                            if ( FloatUtils::isZero( coef ) )
                                continue; // Skip if the coefficient is zero.
                            double sourceNeuronLb = sourceLayer->getLb( neuronIndex );
                            double sourceNeuronUb = sourceLayer->getUb( neuronIndex );

                            if ( FloatUtils::isNegative( sourceNeuronLb ) &&
                                 FloatUtils::isPositive( sourceNeuronUb ) )
                            {
                                countUnstableNeurons++;
                            }

                            if ( FloatUtils::isPositive( coef ) &&
                                 FloatUtils::isNegative( sourceNeuronLb ) )
                            {
                                continue;
                            }
                            if ( FloatUtils::isNegative( sourceNeuronUb ) ||
                                 FloatUtils::isZero( sourceNeuronUb ) )
                            {
                                continue;
                            }

                            newPostCondition[neuronIndex] += coef;
                        }
                        newAndPostConditions.append( std::move( newPostCondition ) );
                        std::cout << "Number of unstable neurons: " << countUnstableNeurons
                                  << std::endl;
                    }
                    // _postConditions[layerIndex][orIndex] = theLastPostConditions;
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
                    const auto &theLastPostConditions = _postConditions[layerIndex][orIndex];
                    const auto &theLastBias = _biasVectors[layerIndex][orIndex];
                    Vector<Vector<double>> newAndPostConditions( theLastPostConditions.size(),
                                                                 emptyPostConditions );
                    Vector<double> newAndBiases( theLastPostConditions.size(), emptyBias );
                    for ( unsigned andIndex = 0; andIndex < theLastPostConditions.size();
                          ++andIndex )
                    {
                        newPostCondition.clear();
                        newPostCondition = Vector<double>();
                        newBias = theLastBias[andIndex];
                        for ( unsigned targetNeuronIndex = 0;
                              targetNeuronIndex < theLastPostConditions[andIndex].size();
                              ++targetNeuronIndex )
                        {
                            // if the coefficient of the target neuron is 0, we skip it.
                            double coef = theLastPostConditions[andIndex][targetNeuronIndex];
                            double targetNeuronLb = currentLayer->getLb( targetNeuronIndex );
                            double targetNeuronUb = currentLayer->getUb( targetNeuronIndex );
                            if ( FloatUtils::isZero( coef ) )
                                continue;

                            for ( const auto &sourceLayerPair : currentLayer->getSourceLayers() )
                            {
                                unsigned sourceLayerSize = sourceLayerPair.second;

                                // create the size of newPostCondition
                                // TODO: this implementation cannot handle "residual network"
                                // yet. detect if there is a residual block.
                                if ( newPostCondition.empty() )
                                    newPostCondition = Vector<double>( sourceLayerSize, 0.0 );

                                for ( unsigned sourceNeuronIndex = 0;
                                      sourceNeuronIndex < sourceLayerSize;
                                      ++sourceNeuronIndex )
                                {
                                    double weight = currentLayer->getWeight( sourceLayerPair.first,
                                                                             sourceNeuronIndex,
                                                                             targetNeuronIndex );
                                    double sourceNeuronUb = sourceLayer->getUb( sourceNeuronIndex );
                                    double sourceNeuronLb = sourceLayer->getLb( sourceNeuronIndex );
                                    if ( ( FloatUtils::isZero( sourceNeuronUb ) ||
                                           FloatUtils::isNegative( sourceNeuronUb ) ) &&
                                         layerIndex - 1 != 0 )
                                        continue;

                                    if ( FloatUtils::isNegative( coef * weight ) )
                                    {
                                        newPostCondition[sourceNeuronIndex] +=
                                            ( coef * weight * targetNeuronUb );
                                    }
                                    else if ( FloatUtils::isPositive( coef * weight ) )
                                    {
                                        if ( FloatUtils::isNegative( targetNeuronLb ) )
                                        {
                                            continue;
                                        }
                                        else
                                        {
                                            newPostCondition[sourceNeuronIndex] +=
                                                ( coef * weight * targetNeuronLb );
                                        }
                                    }
                                }
                            }
                            double bias = currentLayer->getBias( targetNeuronIndex );
                            newBias += ( coef * bias );
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
