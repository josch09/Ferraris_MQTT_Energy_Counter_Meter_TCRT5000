#ifndef MQTT_SUBSCRIBE
#define MQTT_SUBSCRIBE

#include <Arduino.h>


extern bool saveConfig;


// MQTT subscribe callback
void parseMQTTmessage(char* topic, byte* payload, unsigned int length);

#endif