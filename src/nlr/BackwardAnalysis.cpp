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
}

bool BackPropagation::boundChecking( const Query &inputQuery,
                                     const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                     const unsigned int layerId ) const
{
    /*
    Doing bound checking to check if the bounds can satisfy the post-conditions;

    1. Transfer the set of variables (lb, ub) to be the set of Interval objects.
    2. Iterate through the set of post-conditions with the set of Interval objects.

    */
    Map<std::string, Interval> variables;

    // 1.
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    for ( unsigned int neuronId = 0; neuronId < layer->getSize(); ++neuronId )
    {
        double lb = layer->getLb( neuronId );
        double ub = layer->getUb( neuronId );
        Interval interval = Interval( lb, ub );
        std::string name = "x_" + std::to_string( layerId ) + "_" + std::to_string( neuronId );
        variables[name] = interval;
    }

    // 2.
    for ( auto &orCondition : _postConditions )
    {
        bool andResult = false;
        std::vector<std::string> andConstraints = orCondition.second[layerId];
        ASTEvaluator ast = ASTEvaluator( &variables );
        for ( auto &andConstraint : andConstraints )
        {
            // Because the Vnnparser builds the constraints as <= type,
            // we need to negate the constraint to check if it is satisfied.
            // For example, if the constraint is x1 + x2 <= 0, we need to check
            // if -(x1 + x2) >= 0.
            //
            // Because our algorithm assumes that the post-conditions are in the form of
            // Ax + b >= 0.
            printf( "andConstraint = %s\n", andConstraint.c_str() );
            Interval result = -ast.evaluate( andConstraint.c_str() );
            printf( "result = %f\n", result.getLowerBound() );
            if ( result.getLowerBound() < 0 )
            {
                andResult = false;
                break;
            }
            andResult = true;
        }
        if ( andResult )
            return true;
    }

    return false;
}

void BackPropagation::build( const Query &inputQuery,
                             const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                             const Preprocessor &preprocessor )
{
    _initPostConditions( inputQuery, _networkLevelReasoner, preprocessor );
    _buildRelations( inputQuery, _networkLevelReasoner );
    // dump();
    _generateNewPostConditions( inputQuery, _networkLevelReasoner );
}

void BackPropagation::dump() const
{
    /*
        Display the post-conditions and the relations between the variables.
    */
    for ( auto &var : _vars )
    {
        std::string dep_vars = "";
        for ( auto v : var.second )
        {
            dep_vars += std::to_string( v.coefficient ) + " * " + v.term + " + ";
        }
        printf( "%s = %s\n", var.first.c_str(), dep_vars.c_str() );

        // std::string dep_vars_id = "";
        // for ( auto &v : var.second )
        // {
        //     dep_vars_id += " term = " + v.term + " ; id = " + std::to_string( v.id ) + " , ";
        // }
        // printf( "%s = %s\n", var.first.c_str(), dep_vars_id.c_str() );
    }

    return;
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
    for ( unsigned int i = 0; i < outputLayer.getSize(); ++i )
    {
        std::string name = "x_" + std::to_string( numberOfLayers - 1 ) + "_" + std::to_string( i );
        Node node = Node( inputQuery._outputIndexToVariable[i], 1, name );
        _outputVariables.append( name );
        printf( "output variable name = %s\n", name.c_str() );
    }

    // create the post-conditions
    List<std::string> outputLayerPostCondition;

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
            std::vector<std::string> outputLayerPostCondition;
            for ( auto &eq : splitEquations )
            {
                List<unsigned int> participatingVariables = eq.getListParticipatingVariables();
                std::string postCondition = "";
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
                    double coefficient = eq.getCoefficient( pv );
                    std::string term = "x_" + std::to_string( numberOfLayers - 1 ) + "_" +
                                       std::to_string( inputQuery._variableToOutputIndex[pv] ) +
                                       " + ";
                    postCondition += std::to_string( coefficient ) + " * " + term;

                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.
                }

                // remove the last character "+" from string.
                postCondition = postCondition.substr( 0, postCondition.size() - 2 );
                outputLayerPostCondition.push_back( postCondition );
            }
            // this disjunctive constraint is a post-condition;
            _postConditions[_numberOfOrConditions].push_back( outputLayerPostCondition );
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
        std::vector<std::string> outputLayerPostCondition;
        for ( auto &eq : equations )
        {
            List<unsigned int> participatingVariables = eq.getListParticipatingVariables();
            std::string postCondition = "";
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
                // this equation is a post-condition;
                // lhs
                double coefficient = eq.getCoefficient( pv );
                std::string term = "x_" + std::to_string( numberOfLayers - 1 ) + "_" +
                                   std::to_string( inputQuery._variableToOutputIndex[pv] ) + " + ";
                postCondition += std::to_string( coefficient ) + " * " + term;

                // rhs
                // their storage moves all the terms to the left hand side of the
                // equation/inequality.
            }

            // remove the last character "+" from string.
            postCondition = postCondition.substr( 0, postCondition.size() - 2 );
            outputLayerPostCondition.push_back( postCondition );
        }
        _postConditions[_numberOfOrConditions].push_back( outputLayerPostCondition );
    }

    return;
}

