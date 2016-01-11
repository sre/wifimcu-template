#include "System.h"
#include "LED.h"
#include "Button.h"
#include "Printf.h"
#include "RCC.h"

#include <lwip/opt.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/sockets.h>
#include <lwip/mem.h>
#include <netif/etharp.h>
//#include <ipv4/lwip/ip.h>
#include <lwip/tcpip.h>
#include <lwip/dhcp.h>
#include <wwd_network.h>
#include <wwd_management.h>
#include <wwd_wifi.h>
#include <network/wwd_buffer_interface.h>




#define MAKE_IPV4_ADDRESS(a,b,c,d) ((((uint32_t)(a))<<24)|(((uint32_t)(b))<<16)|(((uint32_t)(c))<<8)|((uint32_t)(d)))

#define AP_SSID "YUKI.N"
#define AP_PASS ""
#define AP_SEC WICED_SECURITY_OPEN

// Constants for WEP usage. NOTE: The WEP key index is a value from 0 - 3 and not 1 - 4
#define WEP_KEY WEP_KEY_104 // Demonstrates usage of WEP if AP_SEC = WICED_SECURITY_WEP_PSK
#define WEP_KEY_40 {{0,5,{0x01,0x23,0x45,0x67,0x89}} // WEP-40  has a 10 hex byte key
#define WEP_KEY_104 {{0,13,{0x01,0x23,0x45,0x67,0x89,0x01,0x23,0x45,0x67,0x89,0x01,0x23,0x45}}}	// WEP-104 has a 26 hex byte key

#define COUNTRY WICED_COUNTRY_FINLAND
#define USE_DHCP 1
#define IP_ADDR MAKE_IPV4_ADDRESS(192,168,1,95)
#define GW_ADDR MAKE_IPV4_ADDRESS(192,168,1,1)
#define NETMASK MAKE_IPV4_ADDRESS(255,255,255,0)
//snt#define PING_TARGET MAKE_IPV4_ADDRESS(45,55,226,51) // Uncomment if you want to ping a specific IP instead of the gateway

#define PING_RCV_TIMEOUT 1000 // ping receive timeout - in milliseconds
#define PING_DELAY 1000 // Delay between ping response/timeout and the next ping send - in milliseconds
#define PING_ID 0xafaf
#define PING_DATA_SIZE 32 // ping additional data size to include in the packet
#define APP_THREAD_STACKSIZE 5120




static void StartupTask(void *parameters);
static void TCPIPInitCallback(void *argument);
static void PingFunction();
static err_t ReceivePing(int socket);
static err_t SendPing(int socket,struct ip_addr *addr);




static uint16_t SequenceNumber;
static struct netif wiced_if;

int main()
{
	InitialiseSystem();

	InitialisePrintf();
	printf("EMW3165 Template Project\n");
	printf("========================\n");
	printf("\n");

	InitialiseLEDs();
	InitialiseUserButton();

	for(int i=0;i<96;i++) NVIC_SetPriority(i,0xf);
	NVIC_SetPriorityGrouping(3);

	NVIC_SetPriority(MemoryManagement_IRQn,0);
	NVIC_SetPriority(BusFault_IRQn,0);
	NVIC_SetPriority(UsageFault_IRQn,0);
	NVIC_SetPriority(SVCall_IRQn,0);
	NVIC_SetPriority(DebugMonitor_IRQn,0);
	NVIC_SetPriority(PendSV_IRQn,configKERNEL_INTERRUPT_PRIORITY>>4);
	NVIC_SetPriority(SysTick_IRQn,configKERNEL_INTERRUPT_PRIORITY>>4);

	NVIC_SetPriority(RTC_WKUP_IRQn,1);
	NVIC_SetPriority(SDIO_IRQn,2);
	NVIC_SetPriority(DMA2_Stream3_IRQn,3); // WLAN SDIO DMA

	printf("Creating startup task.\n");
	xTaskCreate(StartupTask,"StartupTask",APP_THREAD_STACKSIZE,NULL,DEFAULT_THREAD_PRIO,NULL);

	printf("Starting FreeRTOS scheduler... ");
	vTaskStartScheduler();

	printf("vTaskStartScheduler() returned unexpectedly\n");

	return 0;
}


