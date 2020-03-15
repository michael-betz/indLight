/*
 * indLight.c
 *
 *  Created on: Mar 4, 2017
 *      Author: michael
 */

#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <mosquitto.h>
#include "perlin.h"
#include "colorMaps.h"
#include "indLight.h"

struct sockaddr_in g_sockStruct;
int g_socketHandle;

#define FPS 30          // Frames Per Second
#define N_LEDS 100
#define N_PAYL 3*N_LEDS + 5

#define GAMMA_AURORA    1.1
#define GAMMA_RAINBOW   2.8
#define GAMMA_SOLID     1.1
#define GAMMA_SUNRISE   2.8
#define GAMMA_ROLL      1.1

#define ROLL_ARRAY_SIZE 256
unsigned roll_max_size = ROLL_ARRAY_SIZE;
rgb g_rollingArray[ROLL_ARRAY_SIZE];

float g_gammaFact = GAMMA_SOLID;
uint8_t g_payLoadBuffer[ N_PAYL ];
uint8_t *g_rgbArray = &g_payLoadBuffer[5];
rgb black = {0,0,0};
rgb white = {1,1,1};

// Random float between 0.0 and a
#define randFloat(a) ((float)rand()/(float)(RAND_MAX/a))

char *MOS_PATH = "indLight/#";          //SUbscribe to anything starting with indLight

int keepRunning = true;
struct mosquitto *mos = NULL;

typedef enum {
    OFF, SOLID_COLOR, RAINBOW, AURORA, SUNRISE, ROLL_ARRAY, PERLIN
} animationState_t;
animationState_t g_animationState = OFF;

void intHandler( int dummy ){
    printf("\n... Shutting down! \n");
    keepRunning = false;
}

void callbackLog(struct mosquitto *mosq, void *obj, int level, const char *str){
    char dateBuffer[100];
    //Output all logging messages which are >= Warning
    if( (level==MOSQ_LOG_WARNING) | (level==MOSQ_LOG_ERR) | (level==MOSQ_LOG_NOTICE) ){
        //Build a time string
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(dateBuffer, sizeof(dateBuffer)-1, "%d.%m.%Y %H:%M:%S", t);
        printf("[%s]  %s\n", dateBuffer, str);
    }
}

// Print an error message and bite the dust
void die( char *s ){
    perror( s );
    exit( -1 );
}

// Print a pretty hex-dump on the debug out
void hexDump( uint8_t *buffer, uint16_t nBytes ){
    #define BYTES_PER_LINE 18
    for( uint16_t i=0; i<nBytes; i++ ){
        if( (nBytes>BYTES_PER_LINE) && ((i%BYTES_PER_LINE)==0) ){
            printf("\n    %04x: ", i);
        }
        printf("%02x ", *buffer++);
    }
    printf("\n");
}