void BackPropagation::_buildRelations( const Query &inputQuery,
                                       const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    /*
        Build the formulation for computing each variable.
    */
    unsigned int layerCounter = 0;
    unsigned int numberOfLayers = _networkLevelReasoner.getNumberOfLayers();
    // _networkLevelReasoner.dumpTopology();
    Map<unsigned int, NLR::Layer *> _layerIndexToLayer =
        _networkLevelReasoner.getLayerIndexToLayer();

    for ( int index = numberOfLayers - 1; index >= 0; --index )
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
            // In my opinion, NLR::Layer::Type::INPUT == torch.nn.Flatten;
            // For the Flatten layer,
            // we don't need to do anything.
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
            if ( index == numberOfLayers - 1 )
            {
                _isActivationBeforeOutput = true;
                // In this situation, the one previous layer must be "WEIGHTED_SUM"
                // But, there is no weights between the output and the provious layer.
                // Thus, we only need to build the connection to link each neuon with same index in
                // the layer. For example, x_1_0 = x_2_0, x_1_1 = x_2_1, x_1_2 = x_2_2, ...
                for ( unsigned int i = 0; i < currentLayer->getSize(); ++i )
                {
                    std::string var1 = "x_" + std::to_string( index ) + "_" + std::to_string( i );
                    Node var2 = Node(
                        i, 1, "x_" + std::to_string( index - 1 ) + "_" + std::to_string( i ) );
                    _vars[var1].push_back( var2 );
                }
            }
            else
            {
                layerCounter++;
            }
            continue;
        }
        else if ( layerType == NLR::Layer::Type::SIGMOID )
        {
            if ( index == numberOfLayers )
            {
                _isActivationBeforeOutput = true;
            }
            else
            {
                layerCounter++;
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
            // In my opinion, NLR::Layer::Type::WEIGHTED_SUM == torch.Linear;
            bool isBeforeIputLayer = index == 1 ? true : false;
            for ( unsigned int i = 0; i < currentLayer->getSize(); ++i )
            {
                std::string var1 = "x_" + std::to_string( index ) + "_" + std::to_string( i );

                // ref: LPFormulatior.cpp `Line 1589`;
                // To see how to use layer->getSourceLayers();
                // It seems that getSourceLayers() returns a map of source layer id and the
                //     size of
                // the source layer.
                for ( const auto &sourceLayerPair : currentLayer->getSourceLayers() )
                {
                    unsigned int sourceLayerSize = sourceLayerPair.second;
                    for ( unsigned int j = 0; j < sourceLayerSize; ++j )
                    {
                        double weight = currentLayer->getWeight( sourceLayerPair.first, j, i );
                        if ( weight != 0 )
                        {
                            Node var2 =
                                Node( j,
                                      weight,
                                      "x_" + std::to_string( index - 2 + isBeforeIputLayer ) + "_" +
                                          std::to_string( j ) );
                            _vars[var1].push_back( var2 );
                        }
                    }
                    double bias = currentLayer->getBias( i );
                    if ( bias != 0 )
                    {
                        Node biasNode = Node( -1, bias, "bias" );
                        _vars[var1].push_back( biasNode );
                    }
                }
            }
            _numberOfLinearLayers++;
        }

        layerCounter++;
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

    for ( unsigned int i = 0; i < _numberOfOrConditions + 1; ++i )
    {
        unsigned int countAddedPostConditions = 1; // It should be 1, because there is an onriginal
                                                   // post-condition in the output layer by default.
        std::vector<std::string> theLastPostConditions = _postConditions[i][0];

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
                _postConditions[i].insert( _postConditions[i].begin(), std::vector<std::string>() );
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
                _postConditions[i].insert( _postConditions[i].begin(), std::vector<std::string>() );
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
                    std::vector<std::string> newPostConditions;
                    for ( auto &postCondition : theLastPostConditions )
                    {
                        // Split the postCondition string by whitespace
                        std::istringstream iss( postCondition );
                        std::vector<std::string> tokens;
                        std::string token;

                        // Read tokens separated by whitespace
                        while ( iss >> token )
                            tokens.push_back( token );

                        for ( unsigned int j = 0; j < tokens.size(); ++j )
                        {
                            if ( _vars.exists( tokens[j] ) )
                            {
                                std::string newVariables = "ReLU( ";
                                bool hasBias = false;
                                for ( auto &dep_var : _vars[tokens[j]] )
                                {
                                    if ( dep_var.term == "bias" )
                                    {
                                        hasBias = true;
                                        newVariables += std::to_string( dep_var.coefficient );
                                    }
                                    else
                                    {
                                        newVariables += std::to_string( dep_var.coefficient ) +
                                                        " * " + dep_var.term + " + ";
                                    }
                                }
                                // if the last term is not bias, then remove the last character "+"
                                // from string.
                                if ( !hasBias )
                                    newVariables =
                                        newVariables.substr( 0, newVariables.size() - 2 );
                                newVariables += " )";
                                tokens[j] = newVariables;
                            }
                        }

                        // Concate the tokens into a new postCondition
                        std::string newPostCondition = "";
                        for ( unsigned int j = 0; j < tokens.size(); ++j )
                        {
                            if ( tokens[j] == "*" )
                                tokens[j] = " * ";
                            else if ( tokens[j] == "+" )
                                tokens[j] = " + ";

                            newPostCondition += tokens[j];
                        }

                        // Add the new postCondition to the list of postConditions
                        newPostConditions.push_back( newPostCondition );
                    }
                    // Add the new post-condition to the list of post-conditions.
                    _postConditions[i].insert( _postConditions[i].begin(), newPostConditions );
                    countAddedPostConditions++;
                    theLastPostConditions = newPostConditions;
                }
                else
                {
                    _postConditions[i].insert( _postConditions[i].begin(),
                                               std::vector<std::string>() );
                }
                continue;
            }
        }
    }
    return;
}
} // namespace BP
