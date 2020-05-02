// --------------------------------------
//  UDP to Neopixel firmware for ESP8266
// --------------------------------------
// Build / Upload / Monitor it with
// $ platformio run -t upload -t monitor
//
//
// Implementation of MXP (Matrix Protocol), a very simplistic
// protocol for LED Matrix / LED Strip data communication.
// Compatible with: https://github.com/Jeija/WS2811LEDMatrix
//
// UDP Port 2711
//
// ############################
// LED Matrix UDP Data Protocol
// ############################
// [ 1 byte ] [ 0 -  0] Packet type
// 	    - 0x00 = [MXP_FRM] Frame data from PC
//
// In case of frame data:
// [ 2 bytes] [ 1 -     2] Offset (nth LED in string)
// [ 2 bytes] [ 3 -     4] Length of data in LEDs (number n of bytes / 3); Offset + Length must be <= Number of LEDs
// [ n bytes] [ 5 - n + 5] Frame data, which consists of:
// ----------- Frame data -----------
// [ 1 byte] Red from 0-255
// [ 1 byte] Green from 0-255
// [ 1 byte] Blue from 0-255
// This 3-byte Order is repeated for each LED pixel (so Length / 3 times)
// --> There are Length * 3 bytes of color data in total

#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define PIN_DATA 3
#define N_LEDS 100
#define UDP_PORT 2711

// WS2812 black LED strips
// Adafruit_NeoPixel strip(N_LEDS, PIN_DATA, NEO_GRB + NEO_KHZ800);

// WS2811 installed on the wall
Adafruit_NeoPixel strip(N_LEDS, PIN_DATA, NEO_BRG + NEO_KHZ800);

WiFiUDP Udp;
uint8_t udp_buff[4096];

void setup() {
	Serial.begin(115200);

	strip.begin();
	strip.setPixelColor(0, 0xFF, 0, 0);
	strip.show();

	WiFi.begin(WIFI_NAME, WIFI_PW);

	Serial.printf("\nThis is indLight, connecting to %s ", WIFI_NAME);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	strip.setPixelColor(0, 0, 0xFF, 0);
	strip.show();

	Serial.print("\nConnected! IP: ");
	Serial.println(WiFi.localIP());

	Udp.begin(UDP_PORT);
}

void loop() {
	if (!Udp.parsePacket())
		return;

	int len = Udp.read(udp_buff, 4095);
	if (len < 8)
		return;

	if (udp_buff[0] != 0)
		return;

	int led_offset = (udp_buff[1] << 8) | udp_buff[2];
	int led_len = (udp_buff[3] << 8) | udp_buff[4];
	uint8_t *p = &udp_buff[5];

	if (led_len > strip.numPixels())
		led_len = strip.numPixels();

	for (int i=0; i<led_len; i++) {
		strip.setPixelColor(led_offset, p[0], p[1], p[2]);
		led_offset++;
		p += 3;
	}
	strip.show();
}
