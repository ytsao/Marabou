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
                        const unsigned int layerId ) const;
    void boundRefinement(); // TODO: maybe we need to use this to improve the bounds from DeepPoly &
                            // Interval
    void build( const Query &inputQuery,
                const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                const Preprocessor &preprocessor );

    inline bool getIsActivationBeforeOutput()
    {
        return _isActivationBeforeOutput;
    }

    inline unsigned int getNumberOfOrConditions()
    {
        return _numberOfOrConditions;
    }

    inline unsigned int getNumberOfLinearLayers()
    {
        return _numberOfLinearLayers;
    }

    inline List<std::string> getOutputVariables()
    {
        return _outputVariables;
    }

    inline Map<unsigned int, Vector<Vector<std::string>>> getPostConditions()
    {
        return _postConditions;
    }

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

    void dump() const;

private:
    unsigned int _numberOfOrConditions = 0;
    unsigned int _numberOfLinearLayers = 0;
    bool _isDisjunctivePostCondition = false;
    bool _isActivationBeforeOutput = false;
    List<std::string> _outputVariables;
    Map<unsigned int, Vector<Vector<std::string>>> _postConditions;

    Map<std::string, Vector<Node>> _vars;

    void _initPostConditions( const Query &inputQuery,
                              const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                              const Preprocessor &preprocessor );
    void _buildRelations( const Query &inputQuery,
                          const NLR::NetworkLevelReasoner &_networkLevelReasoner );
    void _generateNewPostConditions( const Query &inputQuery,
                                     const NLR::NetworkLevelReasoner &_networkLevelReasoner );
};
} // namespace BP
#endif // __BackwardAnalysis_h__
