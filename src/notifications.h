#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Arduino.h>

// Send Pushover alert
void sendPushoverAlert(const char* title, const char* message, int priority = 0);

// URL-encode a string for application/x-www-form-urlencoded
String urlEncode(const char* str);

#endif
