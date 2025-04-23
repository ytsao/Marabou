/*********************                                                        */
/*! \file FixedPointLoop.cpp
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

#include "FixedPointLoop.h"

#include "Vector.h"


namespace FPL {

FixedPointPropagation::FixedPointPropagation( const BP::BackPropagation &backPropagation )
    : _backPropagation( backPropagation )
{
}

FixedPointPropagation::~FixedPointPropagation()
{
}


bool FixedPointPropagation::iterate( NLR::NetworkLevelReasoner &_networkLevelReasoner )
{
    // TODO: Implement the bound tightening method.
    // Concept:
    //          -h_1 + 0.4h_2 >= 0.5
    //          (1-1) -h_1 >= 0.5 - 0.4h_2
    //          (1-2) h_1 <= 0.4h_2 - 0.5
    //          --------------------------
    //          (2-1) 0.4h_2 >= 0.5 + h_1
    //          (2-2) h_2 >= (0.5 + h_1) / 0.4
    // Step 1. Get post-conditions, output variables.
    // Step 2. Get interval for each variable.
    // Step 3.
    // Step 4.

    bool isLocalTightenBounds = false;
    bool isGlobalTightenBounds = false;

    // Get postConditions
    Map<unsigned int, Vector<Vector<std::string>>> postConditions =
        _backPropagation.getPostConditions();
    List<std::string> outputVariables = _backPropagation.getOutputVariables();

    // The procedure can be like bound checking, replace the interval as a constant
    Map<std::string, Interval> variables;
    const NLR::Layer *layer = _networkLevelReasoner.getLayer( _currentStartLayer );
    for ( unsigned int neuronId = 0; neuronId < layer->getSize(); ++neuronId )
    {
        double lb = layer->getLb( neuronId );
        double ub = layer->getUb( neuronId );
        Interval interval = Interval( lb, ub );
        std::string name =
            "x_" + std::to_string( _currentStartLayer ) + "_" + std::to_string( neuronId );
        variables[name] = interval;
    }

    do
    {
        isLocalTightenBounds = false; // reset value to false.
        for ( auto &orCondition : postConditions )
        {
            bool andResult = false;
            std::vector<std::string> andConstraints = orCondition.second[_currentStartLayer];
            for ( auto &andConstraint : andConstraints )
            {
                List<std::string> variableOrder;

                // Split the constraint into several terms;
                // Check if the term is variable?
                // assert(number of variables == 2);
                // assert(all variables are in the outputVariable list)
                std::istringstream iss( andConstraint );
                Vector<std::string> tokens;
                std::string token;

                while ( iss >> token )
                    tokens.push_back( token );

                for ( unsigned int j = 0; j < tokens.size(); ++j )
                {
                    if ( outputVariables.exists( tokens[j] ) )
                    {
                        variableOrder.append( tokens[j] );
                    }
                }
                assert( variableOrder.size() == 2 );

                for ( std::string fixedVariable : variableOrder )
                {
                    Interval temp_interval = variables[fixedVariable];
                    Interval zero_interval = Interval( 0, 0 );
                    variables[fixedVariable] = temp_interval;

                    ASTEvaluator ast = ASTEvaluator( &variables );
                    Interval result = ast.evaluate( andConstraint.c_str() );

                    // update bounds
                    if ( result.getlowerBound() > temp_interval.getLowerBound() )
                    {
                        temp_interval.setLowerBound( result.getlowerBound() );
                        isLocalTightenBounds = true;
                        isGlobalTightenBounds = true;
                    }
                    if ( result.getUpperBound() < temp_interval.getUpperBound() )
                    {
                        temp_interval.setUpperBound( result.getUpperBound() );
                        isLocalTightenBounds = true;
                        isGlobalTightenBounds = true;
                    }
                    variables[fixedVariable] = temp_interval;
                }
            }
        }
    }
    while ( isLocalTightenBounds );

    // update bounds in _networkLevelReasoner
    // TODO: to see how to update the bounds in _networkLevelReasoner & inputQuery.
    // if ( isGlobalTightenBounds )
    // {
    // }


    return isGlobalTightenBounds;
}

} // namespace FPL
