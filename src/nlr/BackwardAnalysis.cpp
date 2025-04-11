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

bool BackPropagation::bound_checking( const Query &inputQuery,
                                      const int pc_id,
                                      const int layer_id ) const
{
    /*
    TODO:
    Doing bound checking to check if the bounds can satisfy the post-conditions;

    1. Transfer the set of variables (lb, ub) to be the set of Interval objects.
    2. Iterate through the set of post-conditions with the set of Interval objects.

    */
    Map<std::string, Interval> variables;

    // 1.
    for ( unsigned int i = 0; i < inputQuery.getNumberOfVariables(); ++i )
    {
        double lb = inputQuery.getLowerBound( i );
        double ub = inputQuery.getUpperBound( i );
        Interval interval = Interval( lb, ub );
        std::string name = "x_" + std::to_string( layer_id ) + "_" + std::to_string( i );
        variables[name] = interval;
    }

    // 2.
    for ( auto &orCondition : _post_conditions )
    {
        bool andResult = true;
        for ( auto &andConstraints : orCondition.second )
        {
            bool isLastLayer = true ? pc_id == orCondition.second.size() - 1 : false;

            ASTEvaluator ast = ASTEvaluator( &variables, isLastLayer );
            for ( auto &andConstraint : andConstraints )
            {
                Interval result = ast.evaluate( andConstraint.c_str() );
                if ( result.getLowerBound() < 0 )
                {
                    andResult = false;
                    break;
                }
            }
            if ( andResult )
                return false;
        }
    }

    return false;
}

