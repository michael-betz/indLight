#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "colorMaps.h"
#include "indLight.h"

float colorLimit( float x ){
    float y;
    y = (x>1.0) ? 1.0 : x;
    y = (x<0.0) ? 0.0 : x;
    if( x>1.0 || x<0.0 ){
        printf("colorLimit( %f ) %f\n", x, y );
    }
    return( y );
}

rgb gammaCorrect( rgb in, float gammaFact ){
    rgb out;
    out.r = pow( colorLimit(in.r), gammaFact );
    out.g = pow( colorLimit(in.g), gammaFact );
    out.b = pow( colorLimit(in.b), gammaFact );
    return out;
}

rgb rgbAdd( rgb in1, rgb in2 ){
    rgb temp;
    temp.r = colorLimit( in1.r + in2.r );
    temp.g = colorLimit( in1.g + in2.g );
    temp.b = colorLimit( in1.b + in2.b );
    return temp;
}

char *strToRgb(char *str, rgb *dest) {
    // str = "0.1, 0, 1.0"
    char *strToken;
    dest->r = 0; dest->g = 0; dest->b = 0;
    strToken = strsep(&str, ","); if (strToken == 0) {return NULL;}
    dest->r = colorLimit(atof(strToken));
    strToken = strsep(&str, ","); if (strToken == 0) {return NULL;}
    dest->g = colorLimit(atof(strToken));
    strToken = strsep(&str, ","); if (strToken == 0) {return NULL;}
    dest->b = colorLimit(atof(strToken));
    return str;
}

hsv rgb2hsv(rgb in)
{
    hsv         out;
    float       min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0
        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}

rgb hsv2rgb(hsv in)
{
    float       hh, p, q, t, ff;
    long        i;
    rgb         out;

    in.s = colorLimit( in.s );
    in.v = colorLimit( in.v );

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh  = in.h - ((long)in.h);   // Take fractional part (0-1 range)
    //hh  = 1.0 - hh;              // Invert color order (to match wiki picture)
    hh *= 6.0;                   // Change to 0 - 6 range
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}