// Executed when a mosquitto message is received
void onMosMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message ){
    float tempBrightness, tempSpeed, tempWavenumber, tempPersistence;
    int tempOctaves;
    unsigned i;
    // mqtt stuff
    char *topic = message->topic, *strToken;
    char *payload = message->payload;
    bool compareResult;
    printf("Received topic %s payload %s \n", topic, payload);

    //----------------------------------------------------------
    // rollArray: "usleep_value [ms], 1.0, 0.5, 0.2,  1.0, 0.5, 0.2 ..." up to 256 color triples
    //----------------------------------------------------------
    mosquitto_topic_matches_sub("indLight/rollArray", topic, &compareResult);
    if (compareResult) {
        char *strToken;
        strToken = strsep(&payload, ","); if (strToken == 0) {return;}
        int usleep_val = atoi(strToken) * 1000;
        rgb *dest = g_rollingArray;
        for (i=0; i<ROLL_ARRAY_SIZE; i++) {
            payload = strToRgb(payload, dest);
            printf("dest[%d] = [r=%5.3f, g=%5.3f, b=%5.3f]\n", i, dest->r, dest->g, dest->b);
            dest++;
            if (payload == NULL)
                break;
        }
        roll_max_size = i + 1;
        g_animationState = ROLL_ARRAY;
        g_gammaFact = GAMMA_ROLL;
        animatorRollingArray(usleep_val);
    }

    //----------------------------------------------------------
    // solidColor: sth. like "1.0, 0.5, 0.2"
    //----------------------------------------------------------
    mosquitto_topic_matches_sub( "indLight/solidColor", topic, &compareResult );
    if (compareResult) {
        rgb temp;
        strToRgb(payload, &temp);
        animatorSolidColor(temp);
        g_animationState = SOLID_COLOR;
        g_gammaFact = GAMMA_SOLID;
    }

    //----------------------------------------------------------
    // Perlin: 0.7, 5, 1 (persistence, octaves, brightness)
    //----------------------------------------------------------
    mosquitto_topic_matches_sub( "indLight/perlin", topic, &compareResult );
    if (compareResult) {
        g_animationState = PERLIN;
        g_gammaFact = GAMMA_ROLL;
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempPersistence = atof( strToken );
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempOctaves = atoi( strToken );
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempBrightness = atof( strToken );
        animatorPerlin(tempPersistence, tempOctaves, tempBrightness);
    }

    //----------------------------------------------------------
    // rainbow: 0.02, 2e-4, 2 (brightness, speed, wavenumber)
    //----------------------------------------------------------
    mosquitto_topic_matches_sub( "indLight/rainbow", topic, &compareResult );
    if (compareResult) {
        g_animationState = RAINBOW;
        g_gammaFact = GAMMA_RAINBOW;
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempBrightness = atof( strToken );
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempSpeed = atof( strToken );
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempWavenumber = atof( strToken );
        animatorRainbow( tempBrightness, tempSpeed, tempWavenumber );
    }
    //----------------------------------------------------------
    // Sunrise: 300 (duration in seconds)
    //----------------------------------------------------------
    mosquitto_topic_matches_sub( "indLight/sunrise", topic, &compareResult );
    if( compareResult ){
        g_animationState = SUNRISE;
        g_gammaFact = GAMMA_SUNRISE;
        strToken = strsep( &payload, "," ); if( strToken == 0 ){ return; }
        tempSpeed = atof( strToken );
        animatorSunrise( tempSpeed );
    }
    //----------------------------------------------------------
    // aurora: no settings yet
    //----------------------------------------------------------
    mosquitto_topic_matches_sub( "indLight/aurora", topic, &compareResult );
    if( compareResult ){
        g_animationState = AURORA;
        g_gammaFact = GAMMA_AURORA;
        tempSpeed = atof( payload );   //actually the create propability
        animatorAuroa( tempSpeed );
    }
}

void mosConnect() {
    uint8_t retVal;
    if( mos ){
        mosquitto_disconnect( mos );
        mosquitto_destroy( mos );
        mos = NULL;
    }
    printf("Mosquitto: Starting up ... \n");
    mos = mosquitto_new( "indLight_to_MQQT", true, NULL );
    if( !mos ){
        die("Mosquitto: Could not start! Exiting!");
    }
    mosquitto_log_callback_set( mos, callbackLog );
    mosquitto_reconnect_delay_set( mos, 1, 60, true );
    retVal = mosquitto_connect( mos, "127.0.0.1", 1883, 10 );
    if( retVal!=MOSQ_ERR_SUCCESS ){
        die("Mosquitto: Error connecting to MQTT broker. Exiting!");
    }
    printf("Mosquitto: Connected!\n");
    retVal = mosquitto_subscribe( mos, NULL, MOS_PATH, 0 );
    if( retVal!=MOSQ_ERR_SUCCESS ){
        die("Mosquitto: Error Subscribing\n");
    }
    mosquitto_message_callback_set( mos, onMosMessage );
    printf("Mosquitto: Subscribed to %s\n", MOS_PATH );
}