static void StartupTask(void *parameters)
{
	printf("Done.\n");
	printf("In startup task.\n");

	printf("Creating semaphore for LwIP startup... ");
	SemaphoreHandle_t semaphore=xSemaphoreCreateCounting(1,0);
	if(!semaphore)
	{
		printf("Failed!\n");
		vTaskDelete(NULL);
	}
	printf("Done.\n");

	printf("Starting LwIP stack... ");
	tcpip_init(TCPIPInitCallback,(void *)&semaphore);
	xSemaphoreTake(semaphore,portMAX_DELAY);
	vQueueDelete(semaphore);
	printf("Done.\n");

	printf("Starting WICED WiFi driver... ");
	wwd_buffer_init(NULL);
	wwd_result_t result=wwd_management_wifi_on(COUNTRY);
	if(result!=WWD_SUCCESS)
	{
		printf("Error %d while starting WICED!\n",result);
		vTaskDelete(NULL);
	}
	printf("Done.\n");

	printf("Joining network \"" AP_SSID "\"... ");

	static const wiced_ssid_t ap_ssid=
	{
		.length=sizeof(AP_SSID)-1,
		.value=AP_SSID,
	};

	if(AP_SEC==WICED_SECURITY_WEP_PSK)
	{
		wiced_wep_key_t key[]=WEP_KEY;
		while(wwd_wifi_join(&ap_ssid,AP_SEC,(uint8_t *)key,sizeof(key),NULL)!=WWD_SUCCESS)
		{
			printf("Failed, retrying...\n");
		}
	}
	else
	{
		while(wwd_wifi_join(&ap_ssid,AP_SEC,(uint8_t *)AP_PASS,sizeof(AP_PASS)-1,NULL)!=WWD_SUCCESS)
		{
			printf("Failed, retrying...\n");
		}
	}
	printf("Done.\n");

	printf("Adding WICED WiFi driver network interface... ");

	struct ip_addr ipaddr,netmask,gw;

	if(USE_DHCP)
	{
		ip_addr_set_zero(&gw);
		ip_addr_set_zero(&ipaddr);
		ip_addr_set_zero(&netmask);
	}
	else
	{
		ipaddr.addr=htonl(IP_ADDR);
		gw.addr=htonl(GW_ADDR);
		netmask.addr=htonl(NETMASK);
	}

	if(!netif_add(&wiced_if,&ipaddr,&netmask,&gw,(void *)WWD_STA_INTERFACE,ethernetif_init,ethernet_input))
	{
		printf("Failed!\n");
		vTaskDelete(NULL);
	}
	printf("Done.\n");

	if(USE_DHCP)
	{
		printf("Obtaining IP address via DHCP... ");
		struct dhcp netif_dhcp;
		dhcp_set_struct(&wiced_if,&netif_dhcp);
		dhcp_start(&wiced_if);
		while(netif_dhcp.state!=DHCP_BOUND)
		{
			vTaskDelay(10);
		}
		printf("Done.\n");
	}
	else
	{
		netif_set_up(&wiced_if);
	}

	printf("IP address: %u.%u.%u.%u\n",
	(htonl(wiced_if.ip_addr.addr)>>24)&0xff,
	(htonl(wiced_if.ip_addr.addr)>>16)&0xff,
	(htonl(wiced_if.ip_addr.addr)>>8)&0xff,
	(htonl(wiced_if.ip_addr.addr))&0xff);

	#ifdef PING_TARGET
	PingFunction((struct ip_addr){ .addr=htonl(PING_TARGET) });
	#else
	PingFunction(wiced_if.gw);
	#endif

	if(USE_DHCP)
	{
		printf("Stopping DHCP... ");
		dhcp_stop(&wiced_if);
		printf("Done.\n");
	}

	printf("Taking network interface down... ");
	netif_set_down(&wiced_if);
	printf("Done.\n");

	printf("Removing network interface... ");
	netif_remove(&wiced_if);
	printf("Done.\n");

	printf("Shutting down WICED WiFi driver... ");
	if(wwd_wifi_leave(WWD_STA_INTERFACE)!=WWD_SUCCESS ||
	wwd_management_wifi_off()!=WWD_SUCCESS)
	{
		printf("Failed!\n");
		vTaskDelete(NULL);
	}
	printf("Done.\n");

	vTaskDelete(NULL);
}

