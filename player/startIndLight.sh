#!/bin/sh
screen -r indLight -X quit
pkill indLightDaemon
screen -dmS indLight ./indLightDaemon 192.168.42.200 2711
