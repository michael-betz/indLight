#!/usr/bin/env python3
"""
    Sends UDP packets to the esp8266 on the indLight board which controls
    the WS2811 LED strips for the mood lighting in my place
    Things to implement:
      * Sunrise mode in the morning
      * Weather forecast mode
      * Random chill mode in the evening (with a favorite color for every day?)
    Usage:
      
        python3 indLightDaemon.py 192.168.1.200 2711
"""
import sys, socket, struct, time
from numpy import *
from colorsys import hsv_to_rgb

if len( sys.argv ) != 3:
    print( __doc__ )
    sys.exit()

IP   = sys.argv[1]
PORT = int( sys.argv[2] )
SOCK = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )

print( "Sending to {0} {1}".format(IP,PORT) )

def updateLed( rgbArray ):
    global SOCK, IP, PORT
    rawDat = rgbArray.flatten()[:300]
    rawDat = rawDat.clip(0,1) * 255
    rawDat = array( rawDat, dtype=uint8, copy=False )
    l = len( rawDat ) // 3
    offst = 0
    dat = struct.pack( ">bHH", 0, offst, l ) + bytes( rawDat )
    SOCK.sendto( dat, (IP, PORT) )

class RainbowDisplayer(object):
    def __init__( self ):
        self.width = 0.5 + random.random()*5
        self.speed = 5 * random.random() / 1000
        self.brightness = 0.01
    
    def getFrame( self, ledState, tick, brightness=None ):
        if brightness is None:
            brightness = self.brightness
        nLeds = ledState.shape[0]
        for i,color in enumerate(ledState):
            color[:] = hsv_to_rgb( self.width*i/nLeds + tick*self.speed, 1, brightness)

nLeds = 100
tick = 0
ledState = zeros( (nLeds,3) )
displayMode = "rainbow"

rd = RainbowDisplayer()
while( True ):
    if displayMode == "rainbow":
        rd.getFrame( ledState, tick )
    updateLed( ledState )
    tick += 1
    time.sleep( 0.1 )