static void TCPIPInitCallback(void *argument)
{
	SemaphoreHandle_t *semaphore=(xSemaphoreHandle *)argument;
	xSemaphoreGive(*semaphore);
}




static void PingFunction(struct ip_addr target)
{
	wwd_time_t send_time;

	// Open a local socket for pinging.
	int socket=lwip_socket(AF_INET,SOCK_RAW,IP_PROTO_ICMP);
	if(socket<0)
	{
		printf("Unable to create socket for Ping\n");
		return;
	}

	// Set the receive timeout on local socket so pings will time out.
	int timeout=PING_RCV_TIMEOUT;
	lwip_setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));

	printf("Pinging: %u.%u.%u.%u\n",
	(htonl(target.addr)>>24)&0xff,
	(htonl(target.addr)>>16)&0xff,
	(htonl(target.addr)>>8)&0xff,
	(htonl(target.addr))&0xff);

	for(;;)
	{
		err_t result;

		SetLEDs(1);
		if(SendPing(socket,&target)!=ERR_OK)
		{
			printf("Unable to send ping!\n");
			break;
		}

		uint32_t sendtime=xTaskGetTickCount()*1000/configTICK_RATE_HZ;

		if(ReceivePing(socket)==ERR_OK)
		{
			uint32_t receivetime=xTaskGetTickCount()*1000/configTICK_RATE_HZ;
			printf("Ping time: %d ms\n",receivetime-sendtime);
		}
		else
		{
			printf("Ping timeout\n");
		}
		SetLEDs(0);

		sys_msleep(PING_DELAY);
	}

	lwip_close(socket);
}

static err_t SendPing(int socket,struct ip_addr *addr)
{
	size_t pingsize=sizeof(struct icmp_echo_hdr)+PING_DATA_SIZE;

	struct icmp_echo_hdr *iecho=(struct icmp_echo_hdr *)mem_malloc(pingsize);
	if(!iecho) return ERR_MEM;

	SequenceNumber++;

	ICMPH_TYPE_SET(iecho,ICMP_ECHO);
	ICMPH_CODE_SET(iecho,0);
	iecho->chksum=0;
	iecho->id=PING_ID;
	iecho->seqno=htons(SequenceNumber);

	uint8_t *data=(uint8_t *)&iecho[1];
	for(int i=0;i<PING_DATA_SIZE;i++) data[i]=i;

	iecho->chksum=inet_chksum(iecho,pingsize);

	struct sockaddr_in to;
	to.sin_len=sizeof(to);
	to.sin_family=AF_INET;
	to.sin_addr.s_addr=addr->addr;

	int err=lwip_sendto(socket,iecho,pingsize,0,(struct sockaddr *)&to,sizeof(to));

	mem_free(iecho);

	return err?ERR_OK:ERR_VAL;
}


static err_t ReceivePing(int socket)
{
	for(;;)
	{
		char buf[64];
		struct sockaddr_in from;
		socklen_t fromlen;
		int len=lwip_recvfrom(socket,buf,sizeof(buf),0,(struct sockaddr *)&from,&fromlen);
		if(len<=0) break;

		if(len>=sizeof(struct ip_hdr)+sizeof(struct icmp_echo_hdr))
		{
			struct ip_hdr *iphdr=(struct ip_hdr *)buf;
			struct icmp_echo_hdr *iecho=(struct icmp_echo_hdr *)&buf[IPH_HL(iphdr)*4];

			if(iecho->id==PING_ID)
			if(iecho->seqno==htons(SequenceNumber))
			if(ICMPH_TYPE(iecho)==ICMP_ER)
			{
				return ERR_OK;
			}
		}
	}

	return ERR_TIMEOUT;
}
