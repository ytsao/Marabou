/*********************                                                        */
/*! \file AbstractSyntaxTree.cpp
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

#include "AbstractSyntaxTree.h"

#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <queue>
#include <sstream>
#include <stack>

// LiteralNode
LiteralNode::LiteralNode( double value )
    : _value( value )
{
}

Interval LiteralNode::evaluate( const ASTEvaluator *evaluator ) const
{
    return Interval( _value, _value );
}

// VariableNode
VariableNode::VariableNode( const std::string &name )
    : _name( name )
{
}

Interval VariableNode::evaluate( const ASTEvaluator *evaluator ) const
{
    return evaluator->getVariableValue( _name );
}

void VariableNode::print( unsigned indent ) const
{
    std::string padding( indent, ' ' );
    std::cout << padding << "Variable: " << _name << std::endl;
}

// BinaryOperationNode
BinaryOperationNode::BinaryOperationNode( OpType op,
                                          std::unique_ptr<ASTNode> left,
                                          std::unique_ptr<ASTNode> right )
    : _op( op )
    , _left( std::move( left ) )
    , _right( std::move( right ) )
{
}

Interval BinaryOperationNode::evaluate( const ASTEvaluator *evaluator ) const
{
    Interval leftInterval = _left->evaluate( evaluator );
    Interval rightInterval = _right->evaluate( evaluator );

    switch ( _op )
    {
    case ADD:
        return leftInterval + rightInterval;
    case SUBTRACT:
        return leftInterval - rightInterval;
    case MULTIPLY:
        return leftInterval * rightInterval;
    default:
        throw std::runtime_error( "Unknown operation type" );
    }
}

void BinaryOperationNode::print( unsigned indent ) const
{
    std::string padding( indent, ' ' );
    std::cout << padding << "Binary Operation: ";
    switch ( _op )
    {
    case ADD:
        std::cout << "+" << std::endl;
        break;
    case SUBTRACT:
        std::cout << "-" << std::endl;
        break;
    case MULTIPLY:
        std::cout << "*" << std::endl;
        break;
    default:
        throw std::runtime_error( "Unknown operation type" );
    }
    _left->print( indent + 2 );
    _right->print( indent + 2 );
}

// UnaryOperationNode
UnaryOperationNode::UnaryOperationNode( OpType op, std::unique_ptr<ASTNode> operand )
    : _op( op )
    , _operand( std::move( operand ) )
{
}

Interval UnaryOperationNode::evaluate( const ASTEvaluator *evaluator ) const
{
    Interval operandInterval = _operand->evaluate( evaluator );

    switch ( _op )
    {
    case NEGATE:
        return -operandInterval;
    case ABS:
        return Interval( std::abs( operandInterval.getLowerBound() ),
                         std::abs( operandInterval.getUpperBound() ) );
    case RELU:
        return Interval( std::max( 0.0, operandInterval.getLowerBound() ),
                         std::max( 0.0, operandInterval.getUpperBound() ) );
    default:
        throw std::runtime_error( "Unknown operation type" );
    }
}

void UnaryOperationNode::print( unsigned indent ) const
{
    std::string padding( indent, ' ' );
    std::cout << padding;

    switch ( _op )
    {
    case NEGATE:
        std::cout << "Operation: negate";
        break;
    case ABS:
        std::cout << "Operation: abs";
        break;
    case RELU:
        std::cout << "Operation: ReLU";
        break;
    default:
        std::cout << "Unknown operation";
        break;
    }

    std::cout << std::endl;
    _operand->print( indent + 2 );
}

// ASTEvaluator
ASTEvaluator::ASTEvaluator()
    : _variables( nullptr )
    , _isLastLayer( false )
{
}

ASTEvaluator::ASTEvaluator( const std::map<std::string, Interval> *variables, bool isLastLayer )
    : _variables( variables )
    , _isLastLayer( isLastLayer )
{
}

ASTEvaluator::~ASTEvaluator()
{
}

Interval ASTEvaluator::getVariableValue( const std::string &name ) const
{
    if ( !_variables )
    {
        throw std::runtime_error( "No variables provided to evaluator" );
    }

    auto it = _variables->find( name );
    if ( it == _variables->end() )
    {
        throw std::runtime_error( "Variable not found: " + name );
    }

    return it->second;
}

Interval ASTEvaluator::evaluate( const char *expression )
{
    // Parse the expression and build the AST
    std::unique_ptr<ASTNode> root = parse( expression );

    // Evaluate the AST
    _result = root->evaluate( this );

    return _result;
}

void ASTEvaluator::printResult() const
{
    std::cout << "Result: [" << _result.getLowerBound() << ", " << _result.getUpperBound() << "]"
              << std::endl;
}

// This is a simple parser implementation.
// For a real application, you might want to use a proper parser generator like ANTLR.
std::unique_ptr<ASTNode> ASTEvaluator::parse( const char *expression )
{
    // This is a placeholder for a real parser
    // In a real implementation, you would parse the expression string
    // and construct an AST from it.

    // For demonstration purposes, let's assume we're parsing a simple expression
    // like "x + 5" or "ReLU(x - 2)"

    // Simplified parsing logic here...

    // For now, just return a dummy node
    return std::make_unique<LiteralNode>( 0.0 );
}