void initUdp( char *IP, int PORT ){
    g_payLoadBuffer[0] = 0; // Function code
    g_payLoadBuffer[1] = 0; // Offset
    g_payLoadBuffer[2] = 0; // Offset
    g_payLoadBuffer[3] = N_LEDS>>8; // NLEDs
    g_payLoadBuffer[4] = N_LEDS&0xFF;   // NLEDs
    //create a UDP socket
    if ((g_socketHandle = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) == -1){
        die("UDP socket problem");
    }
    memset((char *) &g_sockStruct, 0, sizeof(g_sockStruct));
    g_sockStruct.sin_family = AF_INET;
    g_sockStruct.sin_port = htons(PORT);
    if ( inet_aton(IP, &g_sockStruct.sin_addr) == 0 ) {
        die("inet_aton() failed");
    }
}

// Blast out the global led buffer over UDP
void updateLeds(){
    int retVal;
    retVal = sendto( g_socketHandle,
                     g_payLoadBuffer,
                     N_PAYL,
                     0,
                     (struct sockaddr*) &g_sockStruct,
                     sizeof(g_sockStruct) );
    if ( retVal == -1 ){
        die("updateLeds()");
    }
}

// Puts the gamma corected color in the main array and returns incremented pointer
uint8_t *putRgb(uint8_t *rgbPointer, rgb rgbColor){
    rgbColor = gammaCorrect(rgbColor, g_gammaFact);

    // printf("rgbColor = [r=%5.3f, g=%5.3f, b=%5.3f]\n", rgbColor.r, rgbColor.g, rgbColor.b);
    // printf(
    //     "putRgb([%d, %d, %d])\n",
    //     (uint8_t)(colorLimit(rgbColor.b)*255),
    //     (uint8_t)(colorLimit(rgbColor.r)*255),
    //     (uint8_t)(colorLimit(rgbColor.g)*255)
    // );

    *rgbPointer++ = (uint8_t)(colorLimit(rgbColor.r)*255);
    *rgbPointer++ = (uint8_t)(colorLimit(rgbColor.g)*255);
    *rgbPointer++ = (uint8_t)(colorLimit(rgbColor.b)*255);
    return rgbPointer;
}

void animatorPerlin(float setPersistence, int setOctaves, float setBrightness)
{
    hsv hsvColor;
    float n1, n2;
    uint8_t *arrPointer = g_rgbArray;
    static float persistence=0.7, brightness=0.1;
    static int octaves=5;
    static uint32_t tick=0, seed1=0, seed2=0;

    if (tick == 0) {
        seed1 = rand();
        seed2 = rand();
    }
    if (setPersistence > 0) persistence = setPersistence;
    if (setOctaves > 0) octaves = setOctaves;
    if (setBrightness >= 0) brightness = setBrightness;

    for (int i=0; i<N_LEDS; i++) {
        n1 = pnoise2d(i * 0.1, tick * 0.005, persistence, octaves, seed1);
        n2 = pnoise2d(i * 0.1, tick * 0.005, persistence, octaves, seed2);
        n1 = n1 / 2.0 + 0.5;
        n2 = n2 / 2.0 + 0.5;
        if (n1 > 1) n1 = 1;
        if (n1 < 0) n1 = 0;
        if (n2 > 1) n2 = 1;
        if (n2 < 0) n2 = 0;
        hsvColor.h = n1;
        hsvColor.s = n2;
        hsvColor.v = brightness;
        arrPointer = putRgb(arrPointer, hsv2rgb(hsvColor));
    }
    tick++;
}


