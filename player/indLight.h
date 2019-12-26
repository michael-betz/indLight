/*
 * indLight.h
 *
 *  Created on: Mar 4, 2017
 *      Author: michael
 */

#ifndef INDLIGHT_C_
#define INDLIGHT_C_
#include <inttypes.h>

void hexdump( uint8_t *data, uint16_t len );
void updateLeds();											// Blast out the global led buffer over UDP
uint8_t *putRgb( uint8_t *rgbPointer, rgb rgbColor );		// Puts the color in the main array and returns incremented pointer
void animatorSolidColor( rgb rgbColor );
void animatorRainbow( float setBrightness, float setSpeed, float setWavenumber );
void animatorSunrise( float seconds );
void animatorAuroa( float setCreateProp );
void animatorRollingArray(int slp);
void die( char *s );


#endif /* INDLIGHT_C_ */
