#!/bin/sh
screen -r indLight -X quit
pkill indLightDaemon
screen -dmS indLight -L indLight.log ./indLightDaemon 192.168.42.200 2711
