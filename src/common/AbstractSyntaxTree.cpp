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


// LiteralNode
LiteralNode::LiteralNode( double value )
    : _value( value )
{
}

Interval LiteralNode::evaluate( const ASTEvaluator *evaluator ) const
{
    return Interval( _value, _value );
}

void LiteralNode::print( unsigned indent ) const
{
    std::string padding( indent, ' ' );
    std::cout << padding << "Literal: " << _value << std::endl;
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
    Interval product;

    switch ( _op )
    {
    case ADD:
        return leftInterval + rightInterval;
    case SUBTRACT:
        return leftInterval - rightInterval;
    case MULTIPLY:
        // TODO:
        //  it seems not correct,
        /* product = leftInterval * rightInterval; */
        /* if ( evaluator->getisLastLayer() ) */
        /*     return product; */
        /* return Interval( std::max( product.getLowerBound(), 0.0 ), */
        /*                  std::max( product.getUpperBound(), 0.0 ) ); */
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
        printf( "Here is a relu function.\n" );
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
{
}

ASTEvaluator::ASTEvaluator( const Map<std::string, Interval> *variables )
    : _variables( variables )
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

    if ( !_variables->exists( name ) )
    {
        throw std::runtime_error( "Variable not found: " + name );
    }

    return ( *_variables )[name];
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
    // // This is a placeholder for a real parser
    // // In a real implementation, you would parse the expression string
    // // and construct an AST from it.

    // // For demonstration purposes, let's assume we're parsing a simple expression
    // // like "x + 5" or "ReLU(x - 2)"

    // // Simplified parsing logic here...

    // // For now, just return a dummy node
    // return std::make_unique<LiteralNode>( 0.0 );

    std::string expr( expression );

    // Step 2 : Handle addition and subtraction
    size_t lastAddPos = 0, lastSubPos = 0;
    int depth = 0;

    for ( size_t i = 0; i < expr.length(); i++ )
    {
        if ( expr[i] == '(' )
            depth++;
        else if ( expr[i] == ')' )
            depth--;
        else if ( depth == 0 && expr[i] == '+' )
            lastAddPos = i;
        else if ( depth == 0 && expr[i] == '-' && i > 0 && !std::isdigit( expr[i - 1] ) &&
                  expr[i - 1] != ')' )
            lastSubPos = i;
    }

    if ( lastAddPos > 0 )
    {
        std::string leftExpr = expr.substr( 0, lastAddPos );
        std::string rightExpr = expr.substr( lastAddPos + 1 );

        // Trim whitespace
        leftExpr.erase( 0, leftExpr.find_first_not_of( " \t" ) );
        leftExpr.erase( leftExpr.find_last_not_of( " \t" ) + 1 );
        rightExpr.erase( 0, rightExpr.find_first_not_of( " \t" ) );
        rightExpr.erase( rightExpr.find_last_not_of( " \t" ) + 1 );

        std::unique_ptr<ASTNode> leftNode = parse( leftExpr.c_str() );
        std::unique_ptr<ASTNode> rightNode = parse( rightExpr.c_str() );

        return std::make_unique<BinaryOperationNode>(
            BinaryOperationNode::ADD, std::move( leftNode ), std::move( rightNode ) );
    }

    if ( lastSubPos > 0 )
    {
        std::string leftExpr = expr.substr( 0, lastSubPos );
        std::string rightExpr = expr.substr( lastSubPos + 1 );

        // Trim whitespace
        leftExpr.erase( 0, leftExpr.find_first_not_of( " \t" ) );
        leftExpr.erase( leftExpr.find_last_not_of( " \t" ) + 1 );
        rightExpr.erase( 0, rightExpr.find_first_not_of( " \t" ) );
        rightExpr.erase( rightExpr.find_last_not_of( " \t" ) + 1 );

        std::unique_ptr<ASTNode> leftNode = parse( leftExpr.c_str() );
        std::unique_ptr<ASTNode> rightNode = parse( rightExpr.c_str() );

        return std::make_unique<BinaryOperationNode>(
            BinaryOperationNode::SUBTRACT, std::move( leftNode ), std::move( rightNode ) );
    }

    // Step 3: Handle multiplication and division
    size_t lastMulPos = 0;
    depth = 0;

    for ( size_t i = 0; i < expr.length(); i++ )
    {
        if ( expr[i] == '(' )
            depth++;
        else if ( expr[i] == ')' )
            depth--;
        else if ( depth == 0 && expr[i] == '*' )
            lastMulPos = i;
    }

    if ( lastMulPos > 0 )
    {
        std::string leftExpr = expr.substr( 0, lastMulPos );
        std::string rightExpr = expr.substr( lastMulPos + 1 );

        // Trim whitespace
        leftExpr.erase( 0, leftExpr.find_first_not_of( " \t" ) );
        leftExpr.erase( leftExpr.find_last_not_of( " \t" ) + 1 );
        rightExpr.erase( 0, rightExpr.find_first_not_of( " \t" ) );
        rightExpr.erase( rightExpr.find_last_not_of( " \t" ) + 1 );

        std::unique_ptr<ASTNode> leftNode = parse( leftExpr.c_str() );
        std::unique_ptr<ASTNode> rightNode = parse( rightExpr.c_str() );

        return std::make_unique<BinaryOperationNode>(
            BinaryOperationNode::MULTIPLY, std::move( leftNode ), std::move( rightNode ) );
    }

    // Step 4: Handle unary operations
    // Check for ReLU
    if ( expr.substr( 0, 5 ) == "ReLU(" && expr[expr.length() - 1] == ')' )
    {
        std::string innerExpr = expr.substr( 5, expr.length() - 6 );
        std::unique_ptr<ASTNode> operand = parse( innerExpr.c_str() );

        return std::make_unique<UnaryOperationNode>( UnaryOperationNode::RELU,
                                                     std::move( operand ) );
    }

    // Check for abs
    if ( expr.substr( 0, 4 ) == "abs(" && expr[expr.length() - 1] == ')' )
    {
        std::string innerExpr = expr.substr( 4, expr.length() - 5 );
        std::unique_ptr<ASTNode> operand = parse( innerExpr.c_str() );

        return std::make_unique<UnaryOperationNode>( UnaryOperationNode::ABS,
                                                     std::move( operand ) );
    }

    // Check for negation
    if ( expr[0] == '-' )
    {
        std::string innerExpr = expr.substr( 1 );
        std::unique_ptr<ASTNode> operand = parse( innerExpr.c_str() );

        return std::make_unique<UnaryOperationNode>( UnaryOperationNode::NEGATE,
                                                     std::move( operand ) );
    }

    // Step 5: Handle parentheses
    if ( expr[0] == '(' && expr[expr.length() - 1] == ')' )
    {
        // Verify that these parentheses are actually matched
        int parenthesesDepth = 1;
        for ( size_t i = 1; i < expr.length() - 1; i++ )
        {
            if ( expr[i] == '(' )
                parenthesesDepth++;
            else if ( expr[i] == ')' )
                parenthesesDepth--;

            if ( parenthesesDepth == 0 )
                break; // These aren't matching parentheses
        }

        if ( parenthesesDepth == 1 ) // Matching parentheses
        {
            std::string innerExpr = expr.substr( 1, expr.length() - 2 );
            return parse( innerExpr.c_str() );
        }
    }

    // Step 6: Handle literals and variables
    // Trim whitespace
    expr.erase( 0, expr.find_first_not_of( " \t" ) );
    expr.erase( expr.find_last_not_of( " \t" ) + 1 );

    // Check if it's a number
    try
    {
        double value = std::stod( expr );
        return std::make_unique<LiteralNode>( value );
    }
    catch ( ... )
    {
        // If not a number, it's a variable
        return std::make_unique<VariableNode>( expr );
    }
}