void animatorRainbow(float setBrightness, float setSpeed, float setWavenumber){
    static uint32_t tick = 0;
    static float waveNumber = 2;
    static float speed = 1.0/7000;
    static float brightness = 0.02;
    uint8_t *arrPointer = g_rgbArray;
    hsv hsvColor;
    if( setWavenumber > 0 ){
        waveNumber = setWavenumber;
    }
    if( setSpeed > 0 ){
        speed = setSpeed;
    }
    if( setBrightness > 0 ){
        brightness = setBrightness;
    }
    if( setWavenumber > 0 || setSpeed > 0 || setBrightness > 0 ){
        // Set config parameter only, dont put a frame
        printf("animatorRainbow( brightness=%.3e, speed=%.3e, waveNumber=%.3e )\n", brightness, speed, waveNumber );
    } else {
        // No config parameter set, Run a frame
        for ( int i=0; i<N_LEDS; i++ ){
            hsvColor.h = waveNumber*i/(N_LEDS-1) + tick*speed;
            hsvColor.s = 1;
            hsvColor.v = brightness;
            arrPointer = putRgb(arrPointer, hsv2rgb(hsvColor));
        }
        tick++;
    }
}

void animatorRollingArray(int slp)
{
    uint8_t *arrPointer = g_rgbArray;
    static unsigned startIndex = 0;
    static int usleep_val = 0;
    if (slp > 0) {
        printf("animatorRollingArray(usleep_val = %d)\n", slp);
        usleep_val = slp;
    }
    for (int i=0; i<N_LEDS; i++)
        arrPointer = putRgb(arrPointer, g_rollingArray[
            (startIndex + i) % roll_max_size
        ]);
    startIndex++;
    if (usleep_val > 0)
        usleep(usleep_val);
}

typedef struct {
    enum auoraState_t{
        idle,
        active
    } state;
    float position; // visible if within -1 ... 1
    float speed;    // -1 ... 1
    float acc;      // -1 ... 1
    float width;    // -1 ... 1
    hsv   hsvColor;
    float maxIntens;
}auoraParticle_t;

#define N_PARTICLES 48  //Max number of particles

float auroalimit( float in, float limit ){
    in = (in >  limit) ?  limit : in;
    in = (in < -limit) ? -limit : in;
    return in;
}

void drawParticle( rgb *buffer, auoraParticle_t *p ){
    if( p->hsvColor.v < p->maxIntens ){
        p->hsvColor.v += 2e-3;
    }
    for ( int i=0; i<N_LEDS; i++ ){
        float x = ((float)i) / (N_LEDS-1);
        hsv hsvTemp = p->hsvColor;
        rgb rgbTemp = *buffer;
        hsvTemp.v *= exp( -pow(x-(p->position+1)/2,2) * 300 * (5.1-p->width*5) );
        *buffer = rgbAdd( rgbTemp, hsv2rgb(hsvTemp) );
        buffer++;
    }
    // printf("[%5.2f,%5.2f,%5.2f] ", p->position, p->width, p->hsvColor.v );
}

#define dt (1.0/FPS)        // Timestep approximately in [s]

void animatorAuroa( float setCreateProp ){
    uint8_t act = 0;
    static float createProp = 1e-3;
    static uint32_t tick=0;
    static auoraParticle_t particles[N_PARTICLES];
    auoraParticle_t *p = particles;
    rgb outBuffer[N_LEDS];
    rgb *rgbPointer = outBuffer;
    uint8_t *arrPointer = g_rgbArray;
    if( setCreateProp >= 0 ){
        createProp = setCreateProp;
        printf("animatorAuroa( createProp=%f )\n",createProp);
        return;
    }
    // Initialize outBuffer
    for( int i=0; i<N_LEDS; i++ ){
        *rgbPointer++ = black;
    }
    for ( int i=0; i<N_PARTICLES; i++ ){
        switch( p->state ){
            case active:
                p->speed    += auroalimit( p->acc,   1.0 ) / 20.0 * dt;
                p->position += auroalimit( p->speed, 1.0 ) / 20.0 * dt;
                if( p->position != auroalimit( p->position, 1.3 ) ){
                    p->state = idle;
                    // printf( "X                 X ");
                } else {
                    drawParticle( outBuffer, p );
                    act++;
                }
            break;
            case idle:
                if( randFloat(1) < createProp ){
                    p->position  = randFloat(2) - 1;
                    p->speed     = randFloat(2) - 1;
                    p->acc       = randFloat(2) - 1;
                    p->hsvColor  = (hsv){ randFloat(1), 0.5+randFloat(0.5), 0 };
                    p->state = active;
                    p->width     = randFloat(1.0);
                    p->maxIntens = randFloat(0.3);
                    // printf( "N%5.2f,%5.2f,%5.2fN ", p->position, p->width, p->maxIntens );
                } else {
                    // printf( "[                 ] ");
                }
            break;
            default:
                die("animatorAuroa(): snap!");
        }
        p++;
    }
    rgbPointer = outBuffer;
    for( int i=0; i<N_LEDS; i++ ){
        arrPointer = putRgb( arrPointer, *rgbPointer++ );
    }
    // printf(" %d\n", act);
    tick++;
}

