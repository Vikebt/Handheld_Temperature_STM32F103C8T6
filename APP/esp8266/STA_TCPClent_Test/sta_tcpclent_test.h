#ifndef _sta_tcpclent_test_H
#define _sta_tcpclent_test_H

#include "system.h"

extern volatile uint8_t TcpClosedFlag;

u8 ESP8266_STA_TCPClient_Connect(char *ssid,
                                 char *password,
                                 char *server_ip,
                                 uint16_t server_port);
void Data_Packaged(void);

#endif
