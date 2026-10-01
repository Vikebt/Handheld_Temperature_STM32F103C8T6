#include "sta_tcpclent_test.h"
#include "systick.h"
#include "usart.h"
#include "esp8266_drive.h"
#include "oled_iic.h"
#include <stdio.h>

#define ESP8266_RETRY_MAX 20U

volatile u8 TcpClosedFlag = 0U;

static u8 retry_join_ap(char *ssid, char *password)
{
	u8 retry;

	for (retry = 0U; retry < ESP8266_RETRY_MAX; retry++)
	{
		if (ESP8266_JoinAP(ssid, password) == true)
		{
			return 1U;
		}
		delay_ms(200);
	}

	return 0U;
}

static u8 retry_link_server(char *server_ip, uint16_t server_port)
{
	u8 retry;
	char port[6];

	snprintf(port, sizeof(port), "%u", (unsigned int)server_port);

	for (retry = 0U; retry < ESP8266_RETRY_MAX; retry++)
	{
		if (ESP8266_Link_Server(enumTCP, server_ip, port, Single_ID_0) == true)
		{
			return 1U;
		}
		delay_ms(200);
	}

	return 0U;
}

static u8 retry_enable_unvarnish_send(void)
{
	u8 retry;

	for (retry = 0U; retry < ESP8266_RETRY_MAX; retry++)
	{
		if (ESP8266_UnvarnishSend() == true)
		{
			return 1U;
		}
		delay_ms(100);
	}

	return 0U;
}


u8 ESP8266_STA_TCPClient_Connect(char *ssid,
                                 char *password,
                                 char *server_ip,
                                 uint16_t server_port)
{
	if ((ssid == NULL) || (password == NULL) || (server_ip == NULL) ||
	    (ssid[0] == '\0') || (server_ip[0] == '\0') || (server_port == 0U))
	{
		printf("ESP8266 configuration is not provisioned.\r\n");
		return 0U;
	}

	printf("\r\nESP8266 setup start...\r\n");

	ESP8266_CH_PD_Pin_SetH;

	ESP8266_AT_Test();

	if (ESP8266_Net_Mode_Choose(STA) == false)
	{
		printf("Set STA mode failed.\r\n");
		return 0U;
	}

	if (retry_join_ap(ssid, password) == 0U)
	{
		printf("Join AP failed.\r\n");
		return 0U;
	}

	if (ESP8266_Enable_MultipleId(DISABLE) == false)
	{
		printf("Set single-link mode failed.\r\n");
		return 0U;
	}

	if (retry_link_server(server_ip, server_port) == 0U)
	{
		printf("Link server failed.\r\n");
		return 0U;
	}

	if (retry_enable_unvarnish_send() == 0U)
	{
		printf("Enable transparent send failed.\r\n");
		return 0U;
	}

	printf("ESP8266 setup done.\r\n");
	return 1U;
}

//void TCP_Service()
//{
//		u8 res;
//	short read_temp=0;
//	float rtemp;
//	
//	char str[100]={0};
//	
//	printf ( "\r\n��������ESP8266�����ĵȴ�...\r\n" );

//	ESP8266_AT_Test();
//	ESP8266_Net_Mode_Choose(AP);
//	
//	
//	
//}

void Data_Packaged(void)
{
	short read_temp;
	short read_temp2;
	float rtemp;
	float rtemp2;
 	char str[64];

	read_temp = (short)tem_trans();
	read_temp2 = (short)tem2_trans();

	rtemp = (float)read_temp / 100.0f;
	rtemp2 = (float)read_temp2 / 100.0f;

	ESP8266_SendString(ENABLE, "HELLO\r\n", 0, Single_ID_0);

	sprintf(str, "TEMP1=%.1f\r\n", rtemp);
	ESP8266_SendString(ENABLE, str, 0, Single_ID_0);

	sprintf(str, "TEMP2=%.1f\r\n", rtemp2);
	ESP8266_SendString(ENABLE, str, 0, Single_ID_0);

	ESP8266_SendString(ENABLE, "ID=00179836\r\n", 0, Single_ID_0);
}



