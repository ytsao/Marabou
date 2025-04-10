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

class BackPropagation
{
public:
    BackPropagation();
    ~BackPropagation();
    bool bound_checking() const;
    void build();

private:
    // All the properties that I need to implement the backward analysis.
    // _post_conditions;    ->
    // _vars;               ->
    // _network;            -> Layer.h
    // _true_label;         -> (maybe I dont need this actually)
    // _start_layer_id;     ->
    // _num_linear_layers;  ->

    void _init_post_conditions() const;
    void _build_relation() const;
    void _print() const;
    void _generate_new_post_conditions() const;
};
#endif // __BackwardAnalysis_h__