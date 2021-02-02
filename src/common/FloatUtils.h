/*********************                                                        */
/*! \file FloatUtils.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __FloatUtils_h__
#define __FloatUtils_h__

#include "Debug.h"
#include "GlobalConfiguration.h"
#include "MString.h"

#include <cfloat>
#include <cmath>

#ifdef _WIN32
#undef max
#undef min
#endif

class FloatUtils
{
public:
    static bool areEqual( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS );
    static String doubleToString( double x,
                                  unsigned precision = GlobalConfiguration::DEFAULT_DOUBLE_TO_STRING_PRECISION );

    inline static bool isNan( double x )
    {
        return std::isnan( x );
    }

    inline static bool isInf( double x )
    {
        return std::isinf( x );
    }

    static bool isZero( double x, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        ASSERT( epsilon > 0 );                                                                                                                
        double lower = -epsilon; 
        double upper = epsilon;                                                                                                               
        return ( x - upper ) * ( x - lower ) <= 0;
    }

    inline static bool isPositive( double x, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        ASSERT( epsilon > 0 ); 
        return x > epsilon;
    }

    inline static bool isNegative( double x, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        ASSERT( epsilon > 0 ); 
        return x < -epsilon;
    }

    inline static double abs( double x )
    {
        return fabs( x );
    }

    static bool areDisequal( double x,
                             double y,
                             double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return !areEqual( x, y, epsilon );
    }

    inline static double roundToZero( double x, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return isZero( x, epsilon ) ? 0.0 : x;
    }

    inline static bool gt( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return isPositive( x - y, epsilon );
    }

    inline static bool gte( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return !isNegative( x - y, epsilon );
    }

    inline static bool lt( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return gt( y, x, epsilon );
    }

    inline static bool lte( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return gte( y, x, epsilon );
    }

    inline static double min( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return lt( x, y, epsilon ) ? x : y;
    }

    inline static double max( double x, double y, double epsilon = GlobalConfiguration::DEFAULT_EPSILON_FOR_COMPARISONS )
    {
        return gt( x, y, epsilon ) ? x : y;
    }

    inline static double infinity()
    {
        return DBL_MAX;
    }

    inline static double negativeInfinity()
    {
        return -DBL_MAX;
    }

    inline static bool isFinite( double x )
    {
        return ( x != infinity() ) && ( x != negativeInfinity() );
    }

    inline static bool wellFormed( double x )
    {
        return !isNan( x ) && !isInf( x );
    }

};

#endif // __FloatUtils_h__
