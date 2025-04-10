/*********************                                                        */
/*! \file Interval.h
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

#ifndef __Interval_h__
#define __Interval_h__

class Interval
{
public:
    Interval();
    Interval( double lb, double ub );
    ~Interval();

    // overwrite the arithmetic operators for Interval.
    Interval operator+( const Interval &other ) const;
    Interval operator-() const; // unary minus, similar to __neg__ in Python.
    Interval operator-( const Interval &other ) const;
    Interval operator*( const Interval &other ) const;
    Interval &operator+=( const Interval &other );
    Interval &operator-=( const Interval &other );
    Interval &operator*=( const Interval &other );

    // overwrite the binary operators for Interval.
    bool operator<( const Interval &other ) const;
    bool operator<=( const Interval &other ) const;
    bool operator>( const Interval &other ) const;
    bool operator>=( const Interval &other ) const;

    double getLowerBound() const;
    double getUpperBound() const;


private:
    double _lb;
    double _ub;
};

#endif // __Interval_h__
