/*********************                                                        */
/*! \file BackwardAnalysis.h
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

#ifndef __BackwardAnalysis_h__
#define __BackwardAnalysis_h__

#include "InputQuery.h"
#include "Interval.h"
#include "Layer.h"
#include "LayerOwner.h"
#include "NetworkLevelReasoner.h"
#include "Options.h"
#include "Preprocessor.h"
#include "Query.h"

#include <vector>

namespace BP {
class BackPropagation
{
public:
    BackPropagation();
    ~BackPropagation();

    bool boundChecking( const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                        const unsigned layerId ); // Check the post-condition of the
                                                  // layerId
    bool boundRefinement( std::unique_ptr<Query> inputQuery,
                          NLR::NetworkLevelReasoner &_networkLevelReasoner );
    void build( const Query &inputQuery, const NLR::NetworkLevelReasoner &_networkLevelReasoner );

    void generate( const Query &inputQuery,
                   const NLR::NetworkLevelReasoner &_networkLevelReasoner );

    inline bool getIsActivationBeforeOutput()
    {
        return _isActivationBeforeOutput;
    }

    inline bool getIsAddingAuxiliaryLayer()
    {
        return _isAddingAuxiliaryLayer;
    }

    inline unsigned getNumberOfOrConditions()
    {
        return _numberOfOrConditions;
    }

    inline unsigned getNumberOfLinearLayers()
    {
        return _numberOfLinearLayers;
    }

    inline Map<unsigned, Vector<Vector<Vector<Interval>>>> getPostConditions()
    {
        return _postConditions;
    }

    inline Vector<bool> getHasPostConditions()
    {
        return _hasPostConditions;
    }

private:
    unsigned _numberOfOrConditions = 0;
    unsigned _numberOfLinearLayers = 0;
    bool _isDisjunctivePostCondition = false;
    bool _isActivationBeforeOutput = false;
    bool _isAddingAuxiliaryLayer = true;

    // key: layer index,
    // value:
    // 1. Vector<double> := the coefficients of the post-condition (1 post-condition)
    // 2. Vector<Vector<double>> := the clause with AND operator.
    // TODO: it can have multiple disjunctive conditions but only 1 conjunctive condition for each
    //       disjunctive clause.
    // 3. Vector<Vector<Vector<double>>> := the clause with OR operator.
    Map<unsigned, Vector<Vector<Vector<Interval>>>> _postConditions; // Changing the expression
                                                                     // from std::string to
                                                                     // Vector<double>
    // key: layer index,
    // value:
    // 1. Vector<double> := the bias for each expression with AND operators.
    // 2. Vector<Vector<double>> := the bias for each expression with OR operators.
    Map<unsigned, Vector<Vector<Interval>>> _biasVectors;
    Vector<bool> _hasPostConditions; // Whether the post-conditions for each layer exist

    // Pair layer index, it is for dealing residual networks.
    Map<unsigned, Vector<unsigned>> _residualPairLayers;

    void _initPostConditions( const Query &inputQuery,
                              const NLR::NetworkLevelReasoner &_networkLevelReasoner );
    void _buildRelations( const Query &inputQuery,
                          const NLR::NetworkLevelReasoner &_networkLevelReasoner );
    void _generateNewPostConditions( const Query &inputQuery,
                                     const NLR::NetworkLevelReasoner &_networkLevelReasoner );

    void freeMemoryIfNeeded();
};
} // namespace BP
#endif // __BackwardAnalysis_h__
