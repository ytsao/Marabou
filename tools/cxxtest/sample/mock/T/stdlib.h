#ifndef __T__STDLIB_H
#define __T__STDLIB_H

#include <cxxtest/Mock.h>
#include <stdlib.h>
#include <time.h>

CXXTEST_MOCK_VOID_GLOBAL( srand, ( unsigned seed ), ( seed ) );
CXXTEST_MOCK_GLOBAL( int, rand, (void), () );
CXXTEST_MOCK_GLOBAL( time_t, time, ( time_t * t ), ( t ) );

#endif // __T__STDLIB_H
