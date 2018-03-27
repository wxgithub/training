/*---------------------------------------------------------------------------*/
/* Include Files                                                             */
/*---------------------------------------------------------------------------*/
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <apps/netutils/netlib.h>
#include <apps/netutils/wifi/slsi_wifi_api.h>
#include <apps/netutils/wifi/slsi_wifi_utils.h>
#include <iotbus/iotbus_gpio.h>
#include <sys/mount.h>
#include <tinyara/arch.h>
#include <tinyara/config.h>
#include <tinyara/gpio.h>
#include <artik_network.h>

/*---------------------------------------------------------------------------*/
/* Defines                                                                   */
/*---------------------------------------------------------------------------*/
#define STATE_DISCONNECTED          0
#define STATE_CONNECTED             1

#define SLSI_WIFI_SECURITY_OPEN     "open"
#define SLSI_WIFI_SECURITY_WPA2_AES "wpa2_aes"

#ifndef APP_PRIORITY
#define APP_PRIORITY 100
#endif

#ifndef APP_STACKSIZE
#define APP_STACKSIZE 2048
#endif


/*---------------------------------------------------------------------------*/
/* User Required Defines                                                     */
/*---------------------------------------------------------------------------*/
#define SSID "YOUR_SSID_GOES_HERE"
#define PSK  "YOUR_WIFI_PASSWORD_GOES_HERE"

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/
slsi_security_config_t *getSecurityConfig(char *sec_type, char *psk, WiFi_InterFace_ID_t mode);

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/
static artik_network_dhcp_client_handle g_dhcp_handle;
static int g_connection_state = STATE_DISCONNECTED;
static uint8_t g_join_result  = 0;
static sem_t g_sem_join;

/**
 * Handler for network link up connection event
 *
 *   Sets the global connection state variable and the result of the network
 *   join request.
 */
void networkLinkUpHandler(slsi_reason_t* reason) {
    g_connection_state = STATE_CONNECTED;

    g_join_result = reason->reason_code;
    sem_post(&g_sem_join);
}

/**
 * Handler for network link down connection event
 *
 *   Sets the global connection variable when the access point is disconnected
 *   from the network.
 */
void networkLinkDownHandler(slsi_reason_t* reason) {
    g_connection_state = STATE_DISCONNECTED;

    if (reason) {
        printf("Disconnected from network %s reason_code: %d %s\n", reason->bssid, reason->reason_code,
                    reason->locally_generated ? "(locally_generated)": "");
    } else {
        printf("Disconnected from network\n");
    }
}

/**
 * Starts the Wi-Fi interface and request connection to the specified network
 * Return: Completed successfully or failed
 *
 *   Starts the Wi-Fi interface in Station mode and requests to join the
 *   specified network.
 */
int8_t start_wifi_interface(void) {

    if ( WiFiRegisterLinkCallback(&networkLinkUpHandler, &networkLinkDownHandler) ) {
        return SLSI_STATUS_ERROR;
    }

    if ( WiFiStart(SLSI_WIFI_STATION_IF, NULL) == SLSI_STATUS_ERROR ) {
        return SLSI_STATUS_ERROR;
    }

    sem_init(&g_sem_join, 0, 0);

    slsi_security_config_t *security_config = getSecurityConfig(SLSI_WIFI_SECURITY_WPA2_AES, PSK, SLSI_WIFI_STATION_IF);

    if ( WiFiNetworkJoin((uint8_t*)SSID, strlen(SSID), NULL, security_config) == SLSI_STATUS_ERROR ) {
        return SLSI_STATUS_ERROR;
    }

    sem_wait(&g_sem_join);

    if( g_join_result ) {
       return SLSI_STATUS_ERROR;
    }

    free(security_config);
    sem_destroy(&g_sem_join);

    return SLSI_STATUS_SUCCESS;
}

static int start_dhcp_client(void)
{
	int ret = 0;
	artik_network_module *network = (artik_network_module *)
			artik_request_api_module("network");
	artik_error err = S_OK;

	if (!network) {
		fprintf(stderr, "Failed to request network module\n");
		return -1;
	}

	err = network->dhcp_client_start(&g_dhcp_handle, ARTIK_WIFI);
	if (err != S_OK) {
		fprintf(stderr, "Failed to request DHCP lease (err=%d)\n", ret);
		ret = -1;
		goto exit;
	}

exit:
	artik_release_api_module(network);
	return ret;
}

/**
 * Write the value of gpio
 *
 *   Write the value of given gpio port.
 *
 */
void gpio_write(int port, int value)
{
    char str[4];
    static char devpath[16];
    snprintf(devpath, 16, "/dev/gpio%d", port);
    int fd = open(devpath, O_RDWR);

    ioctl(fd, GPIOIOC_SET_DIRECTION, GPIO_DIRECTION_OUT);
    write(fd, str, snprintf(str, 4, "%d", value != 0) + 1);

    close(fd);
}
/**
 * Blinks the LED for the given file descriptor handle
 *
 *   For the given file handle, the value is enabled and disabled at one second
 *   period.
 */
void blink_led(int i) {
    gpio_write(i, 1);
    sleep(1);
    gpio_write(i, 0);
    sleep(1);
    return;
}

/**
 * Enable the LED for the Wi-Fi connectivity state
 *
 *   Turns on LED703 (GPIO16) on the ARTIK 053 Starter Kit.
 */
void enable_wifi_starter_kit_led(void) {
    gpio_write(45, 1);
    return;
}


#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int artik_wifi_main(int argc, char *argv[])
#endif
{
#ifdef CONFIG_CTRL_IFACE_FIFO
    int ret;

    ret = mkfifo(CONFIG_WPA_CTRL_FIFO_DEV_REQ, CONFIG_WPA_CTRL_FIFO_MK_MODE);
    if(ret != 0 && ret != -EEXIST) {
        printf("mkfifo error for %s: %s", CONFIG_WPA_CTRL_FIFO_DEV_REQ, strerror(errno));
    }
    ret = mkfifo(CONFIG_WPA_CTRL_FIFO_DEV_CFM, CONFIG_WPA_CTRL_FIFO_MK_MODE);
    if(ret != 0 && ret != -EEXIST) {
        printf("mkfifo error for %s: %s", CONFIG_WPA_CTRL_FIFO_DEV_CFM, strerror(errno));
    }

        ret = mkfifo(CONFIG_WPA_MONITOR_FIFO_DEV, CONFIG_WPA_CTRL_FIFO_MK_MODE);
    if(ret != 0 && ret != -EEXIST){
        printf("mkfifo error for %s: %s", CONFIG_WPA_MONITOR_FIFO_DEV, strerror(errno));
    }
#endif

#ifdef CONFIG_EXAMPLES_MOUNT
    mount_app_main(0, NULL);
#endif

    if ( start_wifi_interface() == SLSI_STATUS_ERROR ) {
    	printf("Connect Wi-Fi failed. Exit.\n");
        return SLSI_STATUS_ERROR;
    }
    start_dhcp_client();

    printf("Connect Wi-Fi success\n");
    // Turn on LED703 (XGPIO20) after successful Wi-Fi connection
    //   on ARTIK 053 Starter Kit
    //enable_wifi_starter_kit_led();
   /*
    while(1) {
        blink_led(49);
    }
    */

}

