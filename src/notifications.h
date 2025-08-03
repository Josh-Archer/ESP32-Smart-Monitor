#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Arduino.h>

// Send Pushover alert
void sendPushoverAlert(const char* title, const char* message, int priority = 0);

#endif
