/*********************                                                        */
/*! \file FixedPointLoop.h
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

#ifndef __FixedPointPropagation_h__
#define __FixedPointPropagation_h__

#include "BackwardAnalysis.h"

namespace FPP {
class FixedPointPropagation
{
public:
    FixedPointPropagation( unsigned int currentStartLayer,
                           const BP::BackPropagation &backPropagation );
    ~FixedPointPropagation();

    bool iterate( NLR::NetworkLevelReasoner &_networkLevelReasoner );

private:
    unsigned int _currentStartLayer = 0;
    BP::BackPropagation _backPropagation;
};

} // namespace FPP
#endif // __FixedPointPropagation_h__
