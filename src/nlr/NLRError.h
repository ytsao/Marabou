/*********************                                                        */
/*! \file NLRError.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2024 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __NLRError_h__
#define __NLRError_h__

#include "Error.h"

class NLRError : public Error
{
public:
    enum Code {
        UNEXPECTED_RETURN_STATUS_FROM_GUROBI = 0,
        LAYER_TYPE_NOT_SUPPORTED = 1,
        INPUT_LAYER_NOT_THE_FIRST_LAYER = 2,
        LEAKY_RELU_SLOPES_NOT_UNIFORM = 3,
        RELU_NOT_FOUND = 4,
        LAYER_NOT_FOUND = 5
    };

    NLRError( NLRError::Code code )
        : Error( "NLRError", (int)code )
    {
    }

    NLRError( NLRError::Code code, const char *userMessage )
        : Error( "NLRError", (int)code, userMessage )
    {
    }
};

#endif // __NLRError_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
