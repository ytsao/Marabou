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

namespace BP {
BackPropagation::BackPropagation()
{
}

BackPropagation::~BackPropagation()
{
}

bool BackPropagation::boundChecking( const Query &inputQuery,
                                     const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                                     const unsigned int pcId,
                                     const unsigned int layerId ) const
{
    /*
    TODO:
    Doing bound checking to check if the bounds can satisfy the post-conditions;

    1. Transfer the set of variables (lb, ub) to be the set of Interval objects.
    2. Iterate through the set of post-conditions with the set of Interval objects.

    */
    Map<std::string, Interval> variables;

    // 1.
    printf( "first step of boundChecking\n" );
    printf( "layerID = %d\n", layerId );
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( layerId );
    printf( "layer->getSize() = %d\n", layer->getSize() );
    for ( unsigned int neuronId = 0; neuronId < layer->getSize(); ++neuronId )
    {
        double lb = layer->getLb( neuronId );
        double ub = layer->getUb( neuronId );
        Interval interval = Interval( lb, ub );
        std::string name = "x_" + std::to_string( layerId + 1 ) + "_" + std::to_string( neuronId );
        variables[name] = interval;
        printf( "name = %s\n", name.c_str() );
    }

    // 2.
    // TODO:
    // there is an error saying that "RuntimeError: Varaible not found."
    // but I'm not sure why yet,
    // maybe I should printf all the variables' name to see if it is correct.
    printf( "second step of boundChecking\n" );
    printf( "postConditions.size() = %d\n", _postConditions.size() );
    for ( auto &orCondition : _postConditions )
    {
        bool andResult = false;
        printf( "orCondition.first = %d\n", orCondition.first );
        printf( "orCondition.second.size() = %d\n", orCondition.second.size() );
        std::vector<std::string> andConstraints = orCondition.second[pcId];
        bool isLastLayer = ( andConstraints.size() > 0 ) && ( pcId == andConstraints.size() - 1 );
        ASTEvaluator ast = ASTEvaluator( &variables, isLastLayer );
        for ( auto &andConstraint : andConstraints )
        {
            printf( "andConstraints.size() = %ld\n", andConstraints.size() );
            printf( "pcId = %d\n", pcId );
            printf( "isLastLayer = %d\n", isLastLayer );
            printf( "size of andConstiraints = %ld\n", andConstraints.size() );
            printf( " andConstraint = %s\n", andConstraint.c_str() );
            // Because the Vnnparser builds the constraints as <= type,
            // we need to negate the constraint to check if it is satisfied.
            // For example, if the constraint is x1 + x2 <= 0, we need to check
            // if -(x1 + x2) >= 0.
            Interval result = -ast.evaluate( andConstraint.c_str() ); // ! the problem seems in
                                                                      // here.
            printf( "result = %f\n", result.getLowerBound() );
            if ( result.getLowerBound() < 0 )
            {
                andResult = false;
                break;
            }
            if ( andResult )
                return true;
        }
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
    _generateNewPostConditions();
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

        std::string dep_vars_id = "";
        for ( auto &v : var.second )
        {
            dep_vars_id += " term = " + v.term + " ; id = " + std::to_string( v.id ) + " , ";
        }
        printf( "%s = %s\n", var.first.c_str(), dep_vars_id.c_str() );
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
        std::string name = "x_" + std::to_string( numberOfLayers ) + "_" + std::to_string( i );
        Node node = Node( inputQuery._outputIndexToVariable[i], 1, name );
    }

    // create the post-conditions
    List<std::string> outputLayerPostCondition;

    // load given post-conditions that is defined in vnnlib/text file.
    List<Equation> equations = inputQuery.getEquations();
    List<PiecewiseLinearConstraint *> disjunctiveConstraints =
        inputQuery.getPiecewiseLinearConstraints();
    printf( "disjunctiveConstraints.size() = %d\n", disjunctiveConstraints.size() );
    printf( "equations.size() = %d\n", equations.size() );

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

        // update the flag to indicate that the post-conditions are disjunctive in the given
        // property file.
        _isDisjunctivePostCondition = true;

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
                    std::string term = "x_" + std::to_string( numberOfLayers ) + "_" +
                                       std::to_string( inputQuery._variableToOutputIndex[pv] ) +
                                       " + ";
                    postCondition += std::to_string( coefficient ) + " * " + term;

                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.
                }

                // remove the last character "+" from string.
                printf( "introduce a new post-condtion \n" );
                postCondition = postCondition.substr( 0, postCondition.size() - 1 );
                outputLayerPostCondition.push_back( postCondition );
            }
            // this disjunctive constraint is a post-condition;
            _postConditions[_numberOfOrConditions].push_back( outputLayerPostCondition );
        }
        // this disjunctive constraint is a post-condition;
        _numberOfOrConditions++;
    }

    printf( "number of disjunctive constraints = %d\n", _numberOfOrConditions );
    if ( _numberOfOrConditions == 0 )
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
                std::string term = "x_" + std::to_string( numberOfLayers ) + "_" +
                                   std::to_string( inputQuery._variableToOutputIndex[pv] ) + " + ";
                printf( "term = %s\n", term.c_str() );
                postCondition += std::to_string( coefficient ) + " * " + term;

                // rhs
                // their storage moves all the terms to the left hand side of the
                // equation/inequality.
            }

            // remove the last character "+" from string.
            postCondition = postCondition.substr( 0, postCondition.size() - 2 );
            outputLayerPostCondition.push_back( postCondition );
        }
        //
        _postConditions[_numberOfOrConditions].push_back( outputLayerPostCondition );
        printf( "Add a new post-condition\n" );
    }

    return;
}

