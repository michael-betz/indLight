#ifndef COLOR_MAPS_C_
#define COLOR_MAPS_C_

typedef struct {
    float r;       // a fraction between 0 and 1
    float g;       // a fraction between 0 and 1
    float b;       // a fraction between 0 and 1
} rgb;

typedef struct {
    float h;       // a fraction between 0 and 1
    float s;       // a fraction between 0 and 1
    float v;       // a fraction between 0 and 1
} hsv;

float colorLimit( float x );
rgb gammaCorrect( rgb in, float gammaFact );
rgb rgbAdd( rgb in1, rgb in2 );
// str = "0.1, 0, 1.0", returns remainder string or NULL
char *strToRgb(char *str, rgb *dest);
hsv rgb2hsv(rgb in);
rgb hsv2rgb(hsv in);

#endif