void BackPropagation::build( const Query &inputQuery,
                             const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    _init_post_conditions( inputQuery, _networkLevelReasoner );
    _build_relation( inputQuery, _networkLevelReasoner );
    dump();
    _generate_new_post_conditions();
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

void BackPropagation::_init_post_conditions(
    const Query &inputQuery,
    const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    // We can find the origin post-conditions from inputQuery.equations (which is defined in vnnlib
    // or text file).
    // extract the information from inputQuery, for easy to use later.

    List<unsigned int> givenInputVariables = inputQuery.getInputVariables();
    List<unsigned int> givenOutputVariables = inputQuery.getOutputVariables();

    // create variables in the output layer.
    List<Node> outputVariables;
    for ( unsigned int i = 0; i < inputQuery.getNumOutputVariables(); ++i )
    {
        std::string name = "x_" + std::to_string( _networkLevelReasoner.getNumberOfLayers() ) +
                           "_" + std::to_string( i );
        Node node = Node( i, 1, name );
        outputVariables.append( node );
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
        std::string post_condition = "";
        for ( auto &pdv : participatingDisjunctiveVariables )
        {
            if ( givenOutputVariables.exists( pdv ) )
            {
                // update the flag to indicate that the post-conditions are disjunctive in the given
                // property file.
                _isDisjunctivePostCondition = true;

                // this disjunctive constraint is a post-condition;
                _numberOfOrConditions++;

                List<PiecewiseLinearCaseSplit> splits = disCon->getCaseSplits();
                for ( auto &split : splits )
                {
                    List<Equation> equations = split.getEquations();
                    List<std::string> outputLayerPostCondition;
                    for ( auto &eq : equations )
                    {
                        List<unsigned int> participatingVariables =
                            eq.getListParticipatingVariables();
                        std::string post_condition = "";
                        for ( auto &pv : participatingVariables )
                        {
                            // lhs
                            double coefficient = eq.getCoefficient( pv );
                            std::string term =
                                "x_" + std::to_string( _networkLevelReasoner.getNumberOfLayers() ) +
                                "_" + std::to_string( pv ) + " +";

                            // rhs
                            // their storage moves all the terms to the left side of the
                            // equation/inequality.

                            post_condition += term;
                        }

                        // remove the last character "+" from string.
                        if ( post_condition != "" )
                        {
                            post_condition = post_condition.substr( 0, post_condition.size() - 1 );
                            outputLayerPostCondition.append( post_condition );
                        }
                    }
                    // this disjunctive constraint is a post-condition;
                    _post_conditions[_numberOfOrConditions].append( outputLayerPostCondition );
                    _numberOfOrConditions++;
                }
            }
            else
            {
                break;
            }
        }
    }

    if ( _numberOfOrConditions == 1 )
    {
        List<std::string> outputLayerPostCondition;
        for ( auto &eq : equations )
        {
            List<unsigned int> participatingVariables = eq.getListParticipatingVariables();
            std::string post_condition = "";
            for ( auto &pv : participatingVariables )
            {
                if ( givenOutputVariables.exists( pv ) )
                {
                    // this equation is a post-condition;
                    // lhs
                    double coefficient = eq.getCoefficient( pv );
                    std::string term = "x_" +
                                       std::to_string( _networkLevelReasoner.getNumberOfLayers() ) +
                                       "_" + std::to_string( pv ) + " +";
                    // rhs
                    // their storage moves all the terms to the left side of the
                    // equation/inequality.

                    post_condition += term;
                }
                else
                {
                    break;
                }
            }
            if ( post_condition != "" )
            {
                // remove the last character "+" from string.
                post_condition = post_condition.substr( 0, post_condition.size() - 1 );
                outputLayerPostCondition.append( post_condition );

                // this equation is a post-condition;
                _post_conditions[_numberOfOrConditions].append( outputLayerPostCondition );
            }
        }
    }

    return;
}

void BackPropagation::_build_relation( const Query &inputQuery,
                                       const NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    /*
        Build the formulation for computing each variable.
    */
    unsigned int currentLayerId = 0;
    unsigned int numberOfLayers = _networkLevelReasoner.getNumberOfLayers();
    Map<unsigned int, NLR::Layer *> _layerIndexToLayer =
        _networkLevelReasoner.getLayerIndexToLayer();

    for ( unsigned int index = numberOfLayers - 1; index >= 0; --index )
    {
        NLR::Layer *currentLayer = _layerIndexToLayer[index];
        NLR::Layer::Type layerType = currentLayer->getLayerType();
        switch ( layerType )
        {
        case NLR::Layer::Type::ABSOLUTE_VALUE:
            break;
        case NLR::Layer::Type::RELU:
            currentLayerId++;
            break;
        case NLR::Layer::Type::SIGN:
            break;
        case NLR::Layer::Type::LEAKY_RELU:
            break;
        case NLR::Layer::Type::SIGMOID:
            break;
        case NLR::Layer::Type::ROUND:
            break;
        case NLR::Layer::Type::SOFTMAX:
            break;
        case NLR::Layer::Type::BILINEAR:
            break;
        case NLR::Layer::Type::MAX:
            break;
        case NLR::Layer::Type::WEIGHTED_SUM:
            // In my opinion, NLR::Layer::Type::WEIGHTED_SUM == torch.Linear;
            for ( unsigned int i = 0; i < currentLayer->getSize(); ++i )
            {
                std::string var1 = "x_" + std::to_string( numberOfLayers - currentLayerId ) + "_" +
                                   std::to_string( i );
                unsigned int sourceLayer = currentLayer->getSourceLayers()[index];
                unsigned int numberOfSourceNeurons = _layerIndexToLayer[sourceLayer]->getSize();
                for ( unsigned int j = 0; j < numberOfSourceNeurons; ++j )
                {
                    double weight = currentLayer->getWeight( sourceLayer, j, i );
                    if ( weight != 0 )
                    {
                        Node var2 =
                            Node( j,
                                  weight,
                                  "x_" + std::to_string( numberOfLayers - currentLayerId - 2 ) +
                                      "_" + std::to_string( j ) );
                        _vars[var1].append( var2 );
                    }
                }
                double bias = currentLayer->getBias( i );
                if ( bias != 0 )
                {
                    Node biasNode = Node( -1, bias, std::to_string( bias ) );
                    _vars[var1].append( biasNode );
                }
            }
            _numberOfLinearLayers++;
            break;
        case NLR::Layer::Type::INPUT:
            // In my opinion, NLR::Layer::Type::INPUT == torch.nn.Flatten;
            //
            // For the Flatten layer,
            // we don't need to do anything.
            break;
        default:
            printf( "[ERROR] Unknown layer type: %d\n", layerType );
            break;
        }
        currentLayerId++;
    }

    // for ( unsigned int index = 0; index < numberOfLayers; ++index )
    // {
    //     NLR::Layer *currentLayer = _layerIndexToLayer[index];
    //     // TODO
    //     // ref: Expression.h & Expression.cpp
    //     printf( "" );
    // }

    return;
}

void BackPropagation::_generate_new_post_conditions()
{
    /*
        Create new post-conditions for other layers by original post-conditions.
        Build an abstract syntax tree (AST) for representing the post-conditions.
    */
    if ( _isDisjunctivePostCondition )
    {
        for ( unsigned int i = 0; i < _numberOfOrConditions; ++i )
        {
            while ( _post_conditions.size() <
                    GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS )
            {
                // Extract the last post-conditions,
                // Declare a new post-condition for the previous layer.
                std::string newPostCondition = "";
                List<std::string> theLastPostConditions = _post_conditions[i].front();
                List<std::string> newPostConditions;

                for ( auto &postCondition : theLastPostConditions )
                {
                    // Split the postCondition string by whitespace
                    std::istringstream iss( postCondition );
                    std::vector<std::string> tokens;
                    std::string token;

                    // Read tokens separated by whitespace
                    while ( iss >> token )
                        tokens.push_back( token );

                    for ( unsigned int i = 0; i < tokens.size(); ++i )
                        if ( _vars.exists( tokens[i] ) )
                            tokens[i] = "(" + tokens[i] + ")";

                    // Concate the tokens into a new post-condition.
                    std::string newPostCondition = "";
                    for ( unsigned int i = 0; i < tokens.size(); ++i )
                        newPostCondition += tokens[i];

                    // Add the new post-condition to the list of post-conditions.
                    newPostConditions.append( newPostCondition );
                }

                // Add the new post-condition to the list of post-conditions.
                _post_conditions[i].appendHead( newPostConditions );
            }

            if ( GlobalConfiguration::MAX_LAYERS_WITH_ADDITIONAL_POST_CONDITIONS <
                 _numberOfLinearLayers + 1 )
            {
                for ( unsigned int _ = 0; _ < _numberOfLinearLayers; ++_ )
                    _post_conditions[i].appendHead( List<std::string>() );
            }
        }
    }

    return;
}
} // namespace BP