void BackPropagation::_buildRelations( const Query &inputQuery,
                                       const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    /*
        Build the formulation for computing each variable.
    */
    unsigned int linearLayerId = 0;
    unsigned int numberOfLayers = _networkLevelReasoner.getNumberOfLayers();
    Map<unsigned int, NLR::Layer *> _layerIndexToLayer =
        _networkLevelReasoner.getLayerIndexToLayer();

    for ( int index = numberOfLayers - 1; index >= 0; --index )
    {
        NLR::Layer *currentLayer = _layerIndexToLayer[index];
        NLR::Layer::Type layerType = currentLayer->getLayerType();

        if ( layerType == NLR::Layer::Type::ABSOLUTE_VALUE )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::BILINEAR )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::INPUT )
        {
            // In my opinion, NLR::Layer::Type::INPUT == torch.nn.Flatten;
            // For the Flatten layer,
            // we don't need to do anything.
            break;
        }
        else if ( layerType == NLR::Layer::Type::LEAKY_RELU )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::MAX )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::RELU )
        {
            linearLayerId++;
            continue;
        }
        else if ( layerType == NLR::Layer::Type::SIGMOID )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::SIGN )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::SOFTMAX )
        {
            break;
        }
        else if ( layerType == NLR::Layer::Type::WEIGHTED_SUM )
        {
            // In my opinion, NLR::Layer::Type::WEIGHTED_SUM == torch.Linear;
            for ( unsigned int i = 0; i < currentLayer->getSize(); ++i )
            {
                std::string var1 = "x_" + std::to_string( numberOfLayers - linearLayerId ) + "_" +
                                   std::to_string( i );

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
                                      "x_" + std::to_string( numberOfLayers - linearLayerId - 2 ) +
                                          "_" + std::to_string( j ) );
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

        linearLayerId++;
    }

    return;
}

void BackPropagation::_generateNewPostConditions()
{
    /*
        Create new post-conditions for other layers by original post-conditions.
        Build an abstract syntax tree (AST) for representing the post-conditions.
    */
    for ( unsigned int i = 0; i < _numberOfOrConditions + 1; ++i )
    {
        printf( "GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS = %d\n",
                GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS );
        printf( "_postConditions[%d].size() = %d\n", i, _postConditions[i].size() );
        while ( _postConditions[i].size() <
                GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS )
        {
            // Extract the last post-conditions,
            // Declare a new post-condition for the previous layer.
            std::string newPostCondition = "";

            std::vector<std::string> theLastPostConditions = _postConditions[i][0];
            std::vector<std::string> newPostConditions;

            printf( "theLastPostConditions.size() = %d\n", theLastPostConditions.size() );
            for ( auto &postCondition : theLastPostConditions )
            {
                printf( "postCondition = %s\n", postCondition.c_str() );
                // Split the postCondition string by whitespace
                std::istringstream iss( postCondition );
                std::vector<std::string> tokens;
                std::string token;

                // Read tokens separated by whitespace
                while ( iss >> token )
                    tokens.push_back( token );

                for ( unsigned int i = 0; i < tokens.size(); ++i )
                {
                    if ( _vars.exists( tokens[i] ) )
                    {
                        std::string newVariables = "( ";
                        bool hasBias = false;
                        for ( auto &dep_var : _vars[tokens[i]] )
                        {
                            if ( dep_var.term == "bias" )
                            {
                                hasBias = true;
                                newVariables += std::to_string( dep_var.coefficient );
                            }
                            else
                            {
                                newVariables += std::to_string( dep_var.coefficient ) + " * " +
                                                dep_var.term + " + ";
                            }
                        }
                        // if the last term is not bias, then remove the last character "+" from
                        // string.
                        if ( !hasBias )
                            newVariables = newVariables.substr( 0, newVariables.size() - 2 );
                        newVariables += " )";
                        tokens[i] = newVariables;
                        printf( "newVariables = %s\n", newVariables.c_str() );
                    }
                }

                // Concate the tokens into a new post-condition.
                std::string newPostCondition = "";
                for ( unsigned int i = 0; i < tokens.size(); ++i )
                {
                    if ( tokens[i] == "*" )
                        tokens[i] = " * ";
                    else if ( tokens[i] == "+" )
                        tokens[i] = " + ";

                    newPostCondition += tokens[i];
                }

                // Add the new post-condition to the list of post-conditions.
                newPostConditions.push_back( newPostCondition );
                printf( "newPostCondition = %s\n", newPostCondition.c_str() );
            }

            // Add the new post-condition to the list of post-conditions.
            _postConditions[i].insert( _postConditions[i].begin(), newPostConditions );
        }

        printf( "_postConditions[%d].size() = %d\n", i, _postConditions[i].size() );
        printf( "==========================\n" );
        if ( GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS <
             _numberOfLinearLayers + 1 )
        {
            printf( "add empty post-conditions\n" );
            printf( "number of linear layers = %d\n", _numberOfLinearLayers );
            for ( unsigned int _ = GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS;
                  _ < _numberOfLinearLayers + 1;
                  ++_ )
                _postConditions[i].insert( _postConditions[i].begin(), std::vector<std::string>() );
            printf( "_postConditions[%d].size() = %d\n", i, _postConditions[i].size() );
        }
    }

    return;
}
} // namespace BP