void animatorSunrise( float seconds ){
    static uint32_t tick=0;
    static float maxTicks=30*60*FPS;    // 30 Minutes at 30 FPS = 54000 ticks
    uint8_t *arrPointer = g_rgbArray;
    float t;
    hsv hsvColor;
    if( seconds>0 ){
        // Set config parameter only, dont put a frame
        printf("animatorSunrise( seconds=%.3e )\n", seconds );
        maxTicks = seconds * FPS;
        tick = 0;
    } else {
        // No config parameter set, Run a frame
        t = (float)tick / maxTicks; // Time runs from 0.0 to 1.0
        for ( int i=0; i<N_LEDS; i++ ){
            hsvColor.h = t / 7.0;
            hsvColor.s = 1.0 - pow(t,6);
            hsvColor.v = t * pow( (1-t)*sin((float)i*M_PI/(N_LEDS-1))+t, 2 );
            arrPointer = putRgb( arrPointer, hsv2rgb(hsvColor) );
        }
        if( tick >= maxTicks ){
            animatorSolidColor( white );
            g_animationState = SOLID_COLOR;
        }
        tick++;
    }
}

void animatorSolidColor(rgb rgbColor){
    uint8_t *arrPointer = g_rgbArray;
    printf("animatorSolidColor(r=%5.3f, g=%5.3f, b=%5.3f)\n", rgbColor.r, rgbColor.g, rgbColor.b);
    for( int i=0; i<N_LEDS; i++ ){
        arrPointer = putRgb(arrPointer, rgbColor);
    }
}

int main(int argc, char **argv) {
    if( argc != 3 ){
        die( "Wut? Try ./indLightDaemon 192.168.1.200 2711");
    }
    char *IP = argv[1];
    int PORT = atoi( argv[2] );
    srand(time(NULL));
    initUdp( IP, PORT );
    printf( "Opened UDP on %s %d\n", IP, PORT );
    mosquitto_lib_init();
    mosConnect();
    // mosquitto_loop_start( mos );         //Start mosquitto RX daemon thread
    // animatorRainbow( -1, randFloat(10)/10000.0, randFloat(3) );
    while(1){
        switch( g_animationState ){
            case SUNRISE:
                animatorSunrise(-1);
              break;
            case RAINBOW:
                animatorRainbow(-1, -1, -1);
              break;
            case AURORA:
                animatorAuroa(-1);
                break;
            case PERLIN:
                animatorPerlin(-1, -1, -1);
                break;
            case ROLL_ARRAY:
                animatorRollingArray(-1);
                break;
            case SOLID_COLOR:               // No framebuffer updates
                break;
            default:
                animatorSolidColor(black);
                g_animationState = SOLID_COLOR;
        }
        updateLeds();
        //hexDump( g_rgbArray, 18 );
        if (mosquitto_loop(mos, 0, 1) != MOSQ_ERR_SUCCESS) {
            die("Mosquitto_loop error");
        }
        usleep(1000000 / FPS);
    }

    return (0);
}
