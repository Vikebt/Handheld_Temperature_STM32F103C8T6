#ifndef _sta_tcpclent_test_H
#define _sta_tcpclent_test_H

#include "system.h"

#define User_ESP8266_SSID	  "VAX"	      //Ҫ���ӵ��ȵ������
#define User_ESP8266_PWD	  "14725836"	  //Ҫ���ӵ��ȵ������

#define User_ESP8266_TCPServer_IP	  "192.168.43.176"	  //Ҫ���ӵķ�������IP
#define User_ESP8266_TCPServer_PORT	  "5000"	  //Ҫ���ӵķ������Ķ˿�

extern volatile uint8_t TcpClosedFlag;

u8 ESP8266_STA_TCPClient_Test(void);
void Data_Packaged(void);

#endif
