/*********************                                                        */
/*! \file Interval.cpp
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

#include "Interval.h"

#include <algorithm>
#include <cmath>

Interval::Interval()
    : _lb( 0 )
    , _ub( 0 )
{
}

Interval::Interval( double lb, double ub )
    : _lb( lb )
    , _ub( ub )
{
}

Interval::~Interval()
{
}

Interval Interval::operator+( const Interval &other ) const
{
    return Interval( _lb + other._lb, _ub + other._ub );
}

Interval Interval::operator-() const
{
    return Interval( -_ub, -_lb );
}

Interval Interval::operator-( const Interval &other ) const
{
    return Interval( _lb - other._ub, _ub - other._lb );
}

Interval Interval::operator*( const Interval &other ) const
{
    double products[4] = {
        _lb * other._lb,
        _lb * other._ub,
        _ub * other._lb,
        _ub * other._ub,
    };
    double lb = *std::min_element( products, products + 4 );
    double ub = *std::max_element( products, products + 4 );
    return Interval( lb, ub );
}

Interval &Interval::operator+=( const Interval &other )
{
    _lb += other._lb;
    _ub += other._ub;
    return *this;
}

Interval &Interval::operator-=( const Interval &other )
{
    _lb -= other._ub;
    _ub -= other._lb;
    return *this;
}

Interval &Interval::operator*=( const Interval &other )
{
    *this = *this * other;
    return *this;
}

bool Interval::operator<( const Interval &other ) const
{
    return _ub < other._lb;
}

bool Interval::operator<=( const Interval &other ) const
{
    return _ub <= other._lb;
}

bool Interval::operator>( const Interval &other ) const
{
    return _lb > other._ub;
}

bool Interval::operator>=( const Interval &other ) const
{
    return _lb >= other._ub;
}

double Interval::getLowerBound() const
{
    return _lb;
}

double Interval::getUpperBound() const
{
    return _ub;
}