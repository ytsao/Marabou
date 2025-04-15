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

#include "AbstractSyntaxTree.h"
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

    bool boundChecking( const Query &inputQuery,
                        const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                        const unsigned int pcId,
                        const unsigned int layerId ) const;
    void build( const Query &inputQuery,
                const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                const Preprocessor &preprocessor );

    // All the properties that I need to implement the backward analysis.
    // _post_conditions;    -> inputQuery.equations
    // _vars;               -> inputQuery.getInputVariables() / inputQuery.getOutputVariables()
    // _network;            -> Layer.h
    // _true_label;         -> (maybe I dont need this actually)
    // _start_layer_id;     ->
    // _num_linear_layers;  ->

    struct Node
    {
        Node();
        Node( int _id, double _coefficient, std::string _term )
        {
            id = _id;
            coefficient = _coefficient;
            term = _term;
        }
        ~Node(){};

        int id;
        double coefficient;
        std::string term;
    };

    unsigned int _numberOfOrConditions = 0;
    unsigned int _numberOfLinearLayers = 0;
    bool _isDisjunctivePostCondition = false;
    // Map<int, List<List<std::string>>> _postConditions;
    Map<int, std::vector<std::vector<std::string>>> _postConditions;

    // Map<std::string, List<Node>> _vars;
    Map<std::string, std::vector<Node>> _vars;
    Map<unsigned int, unsigned int> _mergedVar2OriginalVar;

    void _initPostConditions( const Query &inputQuery,
                              const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                              const Preprocessor &preprocessor );
    void _buildRelations( const Query &inputQuery,
                          const NLR::NetworkLevelReasoner &_networkLevelReasoner );
    void _generateNewPostConditions();
    void dump() const;
};
} // namespace BP
#endif // __BackwardAnalysis_h__