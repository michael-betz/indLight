#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "pw.h"

#define PIN_DATA 3
#define N_LEDS 7
#define UDP_PORT 2711

Adafruit_NeoPixel strip(N_LEDS, PIN_DATA, NEO_GRB + NEO_KHZ800);

WiFiUDP Udp;
uint8_t udp_buff[4096];

void setup() {
	Serial.begin(115200);
	strip.begin();

	WiFi.begin(WIFI_NAME, WIFI_PW);
	strip.setPixelColor(0, 0xFF, 0, 0);
	strip.show();

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	strip.setPixelColor(0, 0, 0xFF, 0);
	strip.show();

	Serial.print("Connected! IP:");
	Serial.println(WiFi.localIP());

	Udp.begin(UDP_PORT);
}

void loop() {
	int packetSize = Udp.parsePacket();
	if (packetSize <= 0)
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
