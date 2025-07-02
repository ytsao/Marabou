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

    bool boundChecking( const Query &inputQuery,
                        const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                        const unsigned int layerId );
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

    inline Map<unsigned int, Vector<Vector<Vector<double>>>> getPostConditions()
    {
        return _postConditions;
    }

    inline std::vector<bool> getHasPostConditions()
    {
        return _hasPostConditions;
    }

    void freeMemoryIfNeeded();


private:
    unsigned int _numberOfOrConditions = 0;
    unsigned int _numberOfLinearLayers = 0;
    bool _isDisjunctivePostCondition = false;
    bool _isActivationBeforeOutput = false;

    // key: OR condition index,
    // value:
    // 1. Vector<double> := the coefficients of the post-condition (1 post-condition)
    // 2. Vector<Vector<double>> := the collection of the post-conditions in single layer (1+
    // post-conditions)
    // 3. Vector<Vector<Vector<double>>> := the collection of the post-conditions in all layers (1+
    // post-conditions)
    Map<unsigned int, Vector<Vector<Vector<double>>>> _postConditions; // Changing the expression
                                                                       // from std::string to
                                                                       // Vector<double>
    Map<unsigned int, Vector<Vector<double>>> _biasVectors;
    std::vector<bool> _hasPostConditions; // Whether the post-conditions for each layer exist

    // Vector<Interval> _variables; // The variables in the current layer, the size is the maximum
    // size of the layer

    // Map<std::string, Vector<Node>> _vars;

    void _initPostConditions( const Query &inputQuery,
                              const NLR::NetworkLevelReasoner &_networkLevelReasoner,
                              const Preprocessor &preprocessor );
    void _buildRelations( const Query &inputQuery,
                          const NLR::NetworkLevelReasoner &_networkLevelReasoner );
    void _generateNewPostConditions( const Query &inputQuery,
                                     const NLR::NetworkLevelReasoner &_networkLevelReasoner );

    // void freeMemoryIfNeeded();
};
} // namespace BP
#endif // __BackwardAnalysis_h__
