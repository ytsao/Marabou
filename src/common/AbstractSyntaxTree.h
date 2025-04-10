/*********************                                                        */
/*! \file AbstractSyntaxTree.h
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

#ifndef __AbstractSyntaxTree_h__
#define __AbstractSyntaxTree_h__

#include "Interval.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class ASTNode;
class ASTEvaluator;

class ASTNode
{
public:
    virtual ~ASTNode() = default;

    // Evaluate the node to produce an interval result
    virtual Interval evaluate( const ASTEvaluator *evaluator ) const = 0;

    // For debugging; print the expression tree
    virtual void print( unsigned indent = 0 ) const = 0;
};

// Leaf node for literal values
class LiteralNode : public ASTNode
{
public:
    explicit LiteralNode( double value );
    Interval evaluate( const ASTEvaluator *evaluator ) const override;
    void print( unsigned indent = 0 ) const override;

private:
    double _value;
};

// Leaf node for variables
class VariableNode : public ASTNode
{
public:
    explicit VariableNode( const std::string &name );
    Interval evaluate( const ASTEvaluator *evaluator ) const override;
    void print( unsigned indent = 0 ) const override;

private:
    std::string _name;
};

// Binary operation node (addition, subtraction, multiplication, etc.)
class BinaryOperationNode : public ASTNode
{
public:
    enum OpType {
        ADD,
        SUBTRACT,
        MULTIPLY,
    };

    BinaryOperationNode( OpType op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right );
    Interval evaluate( const ASTEvaluator *evaluator ) const override;
    void print( unsigned indent = 0 ) const override;

private:
    OpType _op;
    std::unique_ptr<ASTNode> _left;
    std::unique_ptr<ASTNode> _right;
};

// Unary operation node (negation, function, etc.)
class UnaryOperationNode : public ASTNode
{
public:
    enum OpType {
        NEGATE,
        ABS,
        RELU,
    };

    UnaryOperationNode( OpType op, std::unique_ptr<ASTNode> operand );
    Interval evaluate( const ASTEvaluator *evaluator ) const override;
    void print( unsigned indent = 0 ) const override;

private:
    OpType _op;
    std::unique_ptr<ASTNode> _operand;
};

// Evaluate class to interpret the AST
class ASTEvaluator
{
public:
    ASTEvaluator();
    ASTEvaluator( const std::map<std::string, Interval> *variables, bool isLastLayer );
    ~ASTEvaluator();

    // Parse and evaluate an expression
    Interval evaluate( const char *expression );

    // Get a variable's value
    Interval getVariableValue( const std::string &name ) const;

    // For debugging
    void printResult() const;

private:
    // Parse expression and build AST
    std::unique_ptr<ASTNode> parse( const char *expression );

    // Variables available during evaluation
    const std::map<std::string, Interval> *_variables;
    bool _isLastLayer;

    // Current result
    Interval _result;
};

#endif // __AbstractSyntaxTree_h__