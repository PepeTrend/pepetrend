/*
<:copyright-BRCM:2002:GPL/GPL:standard

   Copyright (c) 2002 Broadcom Corporation
   All Rights Reserved

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:>
*/
/***************************************************************************
* File Name  : board.c
*
* Description: This file contains Linux character device driver entry
*              for the board related ioctl calls: flash, get free kernel
*              page and dump kernel memory, etc.
*
*
***************************************************************************/

/* Includes. */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/if.h>
#include <linux/pci.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/smp.h>
#include <linux/version.h>
#include <linux/reboot.h>
#include <linux/bcm_assert_locks.h>
#include <asm/delay.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/fs.h>

//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
#include <linux/inetdevice.h>
#include <linux/kthread.h>
//#include <linux/netdevice.h>
//>> [CTFN-SYS-016] End
//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
#include <net/addrconf.h>
#include <net/ipv6.h>
//>> [CTFN-SYS-016-2] End

#include <bcmnetlink.h>
#include <net/sock.h>
#include <bcm_map_part.h>
#include <board.h>
#include <spidevices.h>
#define  BCMTAG_EXE_USE
#include <bcmTag.h>
#include <boardparms.h>
#include <boardparms_voice.h>
#include <flash_api.h>
#include <bcm_intr.h>
#include <flash_common.h>
#include <bcmpci.h>
#include <linux/bcm_log.h>
#include <bcmSpiRes.h>
//extern unsigned int flash_get_reserved_bytes_at_end(const FLASH_ADDR_INFO *fInfo);

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
unsigned int have_usb_data_pass = 0;
//>> [JAZ-026] End

//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
#ifdef CTCONFIG_3G_FEATURE
extern char* get_usb_id(void);
#endif
//>> [CTFN-3G-001] End

/* Typedefs. */

#if defined (WIRELESS)
#define SES_EVENT_BTN_PRESSED      0x00000001
#define SES_EVENTS                 SES_EVENT_BTN_PRESSED /*OR all values if any*/
#define SES_LED_OFF                0
#define SES_LED_ON                 1
#define SES_LED_BLINK              2

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
#define WLAN_ONBOARD_SLOT	WLAN_ONCHIP_DEV_SLOT
#else
#define WLAN_ONBOARD_SLOT       1 /* Corresponds to IDSEL -- EBI_A11/PCI_AD12 */
#endif

#define BRCM_VENDOR_ID       0x14e4
#define BRCM_WLAN_DEVICE_IDS 0x4300
#define BRCM_WLAN_DEVICE_IDS_DEC 43

#define WLAN_ON   1
#define WLAN_OFF  0
#endif

typedef struct
{
    unsigned long ulId;
    char chInUse;
    char chReserved[3];
} MAC_ADDR_INFO, *PMAC_ADDR_INFO;

typedef struct
{
    unsigned long ulNumMacAddrs;
    unsigned char ucaBaseMacAddr[NVRAM_MAC_ADDRESS_LEN];
    MAC_ADDR_INFO MacAddrs[1];
} MAC_INFO, *PMAC_INFO;

typedef struct
{
    unsigned char gponSerialNumber[NVRAM_GPON_SERIAL_NUMBER_LEN];
    unsigned char gponPassword[NVRAM_GPON_PASSWORD_LEN];
} GPON_INFO, *PGPON_INFO;

typedef struct
{
    unsigned long eventmask;
} BOARD_IOC, *PBOARD_IOC;


/*Dyinggasp callback*/
typedef void (*cb_dgasp_t)(void *arg);
typedef struct _CB_DGASP__LIST
{
    struct list_head list;
    char name[IFNAMSIZ];
    cb_dgasp_t cb_dgasp_fn;
    void *context;
}CB_DGASP_LIST , *PCB_DGASP_LIST;


/* Externs. */
extern struct file *fget_light(unsigned int fd, int *fput_needed);
//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
extern const char *get_system_type(void);
//>> [CTFN-SYS-016] End
extern unsigned long getMemorySize(void);
extern void __init boardLedInit(void);
extern void boardLedCtrl(BOARD_LED_NAME, BOARD_LED_STATE);

/* Prototypes. */
static void set_mac_info( void );
static void set_gpon_info( void );
static int board_open( struct inode *inode, struct file *filp );
static int board_ioctl( struct inode *inode, struct file *flip, unsigned int command, unsigned long arg );
static ssize_t board_read(struct file *filp,  char __user *buffer, size_t count, loff_t *ppos);
static unsigned int board_poll(struct file *filp, struct poll_table_struct *wait);
static int board_release(struct inode *inode, struct file *filp);

static BOARD_IOC* borad_ioc_alloc(void);
static void borad_ioc_free(BOARD_IOC* board_ioc);

/*
 * flashImageMutex must be acquired for all write operations to
 * nvram, CFE, or fs+kernel image.  (cfe and nvram may share a sector).
 */
DEFINE_MUTEX(flashImageMutex);
static void writeNvramDataCrcLocked(PNVRAM_DATA pNvramData);
static PNVRAM_DATA readNvramData(void);

/* DyingGasp function prototype */
//<< [CTFN-SYS-023] Jimmy Wu : Add macro definition to disable dying gasp
#ifndef CTCONFIG_SYS_DISABLE_DYING_GASP
static irqreturn_t kerSysDyingGaspIsr(int irq, void * dev_id);
static void __init kerSysInitDyingGaspHandler( void );
#endif /* CTCONFIG_SYS_DISABLE_DYING_GASP */
//>> [CTFN-SYS-023] End
static void __exit kerSysDeinitDyingGaspHandler( void );
/* -DyingGasp function prototype - */
/* dgaspMutex protects list add and delete, but is ignored during isr. */
static DEFINE_MUTEX(dgaspMutex);

static int ConfigCs(BOARD_IOCTL_PARMS *parms);
//<< [CTFN-SYS-016] Dylan, jojopo : Support Internet LED for 6328 product
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96318)
static void SetInternetLed(int gpio, GPIO_STATE_t state);
#endif
//>> [CTFN-SYS-016] End


#if defined (WIRELESS)
static irqreturn_t sesBtn_isr(int irq, void *dev_id);
static void __init sesBtn_mapIntr(int context);
static Bool sesBtn_pressed(void);
static unsigned int sesBtn_poll(struct file *file, struct poll_table_struct *wait);
static ssize_t sesBtn_read(struct file *file,  char __user *buffer, size_t count, loff_t *ppos);
static void __init sesLed_mapGpio(void);
static void sesLed_ctrl(int action);
static void __init ses_board_init(void);
static void __exit ses_board_deinit(void);
static void __init kerSysScreenPciDevices(void);
static void kerSetWirelessPD(int state);
#endif

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
static void __init kerSysCheckPowerDownPcie(void);
#endif

static void str_to_num(char* in, char *out, int len);
static int add_proc_files(void);
static int del_proc_files(void);
static int proc_get_param(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_param(struct file *f, const char *buf, unsigned long cnt, void *data);
static int proc_set_led(struct file *f, const char *buf, unsigned long cnt, void *data);
//<< [CTFN-WIFI-012] Antony.Wu : Set default authentication mode and and default Wireless key, 2011/11/25
static int proc_get_string(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_string(struct file *f, const char *buf, unsigned long cnt, void *data);
//>> [CTFN-WIFI-012] End

static irqreturn_t reset_isr(int irq, void *dev_id);

// macAddrMutex is used by kerSysGetMacAddress and kerSysReleaseMacAddress
// to protect access to g_pMacInfo
static DEFINE_MUTEX(macAddrMutex);
static PMAC_INFO g_pMacInfo = NULL;
static PGPON_INFO g_pGponInfo = NULL;
static unsigned long g_ulSdramSize;
#if defined(CONFIG_BCM96368)
static unsigned long g_ulSdramWidth;
#endif
static int g_ledInitialized = 0;
static wait_queue_head_t g_board_wait_queue;
static CB_DGASP_LIST *g_cb_dgasp_list_head = NULL;

#define MAX_PAYLOAD_LEN 64
static struct sock *g_monitor_nl_sk;
static int g_monitor_nl_pid = 0 ;
static void kerSysInitMonitorSocket( void );
static void kerSysCleanupMonitorSocket( void );

#if defined(CONFIG_BCM96368)
static void ChipSoftReset(void);
static void ResetPiRegisters( void );
static void PI_upper_set( volatile uint32 *PI_reg, int newPhaseInt );
static void PI_lower_set( volatile uint32 *PI_reg, int newPhaseInt );
static void TurnOffSyncMode( void );
#endif

#if defined(CONFIG_BCM96816)
void board_Init6829( void );
#endif

/* restore default work structure */
static struct work_struct restoreDefaultWork;

static struct file_operations board_fops =
{
    open:       board_open,
    ioctl:      board_ioctl,
    poll:       board_poll,
    read:       board_read,
    release:    board_release,
};

uint32 board_major = 0;

#if defined (WIRELESS)
static unsigned short sesBtn_irq = BP_NOT_DEFINED;
static unsigned short sesLed_gpio = BP_NOT_DEFINED;
#endif

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
static USB_LED_STATUS usbLedStatus = usbLedInitial;
#define USB_LED_POLL_TIME_MSEC  50
//>> [JAZ-026] End

//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
#define INET_LED_POLL_TIME_MSEC  50

struct trafficInfo
{
    struct trafficInfo *next;
    struct trafficInfo *priv;
    char dev_name[IFNAMSIZ];
    WAN_PHY_TYPE wanType;
    //unsigned long last_rx;	    /* Time of last Rx	*/
    //unsigned long trans_start;	/* Time (in jiffies) of last Tx	*/
    unsigned long  rx_packets ;
    unsigned long  tx_packets ;

	char dev_name_l2[IFNAMSIZ];
	unsigned long  l2Rx_packets ;
    unsigned long  l2Tx_packets ;
};

static int XdslWanType = 0;
static int EthWanType = 0;
static int ThriGWanType = 0;  //for 3G Internet LED
struct trafficInfo *head = NULL;
DECLARE_MUTEX(tarfficInfo_lock);
struct task_struct *blink_thread = NULL;

unsigned char (*get_dsl_state)(void) = NULL;
unsigned int  (*get_eth_state)(void) = NULL;

//<< MHTsai : Support Internet LED for ip extension, 2010/10/14
static char intLedStatus[1] = {0};
//>> MHTsai : End
//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
static WAN_CONN_STATUS wanConnStatus = WAN_UNCONFIGURED;
static WAN_CONN_STATUS wanIPv6ConnStatus = WAN_UNCONFIGURED;
//>> [CTFN-SYS-016-2] End

static int has_ip_addr(struct net_device  *dev)
{
    struct in_ifaddr *ifa = NULL;
    struct in_device *in_dev = NULL;
    int ret = 0;

    if (dev && (in_dev = in_dev_get(dev)) && in_dev)
    {
        for (ifa = in_dev->ifa_list; ifa; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_local)
            {
//<< MHTsai : Support Internet LED for dial on demand, 2010/10/14
                //This is for PPPoE Dial on demand ,Dial on daemon will set ip 10.64.64.64 (0x0a404040)
                //if(((ifa->ifa_local & 0xffffffff) == 0x0a404040) && (strncmp(dev->name,"ppp",3)==0))
                //This is for PPPoE Dial on demand, WAN interface ip is different according to PPPoE or PPPoA, default mode or VLAN mode.
                if ( strcmp(intLedStatus, "d") == 0 )
//>> MHTsai : End
                    continue;
                ret = 1;
                break;
            }
        }
        //in_dev_put(in_dev);
    }
    if(in_dev)
        in_dev_put(in_dev);
    return ret;
}

//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
static int has_ipv6_global_addr(struct net_device  *dev)
{
    struct inet6_ifaddr *ifa = NULL;
    struct inet6_dev *idev = NULL;
    int ret = 0;

    if ((idev = __in6_dev_get(dev)) != NULL) {
       read_lock_bh(&idev->lock);
       for (ifa=idev->addr_list; ifa; ifa=ifa->if_next) {
           /* Only consider IPV6_ADDR_LOOPBACK and IPV6_ADDR_LINKLOCAL conditions currently. Fix it if needed */
           if ((ifa->scope != IFA_LINK) && (ifa->scope != IFA_HOST))
          {
             ret = 1;
             break;
          }
       }
       read_unlock_bh(&idev->lock);
    }
    return ret;
}
//>> [CTFN-SYS-016-2] End

#if 0
typedef struct internet_led
{
    const char board_id[32];
    int gpio_for_inet_act_led;
    int gpio_for_inet_fail_led;
} internet_led;

static const internet_led inet_led_tbl[]=
{
  {"96368M-1331N"   , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for CT-5374 Multi-DSL */
  {"96368M-1331N1"  , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for CT-5374 Multi-DSL with IP1001 PHYID 0x11*/
  {"96369R-1231N"   , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for WAP-5813n Wireless AP */
  {"96368M-123"     , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for VR-3020 Multi-DSL */
  {"96368MT-1341N"  , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for VR-3022u Multi-DSL */
  {"96368MT-1341N1" , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for VR-3026e */
  {"96368V-1341N"   , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for VR-3024u VDSL 30a */
  {"96368M-1341N"   , BP_GPIO_5_AH, BP_GPIO_31_AH},     /* for VR-3025un Multi-DSL */
  {"96328AT-1341N1" , BP_GPIO_11_AL, BP_GPIO_2_AL},     /* for AR-5384u/AR-5381u/AR-5220u */
  {"NULL",0,0} //last
};

static int proc_get_inet_led_gpio(short *inetLedGpio)
{
    static short gpio = 0xFFFF;
    static int not_found = -1;

    if (not_found == 0)
    {
        *inetLedGpio = gpio;
        return 1;
    }
    else if (not_found == 1)
        return 0;
    else
    {
        int i = 0;
        char *boardid = (char *) get_system_type();


        *inetLedGpio = 0xFFFF;
        while(inet_led_tbl[i].board_id)
        {
            if(strcmp(boardid, inet_led_tbl[i].board_id) == 0)
            {
                *inetLedGpio = inet_led_tbl[i].gpio_for_inet_act_led;
                not_found = 0;
                break;
            }
            not_found=1;
            i++;
        }

        return (not_found == 1) ? 0 : 1;
    }
}

static int proc_get_inet_fail_led_gpio(short *inetLedGpio)
{
    static short gpio = 0xFFFF;
    static int not_found = -1;

    if (not_found == 0)
    {
        *inetLedGpio = gpio;
        return 1;
    }
    else if (not_found == 1)
        return 0;
    else
    {
        int i = 0;
        char *boardid = (char *) get_system_type();

        *inetLedGpio = 0xFFFF;
        while(inet_led_tbl[i].board_id)
        {
            if(strcmp(boardid, inet_led_tbl[i].board_id) == 0)
            {
                *inetLedGpio = inet_led_tbl[i].gpio_for_inet_fail_led;
                not_found = 0;
                break;
            }
            not_found=1;
            i++;
        }

		return (not_found == 1) ? 0 : 1;
    }
}
#else

static int proc_get_inet_led_gpio(short *inetLedGpio)
{
    static short gpio = 0xFFFF;
    static int not_found = -1;

    if (not_found == 0)
    {
        *inetLedGpio = gpio;
        return 1;
    }
    else if (not_found == 1)
{
        return 0;
}
    else
    {
        if((BpGetCtInternetLedGpio(&gpio)) == BP_SUCCESS)
        {
            if (gpio != BP_NOT_DEFINED)
            {
                *inetLedGpio = gpio;
                not_found = 0;
            }
            else
            {
            *inetLedGpio = 0xFFFF;
            not_found=1;
            }
        }
        else
        {
            *inetLedGpio = 0xFFFF;
            not_found=1;
        }
	return (not_found == 1) ? 0 : 1;
    }
}

static int proc_get_inet_fail_led_gpio(short *inetLedGpio)
{
    static short gpio = 0xFFFF;
    static int not_found = -1;

    if (not_found == 0)
    {
        *inetLedGpio = gpio;
        return 1;
    }
    else if (not_found == 1)
{
        return 0;
}
    else
    {
        if((BpGetCtInternetFailLedGpio(&gpio)) == BP_SUCCESS)
        {
            if (gpio != BP_NOT_DEFINED)
            {
                *inetLedGpio = gpio;
                not_found = 0;
            }
            else
            {
            *inetLedGpio = 0xFFFF;
            not_found=1;
            }
        }
        else
        {
            *inetLedGpio = 0xFFFF;
            not_found=1;
        }
	return (not_found == 1) ? 0 : 1;
    }
}
#endif

static void BlinkGpio(int gpio)
{
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96318)
    LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(gpio));
//<< [CTFN-SYS-016-1] Camille : Support Internet LED for 963268 product, 2012/01/17
#elif defined(CONFIG_BCM963268) 
    boardLedCtrl(kLedInetOn, kLedStateOff);
    schedule_timeout_interruptible((HZ * INET_LED_POLL_TIME_MSEC) / 1000);
    boardLedCtrl(kLedInetOn, kLedStateOn);
//>> [CTFN-SYS-016-1] End
#else
    if (gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        GPIO->SerialLed ^= GPIO_NUM_TO_MASK(gpio);
    }
    else {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(gpio);
        GPIO->GPIOio ^= GPIO_NUM_TO_MASK(gpio);
    }
#endif
}

static int blink_fun(void *data)
{
    struct trafficInfo *ptr  = NULL;
    struct trafficInfo *next = NULL;
    struct net_device  *dev  = NULL;
    struct net_device_stats *stats  = NULL;
//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
    struct net_device  *brdev = NULL;
//>> [CTFN-SYS-016-2] End

    /* check layer 2 traffic */
    // struct trafficInfo *l2Ptr  = NULL;
    struct net_device  *l2Dev  = NULL;
    struct net_device_stats *l2Stats  = NULL;

    int blink = 0, had_ip = 0;
    short gpio_pin = 0xFFFF, gpiofail_pin = 0xFFFF;

    set_user_nice(current, 20);

    proc_get_inet_led_gpio(&gpio_pin);
    proc_get_inet_fail_led_gpio(&gpiofail_pin);

    while (!kthread_should_stop())
    {

        if (head &&((XdslWanType > 0 && get_dsl_state && get_dsl_state()) ||
                     (EthWanType > 0 && get_eth_state && get_eth_state()) ||
                     (ThriGWanType > 0 )))  //for 3G Internet LED
        {
            if (down_trylock(&tarfficInfo_lock))
            {
                schedule_timeout_interruptible((HZ * INET_LED_POLL_TIME_MSEC) / 1000);
                continue;
            }

            blink = had_ip = 0;

            for (ptr = head; ptr; ptr = ptr->next)
            {
                /* interface up */
                if ((dev = dev_get_by_name(&init_net, ptr->dev_name)) && (dev->flags & IFF_UP))
                {
                    /* interface has IPv4 or IPv6 address */
//<< MHTsai : Support Internet LED for ip extension, 2010/10/14
//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
                    /* Check if dev and br0 have IPv4 and IPv6 global address */
                    brdev = dev_get_by_name(&init_net, "br0");
                    if (has_ip_addr(dev) || has_ipv6_global_addr(dev) || has_ipv6_global_addr(brdev) || (strcmp(intLedStatus, "g") == 0))
//>> [CTFN-SYS-016-2] End
//>> MHTsai : End
                    {
                        had_ip = 1;
                        stats = dev->get_stats(dev);
//<< Camille, use rx_packets only
						/* interface activity */
                        if (stats->rx_packets != ptr->rx_packets)
                        {
                            blink = 1;
				  // For faster screening, we can break out from here on the expenses of slightly less accuracy
                            //dev_put(dev);
				 //continue;
                        }
                            ptr->tx_packets = stats->tx_packets;
                            ptr->rx_packets = stats->rx_packets;

                        if (!ptr->dev_name_l2 || !memcmp(ptr->dev_name_l2, ptr->dev_name, sizeof(ptr->dev_name)))
			{
			    if (dev)
		            {
		                dev_put(dev);
				dev=NULL;
			    }
                            continue;
                        }

                        if ((l2Dev = dev_get_by_name(&init_net, ptr->dev_name_l2)) && (l2Dev->flags & IFF_UP))
                        {
                            l2Stats = l2Dev->get_stats(l2Dev);
                            if (l2Stats->rx_packets != ptr->l2Rx_packets)
                            {
                                blink = 1;
                            }
                                ptr->l2Tx_packets = l2Stats->tx_packets;
                                ptr->l2Rx_packets = l2Stats->rx_packets;
                        }
//>> End, Camille, use rx_packets only
                        if (l2Dev) {
                            dev_put(l2Dev);
                            l2Dev=NULL;
                    }
                }
                }
                if (dev) {
                    dev_put(dev);
                    dev=NULL;
                }
            }

            up(&tarfficInfo_lock);

            if (blink == 1)
            {
                BlinkGpio(gpio_pin);
//<< Dylan : Support Internet LED for 6328 product
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96318)
                SetInternetLed(gpiofail_pin, kGpioInactive);
//<< Lucien Huang : Support Internet LED for 963268 product, 2012/01/17
#elif defined(CONFIG_BCM963268)
                boardLedCtrl(kLedInetFail, kLedStateOff);
//>> Lucien Huang : End
#else
                kerSysSetGpioState(gpiofail_pin, kGpioInactive);
#endif
//>> Dylan : End
            }
            else
            {
//<< MHTsai : Support Internet LED for ip extension, 2010/10/14
                if (had_ip == 1 || ( strcmp(intLedStatus, "g") == 0 ))
//>> MHTsai : End
                {
                    /* turn internet LED green */
//<< Dylan : Support Internet LED for 6328 product
#if defined(CONFIG_BCM96328)  || defined(CONFIG_BCM96318)
                    SetInternetLed(gpio_pin, kGpioActive);
                    SetInternetLed(gpiofail_pin, kGpioInactive);
//<< Lucien Huang : Support Internet LED for 963268 product, 2012/01/17
#elif defined(CONFIG_BCM963268)
                    boardLedCtrl(kLedInetOn, kLedStateOn);
                    boardLedCtrl(kLedInetFail, kLedStateOff);
//>> Lucien Huang : End
#else
                    kerSysSetGpioState(gpio_pin, kGpioActive);
                    kerSysSetGpioState(gpiofail_pin, kGpioInactive);
#endif
//>> Dylan : End
                }
//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
//<< [JAZ-031] Lucien Huang : Turn off Internet LED when the CPE boots up, is synchronizing DSL line and later negotiating PPP (TTS #3024), 2012/09/26
/* Enable this section based on customer request currently
*  Turn off Internet LED when
*  1. the CPE boots up, is synchronizing DSL line and later negotiating PPP.
*  2. removing PPPoE or IPoE service */
#if 1
//>> [JAZ-031] End
                else if ((wanConnStatus == WAN_CONNECTING) || (wanConnStatus == WAN_DISCONNECTING))
                {
                    // turn internet LED off
#if defined(CONFIG_BCM96328)  || defined(CONFIG_BCM96318)
                    SetInternetLed(gpio_pin, kGpioInactive);
                    SetInternetLed(gpiofail_pin, kGpioInactive);
#elif defined(CONFIG_BCM963268)
                    boardLedCtrl(kLedInetOn, kLedStateOff);
                    boardLedCtrl(kLedInetFail, kLedStateOff);
#else
                    kerSysSetGpioState(gpio_pin, kGpioInactive);
                    kerSysSetGpioState(gpiofail_pin, kGpioInactive);
#endif
                }
#endif
//>> [CTFN-SYS-016-2] End
                else
                {
                    /* turn internet LED red */
//<< Dylan : Support Internet LED for 6328 product
#if defined(CONFIG_BCM96328)  || defined(CONFIG_BCM96318)
                    SetInternetLed(gpio_pin, kGpioInactive);
                    SetInternetLed(gpiofail_pin, kGpioActive);
//<< Lucien Huang : Support Internet LED for 963268 product, 2012/01/17
#elif defined(CONFIG_BCM963268)
                    boardLedCtrl(kLedInetOn, kLedStateOff);
                    boardLedCtrl(kLedInetFail, kLedStateOn);
//>> Lucien Huang : End
#else
                    kerSysSetGpioState(gpio_pin, kGpioInactive);
                    kerSysSetGpioState(gpiofail_pin, kGpioActive);
#endif
//>> Dylan : End
                }
            }
        }
        else
        {
            /* turn internet LED off */
//<< Dylan : Support Internet LED for 6328 product
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96318)
            SetInternetLed(gpio_pin, kGpioInactive);
            SetInternetLed(gpiofail_pin, kGpioInactive);
//<< Lucien Huang : Support Internet LED for 963268 product, 2012/01/17
#elif defined(CONFIG_BCM963268)
            boardLedCtrl(kLedInetOn, kLedStateOff);
            boardLedCtrl(kLedInetFail, kLedStateOff);
//>> Lucien Huang : End
#else
            kerSysSetGpioState(gpio_pin, kGpioInactive);
            kerSysSetGpioState(gpiofail_pin, kGpioInactive);
#endif
//>> Dylan : End
        }

        schedule_timeout_interruptible((HZ * INET_LED_POLL_TIME_MSEC) / 1000);
    }

    if (head)
    {
        for (ptr = head; ptr;)
        {
            next = ptr->next;
            kfree(ptr);
            ptr = next;
        }
        head = NULL;
    }
    return 0;
}

static void init_timer_handle(void)
{
    //unsigned short gpio_pin;

    //BpGetPppLedGpio(&gpio_pin);
    //GPIO->GPIOMode &= (uint32)(~(1 << gpio_pin)); //GPIO Pin Mode Control Register

    if (IS_ERR(blink_thread = kthread_run(blink_fun, NULL, "BlinkInterLED")))
    {
        blink_thread = NULL;
        printk("brcm flash: Blink Internet LED thread start fail\n");
    }

    return;
}
//>> [CTFN-SYS-016] End

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
struct task_struct *blinkusbled_thread = NULL;

static int blinkusbled_fun(void *data){

    set_user_nice(current, 19);

    while (!kthread_should_stop()){
		if(have_usb_data_pass){
			boardLedCtrl(kLedUsbOn, kLedStateOff);
			schedule_timeout_interruptible((HZ * USB_LED_POLL_TIME_MSEC) / 1000);
			boardLedCtrl(kLedUsbOn, kLedStateOn);
			have_usb_data_pass = 0;
			usbLedStatus = usbLedOn;
		}
        else {
	        if (usbLedStatus == usbLedOff)   /* turn USB LED off */
	        {
	            boardLedCtrl(kLedUsbOn, kLedStateOff);
	            boardLedCtrl(kLedUsbFail, kLedStateOff);
	        }
	        else if (usbLedStatus == usbLedOn)   /* turn USB LED green */
	        {
	            boardLedCtrl(kLedUsbOn, kLedStateOn);
	            boardLedCtrl(kLedUsbFail, kLedStateOff);
	        }
	        else if (usbLedStatus == usbLedFail)   /* turn USB LED green */
	        {
	            boardLedCtrl(kLedUsbOn, kLedStateOff);
	            boardLedCtrl(kLedUsbFail, kLedStateOn);
	        }			
        }

        schedule_timeout_interruptible((HZ * USB_LED_POLL_TIME_MSEC) / 1000);
    }
    return 0;
}

static void usbLedInit(void)
{
    if (IS_ERR(blinkusbled_thread = kthread_run(blinkusbled_fun, NULL, "BlinkUSBLED")))
    {
        blinkusbled_thread = NULL;
        printk("brcm flash: Blink USB LED thread failed\n");
    }

    return;
}
//>> [JAZ-026] End

#if defined(MODULE)
int init_module(void)
{
    return( brcm_board_init() );
}

void cleanup_module(void)
{
    if (MOD_IN_USE)
        printk("brcm flash: cleanup_module failed because module is in use\n");
    else
        brcm_board_cleanup();
}
#endif //MODULE

static int map_external_irq (int irq)
{
    int map_irq;

    switch (irq) {
    case BP_EXT_INTR_0   :
        map_irq = INTERRUPT_ID_EXTERNAL_0;
        break ;
    case BP_EXT_INTR_1   :
        map_irq = INTERRUPT_ID_EXTERNAL_1;
        break ;
    case BP_EXT_INTR_2   :
        map_irq = INTERRUPT_ID_EXTERNAL_2;
        break ;
    case BP_EXT_INTR_3   :
        map_irq = INTERRUPT_ID_EXTERNAL_3;
        break ;
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    case BP_EXT_INTR_4   :
        map_irq = INTERRUPT_ID_EXTERNAL_4;
        break ;
    case BP_EXT_INTR_5   :
        map_irq = INTERRUPT_ID_EXTERNAL_5;
        break ;
#endif
    default           :
        printk ("Invalid External Interrupt definition \n") ;
        map_irq = 0 ;
        break ;
    }
    return (map_irq) ;
}

/* A global variable used by Power Management and other features to determine if Voice is idle or not */
volatile int isVoiceIdle = 1;
EXPORT_SYMBOL(isVoiceIdle);

#if defined(CONFIG_BCM96368)
static unsigned long getMemoryWidth(void)
{
    unsigned long memCfg;

    memCfg = MEMC->Config;
    memCfg &= MEMC_WIDTH_MASK;
    memCfg >>= MEMC_WIDTH_SHFT;

    return memCfg;
}
#endif

static int __init brcm_board_init( void )
{
    unsigned short rstToDflt_irq;
    int ret;
    bcmLogSpiCallbacks_t loggingCallbacks;

    ret = register_chrdev(BOARD_DRV_MAJOR, "brcmboard", &board_fops );
    if (ret < 0)
        printk( "brcm_board_init(major %d): fail to register device.\n",BOARD_DRV_MAJOR);
    else
    {
        printk("brcmboard: brcm_board_init entry\n");
        board_major = BOARD_DRV_MAJOR;

        g_ulSdramSize = getMemorySize();
#if defined(CONFIG_BCM96368)
        g_ulSdramWidth = getMemoryWidth();
#endif
        set_mac_info();
        set_gpon_info();

        init_waitqueue_head(&g_board_wait_queue);
#if defined (WIRELESS)
        kerSysScreenPciDevices();
        ses_board_init();
        kerSetWirelessPD(WLAN_ON);
#endif
#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
        kerSysCheckPowerDownPcie();
#endif
        kerSysInitMonitorSocket();

//<< [CTFN-SYS-023] Jimmy Wu : Add macro definition to disable dying gasp
#ifndef CTCONFIG_SYS_DISABLE_DYING_GASP
        kerSysInitDyingGaspHandler();
#endif /* CTCONFIG_SYS_DISABLE_DYING_GASP */
//>> [CTFN-SYS-023] End

        boardLedInit();
        g_ledInitialized = 1;

        if( BpGetResetToDefaultExtIntr(&rstToDflt_irq) == BP_SUCCESS )
        {
            rstToDflt_irq = map_external_irq (rstToDflt_irq) ;
            BcmHalMapInterrupt((FN_HANDLER)reset_isr, 0, rstToDflt_irq);
            BcmHalInterruptEnable(rstToDflt_irq);
        }
//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
        init_timer_handle();
//>> [CTFN-SYS-016] End

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
        usbLedInit();
//>> [JAZ-026] End

#if defined(CONFIG_BCM_CPLD1)
        // Reserve SPI bus to control external CPLD for Standby Timer
        BcmCpld1Initialize();
#endif
    }

    add_proc_files();

#if defined(CONFIG_BCM96816)
    board_Init6829();
    loggingCallbacks.kerSysSlaveRead = kerSysBcmSpiSlaveRead;
    loggingCallbacks.kerSysSlaveWrite = kerSysBcmSpiSlaveWrite;
    loggingCallbacks.bpGet6829PortInfo = BpGet6829PortInfo;
#else
    loggingCallbacks.kerSysSlaveRead   = NULL;
    loggingCallbacks.kerSysSlaveWrite  = NULL;
    loggingCallbacks.bpGet6829PortInfo = NULL;
#endif
    loggingCallbacks.reserveSlave = BcmSpiReserveSlave;
    loggingCallbacks.syncTrans = BcmSpiSyncTrans;
    bcmLog_registerSpiCallbacks(loggingCallbacks);

   return ret;
}

static void __init set_mac_info( void )
{
    NVRAM_DATA *pNvramData;
    unsigned long ulNumMacAddrs;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("set_mac_info: could not read nvram data\n");
        return;
    }

    ulNumMacAddrs = pNvramData->ulNumMacAddrs;

    if( ulNumMacAddrs > 0 && ulNumMacAddrs <= NVRAM_MAC_COUNT_MAX )
    {
        unsigned long ulMacInfoSize =
            sizeof(MAC_INFO) + ((sizeof(MAC_ADDR_INFO) - 1) * ulNumMacAddrs);

        g_pMacInfo = (PMAC_INFO) kmalloc( ulMacInfoSize, GFP_KERNEL );

        if( g_pMacInfo )
        {
            memset( g_pMacInfo, 0x00, ulMacInfoSize );
            g_pMacInfo->ulNumMacAddrs = pNvramData->ulNumMacAddrs;
            memcpy( g_pMacInfo->ucaBaseMacAddr, pNvramData->ucaBaseMacAddr,
                NVRAM_MAC_ADDRESS_LEN );
        }
        else
            printk("ERROR - Could not allocate memory for MAC data\n");
    }
    else
        printk("ERROR - Invalid number of MAC addresses (%ld) is configured.\n",
        ulNumMacAddrs);
    kfree(pNvramData);
}

static int gponParamsAreErased(NVRAM_DATA *pNvramData)
{
    int i;
    int erased = 1;

    for(i=0; i<NVRAM_GPON_SERIAL_NUMBER_LEN-1; ++i) {
        if((pNvramData->gponSerialNumber[i] != (char) 0xFF) &&
            (pNvramData->gponSerialNumber[i] != (char) 0x00)) {
                erased = 0;
                break;
        }
    }

    if(!erased) {
        for(i=0; i<NVRAM_GPON_PASSWORD_LEN-1; ++i) {
            if((pNvramData->gponPassword[i] != (char) 0xFF) &&
                (pNvramData->gponPassword[i] != (char) 0x00)) {
                    erased = 0;
                    break;
            }
        }
    }

    return erased;
}

static void __init set_gpon_info( void )
{
    NVRAM_DATA *pNvramData;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("set_gpon_info: could not read nvram data\n");
        return;
    }

    g_pGponInfo = (PGPON_INFO) kmalloc( sizeof(GPON_INFO), GFP_KERNEL );

    if( g_pGponInfo )
    {
        if ((pNvramData->ulVersion < NVRAM_FULL_LEN_VERSION_NUMBER) ||
            gponParamsAreErased(pNvramData))
        {
            strcpy( g_pGponInfo->gponSerialNumber, DEFAULT_GPON_SN );
            strcpy( g_pGponInfo->gponPassword, DEFAULT_GPON_PW );
        }
        else
        {
            strncpy( g_pGponInfo->gponSerialNumber, pNvramData->gponSerialNumber,
                NVRAM_GPON_SERIAL_NUMBER_LEN );
            g_pGponInfo->gponSerialNumber[NVRAM_GPON_SERIAL_NUMBER_LEN-1]='\0';
            strncpy( g_pGponInfo->gponPassword, pNvramData->gponPassword,
                NVRAM_GPON_PASSWORD_LEN );
            g_pGponInfo->gponPassword[NVRAM_GPON_PASSWORD_LEN-1]='\0';
        }
    }
    else
    {
        printk("ERROR - Could not allocate memory for GPON data\n");
    }
    kfree(pNvramData);
}

void __exit brcm_board_cleanup( void )
{
    printk("brcm_board_cleanup()\n");
    del_proc_files();

    if (board_major != -1)
    {
#if defined (WIRELESS)
        ses_board_deinit();
#endif
        kerSysDeinitDyingGaspHandler();
        kerSysCleanupMonitorSocket();
        unregister_chrdev(board_major, "board_ioctl");

    }
//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
    if (blink_thread)
        kthread_stop(blink_thread);
//>> [CTFN-SYS-016] End

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
if (blinkusbled_thread)
	kthread_stop(blinkusbled_thread);
//>> [JAZ-026] End
}

static BOARD_IOC* borad_ioc_alloc(void)
{
    BOARD_IOC *board_ioc =NULL;
    board_ioc = (BOARD_IOC*) kmalloc( sizeof(BOARD_IOC) , GFP_KERNEL );
    if(board_ioc)
    {
        memset(board_ioc, 0, sizeof(BOARD_IOC));
    }
    return board_ioc;
}

static void borad_ioc_free(BOARD_IOC* board_ioc)
{
    if(board_ioc)
    {
        kfree(board_ioc);
    }
}


static int board_open( struct inode *inode, struct file *filp )
{
    filp->private_data = borad_ioc_alloc();

    if (filp->private_data == NULL)
        return -ENOMEM;

    return( 0 );
}

static int board_release(struct inode *inode, struct file *filp)
{
    BOARD_IOC *board_ioc = filp->private_data;

    wait_event_interruptible(g_board_wait_queue, 1);
    borad_ioc_free(board_ioc);

    return( 0 );
}


static unsigned int board_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
#if defined (WIRELESS)
    BOARD_IOC *board_ioc = filp->private_data;
#endif

    poll_wait(filp, &g_board_wait_queue, wait);
#if defined (WIRELESS)
    if(board_ioc->eventmask & SES_EVENTS){
        mask |= sesBtn_poll(filp, wait);
    }
#endif

    return mask;
}

static ssize_t board_read(struct file *filp,  char __user *buffer, size_t count, loff_t *ppos)
{
#if defined (WIRELESS)
    BOARD_IOC *board_ioc = filp->private_data;
    if(board_ioc->eventmask & SES_EVENTS){
        return sesBtn_read(filp, buffer, count, ppos);
    }
#endif
    return 0;
}

/***************************************************************************
// Function Name: getCrc32
// Description  : caculate the CRC 32 of the given data.
// Parameters   : pdata - array of data.
//                size - number of input data bytes.
//                crc - either CRC32_INIT_VALUE or previous return value.
// Returns      : crc.
****************************************************************************/
static UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc)
{
    while (size-- > 0)
        crc = (crc >> 8) ^ Crc32_table[(crc ^ *pdata++) & 0xff];

    return crc;
}

/** calculate the CRC for the nvram data block and write it to flash.
 * Must be called with flashImageMutex held.
 */
static void writeNvramDataCrcLocked(PNVRAM_DATA pNvramData)
{
    UINT32 crc = CRC32_INIT_VALUE;

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

    pNvramData->ulCheckSum = 0;
    crc = getCrc32((char *)pNvramData, sizeof(NVRAM_DATA), crc);
    pNvramData->ulCheckSum = crc;
    kerSysNvRamSet((char *)pNvramData, sizeof(NVRAM_DATA), 0);
}


/** read the nvramData struct from the in-memory copy of nvram.
 * The caller is not required to have flashImageMutex when calling this
 * function.  However, if the caller is doing a read-modify-write of
 * the nvram data, then the caller must hold flashImageMutex.  This function
 * does not know what the caller is going to do with this data, so it
 * cannot assert flashImageMutex held or not when this function is called.
 *
 * @return pointer to NVRAM_DATA buffer which the caller must free
 *         or NULL if there was an error
 */
static PNVRAM_DATA readNvramData(void)
{
    UINT32 crc = CRC32_INIT_VALUE, savedCrc;
    NVRAM_DATA *pNvramData;

    // use GFP_ATOMIC here because caller might have flashImageMutex held
    if (NULL == (pNvramData = kmalloc(sizeof(NVRAM_DATA), GFP_ATOMIC)))
    {
        printk("readNvramData: could not allocate memory\n");
        return NULL;
    }

    kerSysNvRamGet((char *)pNvramData, sizeof(NVRAM_DATA), 0);
    savedCrc = pNvramData->ulCheckSum;
    pNvramData->ulCheckSum = 0;
    crc = getCrc32((char *)pNvramData, sizeof(NVRAM_DATA), crc);
    if (savedCrc != crc)
    {
        // this can happen if we write a new cfe image into flash.
        // The new image will have an invalid nvram section which will
        // get updated to the inMemNvramData.  We detect it here and
        // commonImageWrite will restore previous copy of nvram data.
        kfree(pNvramData);
        pNvramData = NULL;
    }

    return pNvramData;
}



//**************************************************************************************
// Utitlities for dump memory, free kernel pages, mips soft reset, etc.
//**************************************************************************************

/***********************************************************************
* Function Name: dumpaddr
* Description  : Display a hex dump of the specified address.
***********************************************************************/
void dumpaddr( unsigned char *pAddr, int nLen )
{
    static char szHexChars[] = "0123456789abcdef";
    char szLine[80];
    char *p = szLine;
    unsigned char ch, *q;
    int i, j;
    unsigned long ul;

    while( nLen > 0 )
    {
        sprintf( szLine, "%8.8lx: ", (unsigned long) pAddr );
        p = szLine + strlen(szLine);

        for(i = 0; i < 16 && nLen > 0; i += sizeof(long), nLen -= sizeof(long))
        {
            ul = *(unsigned long *) &pAddr[i];
            q = (unsigned char *) &ul;
            for( j = 0; j < sizeof(long); j++ )
            {
                *p++ = szHexChars[q[j] >> 4];
                *p++ = szHexChars[q[j] & 0x0f];
                *p++ = ' ';
            }
        }

        for( j = 0; j < 16 - i; j++ )
            *p++ = ' ', *p++ = ' ', *p++ = ' ';

        *p++ = ' ', *p++ = ' ', *p++ = ' ';

        for( j = 0; j < i; j++ )
        {
            ch = pAddr[j];
            *p++ = (ch > ' ' && ch < '~') ? ch : '.';
        }

        *p++ = '\0';
        printk( "%s\r\n", szLine );

        pAddr += i;
    }
    printk( "\r\n" );
} /* dumpaddr */


/** this function actually does two things, stop other cpu and reset mips.
 * Kept the current name for compatibility reasons.  Image upgrade code
 * needs to call the two steps separately.
 */
void kerSysMipsSoftReset(void)
{
	unsigned long cpu;
	cpu = smp_processor_id();
	printk(KERN_INFO "kerSysMipsSoftReset: called on cpu %lu\n", cpu);

	stopOtherCpu();
	local_irq_disable();  // ignore interrupts, just execute reset code now
	resetPwrmgmtDdrMips();
}

extern void stop_other_cpu(void);  // in arch/mips/kernel/smp.c

void stopOtherCpu(void)
{
#if defined(CONFIG_SMP)
    stop_other_cpu();
#elif defined(CONFIG_BCM_ENDPOINT_MODULE) && defined(CONFIG_BCM_BCMDSP_MODULE)
    unsigned long cpu = (read_c0_diag3() >> 31) ? 0 : 1;

	// Disable interrupts on the other core and allow it to complete processing 
	// and execute the "wait" instruction
    printk(KERN_INFO "stopOtherCpu: stopping cpu %lu\n", cpu);	
    PERF->IrqControl[cpu].IrqMask = 0;
    mdelay(5);
#endif
}

void resetPwrmgmtDdrMips(void)
{
#if defined (CONFIG_BCM963268)
    MISC->miscVdslControl &= ~(MISC_VDSL_CONTROL_VDSL_MIPS_RESET | MISC_VDSL_CONTROL_VDSL_MIPS_POR_RESET );
#endif

#if !defined (CONFIG_BCM96816)
    // Power Management on Ethernet Ports may have brought down EPHY PLL
    // and soft reset below will lock-up 6362 if the PLL is not up
    // therefore bring it up here to give it time to stabilize
    GPIO->RoboswEphyCtrl &= ~EPHY_PWR_DOWN_DLL;
#endif

    // let UART finish printing
    udelay(100);


#if defined(CONFIG_BCM_CPLD1)
    // Determine if this was a request to enter Standby mode
    // If yes, this call won't return and a hard reset will occur later
    BcmCpld1CheckShutdownMode();
#endif

#if defined (CONFIG_BCM96368)
    {
        volatile int delay;
        volatile int i;
        local_irq_disable();
        // after we reset DRAM controller we can't access DRAM, so
        // the first iteration put things in i-cache and the scond interation do the actual reset
        for (i=0; i<2; i++) {
            DDR->DDR1_2PhaseCntl0 &= i - 1;
            DDR->DDR3_4PhaseCntl0 &= i - 1;

            if( i == 1 )
                ChipSoftReset();

            delay = 1000;
            while (delay--);
            PERF->pll_control |= SOFT_RESET*i;
            for(;i;) {} // spin mips and wait soft reset to take effect
        }
    }
#endif
#if !defined(CONFIG_BCM96328) && !defined(CONFIG_BCM96318)
#if defined (CONFIG_BCM96816)
    /* Work around reset issues */
    HVG_MISC_REG_CHANNEL_A->mask |= HVG_SOFT_INIT_0;
    HVG_MISC_REG_CHANNEL_B->mask |= HVG_SOFT_INIT_0;

    {
        unsigned char portInfo6829;
        /* for BHRGR board we need to toggle GPIO30 to
           reset - on early BHR baords this is the GPHY2
           link100 so setting it does not matter */
        if ( (BP_SUCCESS == BpGet6829PortInfo(&portInfo6829)) &&
             (0 != portInfo6829))
        {
            GPIO->GPIODir |= 1<<30;
            GPIO->GPIOio  &= ~(1<<30);
        }
    }
#endif
    PERF->pll_control |= SOFT_RESET;    // soft reset mips
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    PERF->pll_control = 0;
#endif
#else
    TIMER->SoftRst = 1;
#endif
    for(;;) {} // spin mips and wait soft reset to take effect
}

unsigned long kerSysGetMacAddressType( unsigned char *ifName )
{
    unsigned long macAddressType = MAC_ADDRESS_ANY;

    if(strstr(ifName, IF_NAME_ETH))
    {
        macAddressType = MAC_ADDRESS_ETH;
    }
    else if(strstr(ifName, IF_NAME_USB))
    {
        macAddressType = MAC_ADDRESS_USB;
    }
    else if(strstr(ifName, IF_NAME_WLAN))
    {
        macAddressType = MAC_ADDRESS_WLAN;
    }
    else if(strstr(ifName, IF_NAME_MOCA))
    {
        macAddressType = MAC_ADDRESS_MOCA;
    }
    else if(strstr(ifName, IF_NAME_ATM))
    {
        macAddressType = MAC_ADDRESS_ATM;
    }
    else if(strstr(ifName, IF_NAME_PTM))
    {
        macAddressType = MAC_ADDRESS_PTM;
    }
    else if(strstr(ifName, IF_NAME_GPON) || strstr(ifName, IF_NAME_VEIP))
    {
        macAddressType = MAC_ADDRESS_GPON;
    }
    else if(strstr(ifName, IF_NAME_EPON))
    {
        macAddressType = MAC_ADDRESS_EPON;
    }

    return macAddressType;
}

int kerSysGetMacAddress( unsigned char *pucaMacAddr, unsigned long ulId )
{
    const unsigned long constMacAddrIncIndex = 3;
    int nRet = 0;
    PMAC_ADDR_INFO pMai = NULL;
    PMAC_ADDR_INFO pMaiFreeNoId = NULL;
    PMAC_ADDR_INFO pMaiFreeId = NULL;
    unsigned long i = 0, ulIdxNoId = 0, ulIdxId = 0, baseMacAddr = 0;

    mutex_lock(&macAddrMutex);

    /* baseMacAddr = last 3 bytes of the base MAC address treated as a 24 bit integer */
    memcpy((unsigned char *) &baseMacAddr,
        &g_pMacInfo->ucaBaseMacAddr[constMacAddrIncIndex],
        NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
    baseMacAddr >>= 8;

    for( i = 0, pMai = g_pMacInfo->MacAddrs; i < g_pMacInfo->ulNumMacAddrs;
        i++, pMai++ )
    {
        if( ulId == pMai->ulId || ulId == MAC_ADDRESS_ANY )
        {
            /* This MAC address has been used by the caller in the past. */
            baseMacAddr = (baseMacAddr + i) << 8;
            memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                constMacAddrIncIndex);
            memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
            pMai->chInUse = 1;
            pMaiFreeNoId = pMaiFreeId = NULL;
            break;
        }
        else
            if( pMai->chInUse == 0 )
            {
                if( pMai->ulId == 0 && pMaiFreeNoId == NULL )
                {
                    /* This is an available MAC address that has never been
                    * used.
                    */
                    pMaiFreeNoId = pMai;
                    ulIdxNoId = i;
                }
                else
                    if( pMai->ulId != 0 && pMaiFreeId == NULL )
                    {
                        /* This is an available MAC address that has been used
                        * before.  Use addresses that have never been used
                        * first, before using this one.
                        */
                        pMaiFreeId = pMai;
                        ulIdxId = i;
                    }
            }
    }

    if( pMaiFreeNoId || pMaiFreeId )
    {
        /* An available MAC address was found. */
        memcpy(pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,NVRAM_MAC_ADDRESS_LEN);
        if( pMaiFreeNoId )
        {
            baseMacAddr = (baseMacAddr + ulIdxNoId) << 8;
            memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                constMacAddrIncIndex);
            memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
            pMaiFreeNoId->ulId = ulId;
            pMaiFreeNoId->chInUse = 1;
        }
        else
        {
            baseMacAddr = (baseMacAddr + ulIdxId) << 8;
            memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                constMacAddrIncIndex);
            memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
            pMaiFreeId->ulId = ulId;
            pMaiFreeId->chInUse = 1;
        }
    }
    else
        if( i == g_pMacInfo->ulNumMacAddrs )
            nRet = -EADDRNOTAVAIL;

    mutex_unlock(&macAddrMutex);

    return( nRet );
} /* kerSysGetMacAddr */

int kerSysReleaseMacAddress( unsigned char *pucaMacAddr )
{
    const unsigned long constMacAddrIncIndex = 3;
    int nRet = -EINVAL;
    unsigned long ulIdx = 0;
    unsigned long baseMacAddr = 0;
    unsigned long relMacAddr = 0;

    mutex_lock(&macAddrMutex);

    /* baseMacAddr = last 3 bytes of the base MAC address treated as a 24 bit integer */
    memcpy((unsigned char *) &baseMacAddr,
        &g_pMacInfo->ucaBaseMacAddr[constMacAddrIncIndex],
        NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
    baseMacAddr >>= 8;

    /* Get last 3 bytes of MAC address to release. */
    memcpy((unsigned char *) &relMacAddr, &pucaMacAddr[constMacAddrIncIndex],
        NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
    relMacAddr >>= 8;

    ulIdx = relMacAddr - baseMacAddr;

    if( ulIdx < g_pMacInfo->ulNumMacAddrs )
    {
        PMAC_ADDR_INFO pMai = &g_pMacInfo->MacAddrs[ulIdx];
        if( pMai->chInUse == 1 )
        {
            pMai->chInUse = 0;
            nRet = 0;
        }
    }

    mutex_unlock(&macAddrMutex);

    return( nRet );
} /* kerSysReleaseMacAddr */


void kerSysGetGponSerialNumber( unsigned char *pGponSerialNumber )
{
    strcpy( pGponSerialNumber, g_pGponInfo->gponSerialNumber );
}


void kerSysGetGponPassword( unsigned char *pGponPassword )
{
    strcpy( pGponPassword, g_pGponInfo->gponPassword );
}

int kerSysGetSdramSize( void )
{
    return( (int) g_ulSdramSize );
} /* kerSysGetSdramSize */


#if defined(CONFIG_BCM96368)
/*
 * This function returns:
 * MEMC_32BIT_BUS for 32-bit SDRAM
 * MEMC_16BIT_BUS for 16-bit SDRAM
 */
unsigned int kerSysGetSdramWidth( void )
{
    return (unsigned int)(g_ulSdramWidth);
} /* kerSysGetSdramWidth */
#endif


/*Read Wlan Params data from CFE */
int kerSysGetWlanSromParams( unsigned char *wlanParams, unsigned short len)
{
    NVRAM_DATA *pNvramData;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("kerSysGetWlanSromParams: could not read nvram data\n");
        return -1;
    }

    memcpy( wlanParams,
           (char *)pNvramData + ((size_t) &((NVRAM_DATA *)0)->wlanParams),
            len );
    kfree(pNvramData);

    return 0;
}

/*Read Wlan Params data from CFE */
int kerSysGetAfeId( unsigned long *afeId )
{
    NVRAM_DATA *pNvramData;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("kerSysGetAfeId: could not read nvram data\n");
        return -1;
    }

    afeId [0] = pNvramData->afeId[0];
    afeId [1] = pNvramData->afeId[1];
    kfree(pNvramData);

    return 0;
}

void kerSysLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
    if (g_ledInitialized)
        boardLedCtrl(ledName, ledState);
}

/*functionto receive message from usersapce
 * Currently we dont expect any messages fromm userspace
 */
void kerSysRecvFrmMonitorTask(struct sk_buff *skb)
{

   /*process the message here*/
   printk(KERN_WARNING "unexpected skb received at %s \n",__FUNCTION__);
   kfree_skb(skb);
   return;
}

void kerSysInitMonitorSocket( void )
{
   g_monitor_nl_sk = netlink_kernel_create(&init_net, NETLINK_BRCM_MONITOR, 0, kerSysRecvFrmMonitorTask, NULL, THIS_MODULE);

   if(!g_monitor_nl_sk)
   {
      printk(KERN_ERR "Failed to create a netlink socket for monitor\n");
      return;
   }

}


void kerSysSendtoMonitorTask(int msgType, char *msgData, int msgDataLen)
{

   struct sk_buff *skb =  NULL;
   struct nlmsghdr *nl_msgHdr = NULL;
   unsigned int payloadLen =sizeof(struct nlmsghdr);

   if(!g_monitor_nl_pid)
   {
      printk(KERN_INFO "message received before monitor task is initialized %s \n",__FUNCTION__);
      return;
   } 

   if(msgData && (msgDataLen > MAX_PAYLOAD_LEN))
   {
      printk(KERN_ERR "invalid message len in %s",__FUNCTION__);
      return;
   } 

   payloadLen += msgDataLen;
   payloadLen = NLMSG_SPACE(payloadLen);

   /*Alloc skb ,this check helps to call the fucntion from interrupt context */

   if(in_atomic())
   {
      skb = alloc_skb(payloadLen, GFP_ATOMIC);
   }
   else
   {
      skb = alloc_skb(payloadLen, GFP_KERNEL);
   }

   if(!skb)
   {
      printk(KERN_ERR "failed to alloc skb in %s",__FUNCTION__);
      return;
   }

   nl_msgHdr = (struct nlmsghdr *)skb->data;
   nl_msgHdr->nlmsg_type = msgType;
   nl_msgHdr->nlmsg_pid=0;/*from kernel */
   nl_msgHdr->nlmsg_len = payloadLen;
   nl_msgHdr->nlmsg_flags =0;

   if(msgData)
   {
      memcpy(NLMSG_DATA(nl_msgHdr),msgData,msgDataLen);
   }      

   NETLINK_CB(skb).pid = 0; /*from kernel */

   skb->len = payloadLen; 

   netlink_unicast(g_monitor_nl_sk, skb, g_monitor_nl_pid, MSG_DONTWAIT);
   return;
}

void kerSysCleanupMonitorSocket(void)
{
   g_monitor_nl_pid = 0 ;
   sock_release(g_monitor_nl_sk->sk_socket);
}

// Must be called with flashImageMutex held
static PFILE_TAG getTagFromPartition(int imageNumber)
{
    static unsigned char sectAddr1[sizeof(FILE_TAG) + sizeof(int)];
    static unsigned char sectAddr2[sizeof(FILE_TAG) + sizeof(int)];
    int blk = 0;
    UINT32 crc;
    PFILE_TAG pTag = NULL;
    unsigned char *pBase = flash_get_memptr(0);
    unsigned char *pSectAddr = NULL;

    /* The image tag for the first image is always after the boot loader.
    * The image tag for the second image, if it exists, is at one half
    * of the flash size.
    */
    if( imageNumber == 1 )
    {
        FLASH_ADDR_INFO flash_info;

        kerSysFlashAddrInfoGet(&flash_info);
        blk = flash_get_blk((int) (pBase+flash_info.flash_rootfs_start_offset));
        pSectAddr = sectAddr1;
    }
    else
        if( imageNumber == 2 )
        {
            blk = flash_get_blk((int) (pBase + (flash_get_total_size() / 2)));
            pSectAddr = sectAddr2;
        }

        if( blk )
        {
            int *pn;

            memset(pSectAddr, 0x00, sizeof(FILE_TAG));
            flash_read_buf((unsigned short) blk, 0, pSectAddr, sizeof(FILE_TAG));
            crc = CRC32_INIT_VALUE;
            crc = getCrc32(pSectAddr, (UINT32)TAG_LEN-TOKEN_LEN, crc);
            pTag = (PFILE_TAG) pSectAddr;
            pn = (int *) (pTag + 1);
            *pn = blk;
            if (crc != (UINT32)(*(UINT32*)(pTag->tagValidationToken)))
                pTag = NULL;
        }

        return( pTag );
}

// must be called with flashImageMutex held
static int getPartitionFromTag( PFILE_TAG pTag )
{
    int ret = 0;

    if( pTag )
    {
        PFILE_TAG pTag1 = getTagFromPartition(1);
        PFILE_TAG pTag2 = getTagFromPartition(2);
        int sequence = simple_strtoul(pTag->imageSequence,  NULL, 10);
        int sequence1 = (pTag1) ? simple_strtoul(pTag1->imageSequence, NULL, 10)
            : -1;
        int sequence2 = (pTag2) ? simple_strtoul(pTag2->imageSequence, NULL, 10)
            : -1;

        if( pTag1 && sequence == sequence1 )
            ret = 1;
        else
            if( pTag2 && sequence == sequence2 )
                ret = 2;
    }

    return( ret );
}


// must be called with flashImageMutex held
static PFILE_TAG getBootImageTag(void)
{
    static int displayFsAddr = 1;
    PFILE_TAG pTag = NULL;
    PFILE_TAG pTag1 = getTagFromPartition(1);
    PFILE_TAG pTag2 = getTagFromPartition(2);

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

    if( pTag1 && pTag2 )
    {
        /* Two images are flashed. */
        int sequence1 = simple_strtoul(pTag1->imageSequence, NULL, 10);
        int sequence2 = simple_strtoul(pTag2->imageSequence, NULL, 10);
        int imgid = 0;

        kerSysBlParmsGetInt(BOOTED_IMAGE_ID_NAME, &imgid);
        if( imgid == BOOTED_OLD_IMAGE )
            pTag = (sequence2 < sequence1) ? pTag2 : pTag1;
        else
            pTag = (sequence2 > sequence1) ? pTag2 : pTag1;
    }
    else
        /* One image is flashed. */
        pTag = (pTag2) ? pTag2 : pTag1;

    if( pTag && displayFsAddr )
    {
        displayFsAddr = 0;
        printk("File system address: 0x%8.8lx\n",
            simple_strtoul(pTag->rootfsAddress, NULL, 10) + BOOT_OFFSET);
    }

    return( pTag );
}

// Must be called with flashImageMutex held
static void UpdateImageSequenceNumber( unsigned char *imageSequence )
{
    int newImageSequence = 0;
    PFILE_TAG pTag = getTagFromPartition(1);

    if( pTag )
        newImageSequence = simple_strtoul(pTag->imageSequence, NULL, 10);

    pTag = getTagFromPartition(2);
    if(pTag && simple_strtoul(pTag->imageSequence, NULL, 10) > newImageSequence)
        newImageSequence = simple_strtoul(pTag->imageSequence, NULL, 10);

    newImageSequence++;
    sprintf(imageSequence, "%d", newImageSequence);
}

/* Must be called with flashImageMutex held */
static int flashFsKernelImage( unsigned char *imagePtr, int imageLen,
    int flashPartition, int *numPartitions )
{
    int status = 0;
    PFILE_TAG pTag = (PFILE_TAG) imagePtr;
    int rootfsAddr = simple_strtoul(pTag->rootfsAddress, NULL, 10);
    int kernelAddr = simple_strtoul(pTag->kernelAddress, NULL, 10);
    char *tagFs = imagePtr;
    unsigned int baseAddr = (unsigned int) flash_get_memptr(0);
    unsigned int totalSize = (unsigned int) flash_get_total_size();
    unsigned int reservedBytesAtEnd;
    unsigned int availableSizeOneImg;
    unsigned int reserveForTwoImages;
    unsigned int availableSizeTwoImgs;
    unsigned int newImgSize = simple_strtoul(pTag->rootfsLen, NULL, 10) +
        simple_strtoul(pTag->kernelLen, NULL, 10);
    PFILE_TAG pCurTag = getBootImageTag();
    int nCurPartition = getPartitionFromTag( pCurTag );
    int should_yield =
        (flashPartition == 0 || flashPartition == nCurPartition) ? 0 : 1;
    UINT32 crc;
    unsigned int curImgSize = 0;
    unsigned int rootfsOffset = (unsigned int) rootfsAddr - IMAGE_BASE - TAG_LEN;
    FLASH_ADDR_INFO flash_info;
    NVRAM_DATA *pNvramData;

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

    if (NULL == (pNvramData = readNvramData()))
    {
        return -ENOMEM;
    }

    kerSysFlashAddrInfoGet(&flash_info);
    if( rootfsOffset < flash_info.flash_rootfs_start_offset )
    {
        // Increase rootfs and kernel addresses by the difference between
        // rootfs offset and what it needs to be.
        rootfsAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
        kernelAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
        sprintf(pTag->rootfsAddress,"%lu", (unsigned long) rootfsAddr);
        sprintf(pTag->kernelAddress,"%lu", (unsigned long) kernelAddr);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
        *(unsigned long *) &pTag->tagValidationToken[0] = crc;
    }

    rootfsAddr += BOOT_OFFSET;
    kernelAddr += BOOT_OFFSET;

    reservedBytesAtEnd = flash_get_reserved_bytes_at_end(&flash_info);
    availableSizeOneImg = totalSize - ((unsigned int) rootfsAddr - baseAddr) -
        reservedBytesAtEnd;
    reserveForTwoImages =
        (flash_info.flash_rootfs_start_offset > reservedBytesAtEnd)
        ? flash_info.flash_rootfs_start_offset : reservedBytesAtEnd;
    availableSizeTwoImgs = (totalSize / 2) - reserveForTwoImages;

    //    printk("availableSizeOneImage=%dKB availableSizeTwoImgs=%dKB reserve=%dKB\n",
    //            availableSizeOneImg/1024, availableSizeTwoImgs/1024, reserveForTwoImages/1024);
    if( pCurTag )
    {
        curImgSize = simple_strtoul(pCurTag->rootfsLen, NULL, 10) +
            simple_strtoul(pCurTag->kernelLen, NULL, 10);
    }

    if( newImgSize > availableSizeOneImg)
    {
        printk("Illegal image size %d.  Image size must not be greater "
            "than %d.\n", newImgSize, availableSizeOneImg);
        kfree(pNvramData);
        return -1;
    }

    *numPartitions = (curImgSize <= availableSizeTwoImgs &&
         newImgSize <= availableSizeTwoImgs &&
         flashPartition != nCurPartition) ? 2 : 1;

    // If the current image fits in half the flash space and the new
    // image to flash also fits in half the flash space, then flash it
    // in the partition that is not currently being used to boot from.
    if( curImgSize <= availableSizeTwoImgs &&
        newImgSize <= availableSizeTwoImgs &&
        ((nCurPartition == 1 && flashPartition != 1) || flashPartition == 2) )
    {
        // Update rootfsAddr to point to the second boot partition.
        int offset = (totalSize / 2) + TAG_LEN;

        sprintf(((PFILE_TAG) tagFs)->kernelAddress, "%lu",
            (unsigned long) IMAGE_BASE + offset + (kernelAddr - rootfsAddr));
        kernelAddr = baseAddr + offset + (kernelAddr - rootfsAddr);

        sprintf(((PFILE_TAG) tagFs)->rootfsAddress, "%lu",
            (unsigned long) IMAGE_BASE + offset);
        rootfsAddr = baseAddr + offset;
    }

    UpdateImageSequenceNumber( ((PFILE_TAG) tagFs)->imageSequence );
    crc = CRC32_INIT_VALUE;
    crc = getCrc32((unsigned char *)tagFs, (UINT32)TAG_LEN-TOKEN_LEN, crc);
    *(unsigned long *) &((PFILE_TAG) tagFs)->tagValidationToken[0] = crc;

    if( (status = kerSysBcmImageSet((rootfsAddr-TAG_LEN), tagFs,
        TAG_LEN + newImgSize, should_yield)) != 0 )
    {
        printk("Failed to flash root file system. Error: %d\n", status);
        kfree(pNvramData);
        return status;
    }

    kfree(pNvramData);
    return(status);
}

#define IMAGE_VERSION_FILE_NAME "/etc/image_version"
static int getImageVersion( int imageNumber, char *verStr, int verStrSize)
{
    int ret = 0; /* zero bytes copied to verStr so far */
    unsigned long rootfs_ofs;
    if( kerSysBlParmsGetInt(NAND_RFS_OFS_NAME, (int *) &rootfs_ofs) == -1 )
    {
        /* NOR Flash */
    PFILE_TAG pTag = NULL;

    if( imageNumber == 1 )
        pTag = getTagFromPartition(1);
    else
        if( imageNumber == 2 )
            pTag = getTagFromPartition(2);

    if( pTag )
    {
        if( verStrSize > sizeof(pTag->imageVersion) )
            ret = sizeof(pTag->imageVersion);
        else
            ret = verStrSize;

        memcpy(verStr, pTag->imageVersion, ret);
    }
    }
    else
    {
        /* NAND Flash */
        NVRAM_DATA *pNvramData;

        if( (pNvramData = readNvramData()) != NULL )
        {
            char *pImgVerFileName = NULL;

            mm_segment_t fs;
            struct file *fp;
            int updatePart, getFromCurPart;

            // updatePart is the partition number that is not booted
            // getFromCurPart is 1 to retrieive info from the booted partition
            updatePart = (rootfs_ofs==pNvramData->ulNandPartOfsKb[NP_ROOTFS_1])
                ? 2 : 1;
            getFromCurPart = (updatePart == imageNumber) ? 0 : 1;

            fs = get_fs();
            set_fs(get_ds());
            if( getFromCurPart == 0 )
            {
                pImgVerFileName = "/mnt/" IMAGE_VERSION_FILE_NAME;
                sys_mount("mtd:rootfs_update", "/mnt","jffs2",MS_RDONLY,NULL);
            }
            else
                pImgVerFileName = IMAGE_VERSION_FILE_NAME;

            fp = filp_open(pImgVerFileName, O_RDONLY, 0);
            if( !IS_ERR(fp) )
            {
                /* File open successful, read version string from file. */
                if(fp->f_op && fp->f_op->read)
                {
                    fp->f_pos = 0;
                    ret = fp->f_op->read(fp, (void *) verStr, verStrSize,
                        &fp->f_pos);
                    verStr[ret] = '\0';
                }
                filp_close(fp, NULL);
            }

            if( getFromCurPart == 0 )
                sys_umount("/mnt", 0);

            set_fs(fs);
            kfree(pNvramData);
        }
    }

    return( ret );
}

PFILE_TAG kerSysUpdateTagSequenceNumber(int imageNumber)
{
    PFILE_TAG pTag = NULL;
    UINT32 crc;

    switch( imageNumber )
    {
    case 0:
        pTag = getBootImageTag();
        break;

    case 1:
        pTag = getTagFromPartition(1);
        break;

    case 2:
        pTag = getTagFromPartition(2);
        break;

    default:
        break;
    }

    if( pTag )
    {
        UpdateImageSequenceNumber( pTag->imageSequence );
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
        *(unsigned long *) &pTag->tagValidationToken[0] = crc;
    }

    return(pTag);
}

int kerSysGetSequenceNumber(int imageNumber)
{
    int seqNumber = -1;
    unsigned long rootfs_ofs;
    if( kerSysBlParmsGetInt(NAND_RFS_OFS_NAME, (int *) &rootfs_ofs) == -1 )
    {
        /* NOR Flash */
    PFILE_TAG pTag = NULL;

    switch( imageNumber )
    {
    case 0:
        pTag = getBootImageTag();
        break;

    case 1:
        pTag = getTagFromPartition(1);
        break;

    case 2:
        pTag = getTagFromPartition(2);
        break;

    default:
        break;
    }

    if( pTag )
        seqNumber= simple_strtoul(pTag->imageSequence, NULL, 10);
    }
    else
    {
        /* NAND Flash */
        NVRAM_DATA *pNvramData;

        if( (pNvramData = readNvramData()) != NULL )
        {
            char fname[] = NAND_CFE_RAM_NAME;
            char cferam_buf[32], cferam_fmt[32]; 
            int i;

            mm_segment_t fs;
            struct file *fp;
            int updatePart, getFromCurPart;

            // updatePart is the partition number that is not booted
            // getFromCurPart is 1 to retrieive info from the booted partition
            updatePart = (rootfs_ofs==pNvramData->ulNandPartOfsKb[NP_ROOTFS_1])
                ? 2 : 1;
            getFromCurPart = (updatePart == imageNumber) ? 0 : 1;

            fs = get_fs();
            set_fs(get_ds());
            if( getFromCurPart == 0 )
            {
                strcpy(cferam_fmt, "/mnt/");
                sys_mount("mtd:rootfs_update", "/mnt","jffs2",MS_RDONLY,NULL);
            }
            else
                cferam_fmt[0] = '\0';

            /* Find the sequence number of the specified partition. */
            fname[strlen(fname) - 3] = '\0'; /* remove last three chars */
            strcat(cferam_fmt, fname);
            strcat(cferam_fmt, "%3.3d");

            for( i = 0; i < 999; i++ )
            {
                sprintf(cferam_buf, cferam_fmt, i);
                fp = filp_open(cferam_buf, O_RDONLY, 0);
                if (!IS_ERR(fp) )
                {
                    filp_close(fp, NULL);

                    /* Seqence number found. */
                    seqNumber = i;
                    break;
                }
            }

            if( getFromCurPart == 0 )
                sys_umount("/mnt", 0);

            set_fs(fs);
            kfree(pNvramData);
        }
    }

    return(seqNumber);
}

static int getBootedValue(int getBootedPartition)
{
    static int s_bootedPartition = -1;
    int ret = -1;
    int imgId = -1;

    kerSysBlParmsGetInt(BOOTED_IMAGE_ID_NAME, &imgId);

    /* The boot loader parameter will only be "new image", "old image" or "only
     * image" in order to be compatible with non-OMCI image update. If the
     * booted partition is requested, convert this boot type to partition type.
     */
    if( imgId != -1 )
    {
        if( getBootedPartition )
        {
            if( s_bootedPartition != -1 )
                ret = s_bootedPartition;
            else
            {
            /* Get booted partition. */
            int seq1 = kerSysGetSequenceNumber(1);
            int seq2 = kerSysGetSequenceNumber(2);

            switch( imgId )
            {
            case BOOTED_NEW_IMAGE:
                if( seq1 == -1 || seq2 > seq1 )
                    ret = BOOTED_PART2_IMAGE;
                else
                    if( seq2 == -1 || seq1 >= seq2 )
                        ret = BOOTED_PART1_IMAGE;
                break;

            case BOOTED_OLD_IMAGE:
                if( seq1 == -1 || seq2 < seq1 )
                    ret = BOOTED_PART2_IMAGE;
                else
                    if( seq2 == -1 || seq1 <= seq2 )
                        ret = BOOTED_PART1_IMAGE;
                break;

            case BOOTED_ONLY_IMAGE:
                ret = (seq1 == -1) ? BOOTED_PART2_IMAGE : BOOTED_PART1_IMAGE;
                break;

            default:
                break;
            }

                s_bootedPartition = ret;
            }
        }
        else
            ret = imgId;
    }

    return( ret );
}


#if !defined(CONFIG_BRCM_IKOS)
PFILE_TAG kerSysImageTagGet(void)
{
    PFILE_TAG tag;

    mutex_lock(&flashImageMutex);
    tag = getBootImageTag();
    mutex_unlock(&flashImageMutex);

    return tag;
}
#else
PFILE_TAG kerSysImageTagGet(void)
{
    return( (PFILE_TAG) (FLASH_BASE + FLASH_LENGTH_BOOT_ROM));
}
#endif

/*
 * Common function used by BCM_IMAGE_CFE and BCM_IMAGE_WHOLE ioctls.
 * This function will acquire the flashImageMutex
 *
 * @return 0 on success, -1 on failure.
 */
static int commonImageWrite(int flash_start_addr, char *string, int size,
    int *pnoReboot, int partition)
{
    NVRAM_DATA * pNvramDataOrig;
    NVRAM_DATA * pNvramDataNew=NULL;
    int ret;

    mutex_lock(&flashImageMutex);

    // Get a copy of the nvram before we do the image write operation
    if (NULL != (pNvramDataOrig = readNvramData()))
    {
        unsigned long rootfs_ofs;

        if( kerSysBlParmsGetInt(NAND_RFS_OFS_NAME, (int *) &rootfs_ofs) == -1 )
        {
            /* NOR flash */
        ret = kerSysBcmImageSet(flash_start_addr, string, size, 0);
        }
        else
        {
            /* NAND flash */
            char *rootfs_part = "rootfs_update";

            if( partition && rootfs_ofs == pNvramDataOrig->ulNandPartOfsKb[
                NP_ROOTFS_1 + partition - 1] )
            {
                /* The image to be flashed is the booted image. Force board
                 * reboot.
                 */
                rootfs_part = "rootfs";
                if( pnoReboot )
                    *pnoReboot = 0;
            }

            ret = kerSysBcmNandImageSet(rootfs_part, string, size,
                (pnoReboot) ? *pnoReboot : 0);
        }

        /*
         * After the image is written, check the nvram.
         * If nvram is bad, write back the original nvram.
         */
        pNvramDataNew = readNvramData();
        if ((0 != ret) ||
            (NULL == pNvramDataNew) ||
            (BpSetBoardId(pNvramDataNew->szBoardId) != BP_SUCCESS)
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
            || (BpSetVoiceBoardId(pNvramDataNew->szVoiceBoardId) != BP_SUCCESS)
#endif
            )
        {
            // we expect this path to be taken.  When a CFE or whole image
            // is written, it typically does not have a valid nvram block
            // in the image.  We detect that condition here and restore
            // the previous nvram settings.  Don't print out warning here.
            writeNvramDataCrcLocked(pNvramDataOrig);

            // don't modify ret, it is return value from kerSysBcmImageSet
        }
    }
    else
    {
        ret = -1;
    }

    mutex_unlock(&flashImageMutex);

    if (pNvramDataOrig)
        kfree(pNvramDataOrig);
    if (pNvramDataNew)
        kfree(pNvramDataNew);

    return ret;
}

struct file_operations monitor_fops;


//<< [CTFN-DRV-001-1] Antony.Wu : Support auto-detect ETHWAN (external PHY) feature, 20111222
//@@ return the number of external phy if any;
int (*gEthsw_get_extphy)(void) = NULL;
EXPORT_SYMBOL(gEthsw_get_extphy);
//>> [CTFN-DRV-001-1] End

//********************************************************************************************
// misc. ioctl calls come to here. (flash, led, reset, kernel memory access, etc.)
//********************************************************************************************
static int board_ioctl( struct inode *inode, struct file *flip,
                       unsigned int command, unsigned long arg )
{
    int ret = 0;
    BOARD_IOCTL_PARMS ctrlParms;
    unsigned char ucaMacAddr[NVRAM_MAC_ADDRESS_LEN];

    switch (command) {
    case BOARD_IOCTL_FLASH_WRITE:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {

            switch (ctrlParms.action) {
            case SCRATCH_PAD:
                if (ctrlParms.offset == -1)
                    ret =  kerSysScratchPadClearAll();
                else
                    ret = kerSysScratchPadSet(ctrlParms.string, ctrlParms.buf, ctrlParms.offset);
                break;

            case PERSISTENT:
                ret = kerSysPersistentSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case BACKUP_PSI:
                ret = kerSysBackupPsiSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case SYSLOG:
                ret = kerSysSyslogSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case NVRAM:
            {
                NVRAM_DATA * pNvramData;

                /*
                 * Note: even though NVRAM access is protected by
                 * flashImageMutex at the kernel level, this protection will
                 * not work if two userspaces processes use ioctls to get
                 * NVRAM data, modify it, and then use this ioctl to write
                 * NVRAM data.  This seems like an unlikely scenario.
                 */
                mutex_lock(&flashImageMutex);
                if (NULL == (pNvramData = readNvramData()))
                {
                    mutex_unlock(&flashImageMutex);
                    return -ENOMEM;
                }
                if ( !strncmp(ctrlParms.string, "WLANDATA", 8 ) ) { //Wlan Data data
                    memset((char *)pNvramData + ((size_t) &((NVRAM_DATA *)0)->wlanParams),
                        0, sizeof(pNvramData->wlanParams) );
                    memcpy( (char *)pNvramData + ((size_t) &((NVRAM_DATA *)0)->wlanParams),
                        ctrlParms.string+8,
                        ctrlParms.strLen-8);
                    writeNvramDataCrcLocked(pNvramData);
                }
                else {
                    // assumes the user has calculated the crc in the nvram struct
                    ret = kerSysNvRamSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                }
                mutex_unlock(&flashImageMutex);
                kfree(pNvramData);
                break;
            }

            case BCM_IMAGE_CFE:
                if( ctrlParms.strLen <= 0 || ctrlParms.strLen > FLASH_LENGTH_BOOT_ROM )
                {
                    printk("Illegal CFE size [%d]. Size allowed: [%d]\n",
                        ctrlParms.strLen, FLASH_LENGTH_BOOT_ROM);
                    ret = -1;
                    break;
                }

                ret = commonImageWrite(ctrlParms.offset + BOOT_OFFSET,
                    ctrlParms.string, ctrlParms.strLen, NULL, 0);

                break;

            case BCM_IMAGE_FS:
                {
                int numPartitions = 1;
                int noReboot = FLASH_IS_NO_REBOOT(ctrlParms.offset);
                int partition = FLASH_GET_PARTITION(ctrlParms.offset);

                mutex_lock(&flashImageMutex);
                ret = flashFsKernelImage(ctrlParms.string, ctrlParms.strLen,
                    partition, &numPartitions);
                mutex_unlock(&flashImageMutex);

                if(ret == 0 && (numPartitions == 1 || noReboot == 0))
                    resetPwrmgmtDdrMips();
                }
                break;

            case BCM_IMAGE_KERNEL:  // not used for now.
                break;

            case BCM_IMAGE_WHOLE:
                {
                int noReboot = FLASH_IS_NO_REBOOT(ctrlParms.offset);
                int partition = FLASH_GET_PARTITION(ctrlParms.offset);

                if(ctrlParms.strLen <= 0)
                {
                    printk("Illegal flash image size [%d].\n", ctrlParms.strLen);
                    ret = -1;
                    break;
                }

                ret = commonImageWrite(FLASH_BASE, ctrlParms.string,
                    ctrlParms.strLen, &noReboot, partition );

                if(ret == 0 && noReboot == 0)
                {
                    resetPwrmgmtDdrMips();
                }
                else
                {
                    if (ret != 0)
                    printk("flash of whole image failed, ret=%d\n", ret);
                }
                }
                break;

            default:
                ret = -EINVAL;
                printk("flash_ioctl_command: invalid command %d\n", ctrlParms.action);
                break;
            }
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_FLASH_READ:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            switch (ctrlParms.action) {
//<< [CTFN-SYS-024] Arius Su: Support ROM & Whole Flash Image Backup from GUI for NOR Flash, 2012/02/15
            case FLASH_BLOCK:
                ret = kerSysFlashBlockGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;
            case LOCK_FLASH:				
                ret = kerSysGetFlashLock();
                break;
            case UNLOCK_FLASH:			
                ret = kerSysFreeFlashLock();
                break;
//>> [CTFN-SYS-024] End

// << [CTFN-SYS-024-1] jojopo : Support Flash Image Backup from GUI for NAND Flash , 2012/05/30
            case DUMP_NAND_FLASH_DATA:
                ret = kerSysDumpNANDFlashData(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;
            case READ_NAND_FLASH_PAGE:
                ret = kerSysReadNANDFlashPage(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;
            case SHOW_NAND_FLASH_BBT:
                ret = kerSysShowNANDFlashBBT();
                break;
            case GET_NAND_FLASH_BADBLOCK_NUMBER:
                ret = kerSysGetNANDFlashBadBlockNumber();
                break;
            case GET_NAND_FLASH_BADBLOCK:
                ret = kerSysGetNANDFlashBadBlock(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;
            case GET_NAND_FLASH_TOTALSIZE:
                ret = kerSysGetNANDFlashTotalSize();
                break;
            case GET_NAND_FLASH_BLOCKSIZE:
                ret = kerSysGetNANDFlashBlockSize();
                break;
            case GET_NAND_FLASH_PAGESIZE:
                ret = kerSysGetNANDFlashPageSize();
                break;
            case GET_NAND_FLASH_OOBSIZE:
                ret = kerSysGetNANDFlashOobSize();
                break;
// >> [CTFN-SYS-024-1] end
            case SCRATCH_PAD:
                ret = kerSysScratchPadGet(ctrlParms.string, ctrlParms.buf, ctrlParms.offset);
                break;

            case PERSISTENT:
                ret = kerSysPersistentGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case BACKUP_PSI:
                ret = kerSysBackupPsiGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case SYSLOG:
                ret = kerSysSyslogGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case NVRAM:
                kerSysNvRamGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                ret = 0;
                break;

            case FLASH_SIZE:
                ret = kerSysFlashSizeGet();
                break;

            default:
                ret = -EINVAL;
                printk("Not supported.  invalid command %d\n", ctrlParms.action);
                break;
            }
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_FLASH_LIST:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            switch (ctrlParms.action) {
            case SCRATCH_PAD:
                ret = kerSysScratchPadList(ctrlParms.buf, ctrlParms.offset);
                break;

            default:
                ret = -EINVAL;
                printk("Not supported.  invalid command %d\n", ctrlParms.action);
                break;
            }
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_DUMP_ADDR:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            dumpaddr( (unsigned char *) ctrlParms.string, ctrlParms.strLen );
            ctrlParms.result = 0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
    case BOARD_IOCTL_SET_INTF:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            struct trafficInfo *info, *ptr;
//	     INET_LED_REG_PARMS *regInfo;

            //if (ctrlParms.strLen >= IFNAMSIZ)
            if (((INET_LED_REG_PARMS *)(ctrlParms.buf))->l3IfNameLen >= IFNAMSIZ)
            {
                printk("Register interface %s !!\nInterface name too long!!\n", ctrlParms.string);
                ret = -EFAULT;
            }

//<< Lucien Huang : Support Internet LED, 2011/10/11
            if(((INET_LED_REG_PARMS *)(ctrlParms.buf))->isEdit)
            {
            // do nothing. No need to plus the value of XdslWanType, EthWanType, ThriGWanType when user edit WAN Service.
            }
            else
            {
                switch(((INET_LED_REG_PARMS *)(ctrlParms.buf))->wanType)
                {
                    case XDSL_WAN_TYPE:
                        ++XdslWanType;
                        break;
                    case ETH_WAN_TYPE:
                        ++EthWanType;
                        break;
                    case THIR_G_WAN_TYPE:
                        ++ThriGWanType;
                        break;
                    default:
                        return -EFAULT;
                }
	     }
//>> Lucien Huang : End
            info = (struct trafficInfo *)kmalloc(sizeof(struct trafficInfo), GFP_ATOMIC);
            memset(info, 0, sizeof(struct trafficInfo));
            memcpy(info->dev_name, ((INET_LED_REG_PARMS *)(ctrlParms.buf))->l3IfName, ((INET_LED_REG_PARMS *)(ctrlParms.buf))->l3IfNameLen);
            memcpy(info->dev_name_l2, ((INET_LED_REG_PARMS *)(ctrlParms.buf))->l2IfName, ((INET_LED_REG_PARMS *)(ctrlParms.buf))->l2IfNameLen);
            info->wanType = ((INET_LED_REG_PARMS *)(ctrlParms.buf))->wanType;

            printk("Register interface %s !!\n", info->dev_name);

            down(&tarfficInfo_lock);

            if (head)
            {
                for (ptr = head; ptr->next; ptr = ptr->next)
                    ;

                ptr->next = info;
                info->priv = ptr;
            }
            else
                head = info;

            up(&tarfficInfo_lock);

            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_REL_INTF:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            struct trafficInfo *next, *ptr;
            char buf[IFNAMSIZ] = {0};

            copy_from_user((void*)buf, (void*)ctrlParms.string, ctrlParms.strLen);

            if (ctrlParms.strLen >= IFNAMSIZ)
            {
                printk("Unregister interface %s fail !!\nInterface name too long!!\n", buf);
                ret = -EFAULT;
            }

            down(&tarfficInfo_lock);

            for (ptr = head; ptr; ptr = ptr->next)
            {
                if (strncmp(ptr->dev_name, buf, ctrlParms.strLen) == 0)
                    break;
            }

            if (ptr)
            {
                printk("Unregister interface %s !!\n", ptr->dev_name);
                next = ptr->next;

                switch(ptr->wanType)
                {
                    case XDSL_WAN_TYPE:
                        --XdslWanType;
                        break;
                    case ETH_WAN_TYPE:
                        --EthWanType;
                        break;
                    //case THRI_G_WAN_TYPE:
                    case THIR_G_WAN_TYPE:
                        --ThriGWanType;
                        break;
                    default:
                        return -EFAULT;
                }

                if (ptr->priv == NULL)
                    head = ptr->next;
                else
                    ptr->priv->next = ptr->next;

                if (ptr->next != NULL)
                    ptr->next->priv = ptr->priv;

                kfree(ptr);
                ret = 0;
            }
            else
            {
                printk("Unregister interface %s fail !!\nInterface not find in database !!\n", buf);
                ret = -EFAULT;
            }

            up(&tarfficInfo_lock);
        }
        else
            ret = -EFAULT;
        break;
//>> [CTFN-SYS-016] End

//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
    case BOARD_IOCTL_SET_WANCONNSTATUS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
           wanConnStatus = ((WAN_CONN_STATUS_REG_PARMS *)(ctrlParms.buf))->wanConnStatus;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_SET_WANIPV6CONNSTATUS:
	if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
	{
	   wanIPv6ConnStatus = ((WAN_IPV6_CONN_STATUS_REG_PARMS *)(ctrlParms.buf))->wanIPv6ConnStatus;
	}
	else
		ret = -EFAULT;
	break;
//>> [CTFN-SYS-016-2] End

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
    case BOARD_IOCTL_SET_USBLED:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
        	usbLedStatus = ((USB_LED_REG_PARMS *)(ctrlParms.buf))->usbLedStatus;
        }
        else
            ret = -EFAULT;
        break;
//>> [JAZ-026] End

    case BOARD_IOCTL_SET_MEMORY:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            unsigned long  *pul = (unsigned long *)  ctrlParms.string;
            unsigned short *pus = (unsigned short *) ctrlParms.string;
            unsigned char  *puc = (unsigned char *)  ctrlParms.string;
            switch( ctrlParms.strLen ) {
            case 4:
                *pul = (unsigned long) ctrlParms.offset;
                break;
            case 2:
                *pus = (unsigned short) ctrlParms.offset;
                break;
            case 1:
                *puc = (unsigned char) ctrlParms.offset;
                break;
            }
#if !defined(CONFIG_BCM96816)
            /* This is placed as MoCA blocks are 32-bit only
            * accessible and following call makes access in terms
            * of bytes. Probably MoCA address range can be checked
            * here.
            */
            dumpaddr( (unsigned char *) ctrlParms.string, sizeof(long) );
#endif
            ctrlParms.result = 0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_MIPS_SOFT_RESET:
        kerSysMipsSoftReset();
        break;

    case BOARD_IOCTL_LED_CTRL:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            kerSysLedCtrl((BOARD_LED_NAME)ctrlParms.strLen, (BOARD_LED_STATE)ctrlParms.offset);
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_ID:
        if (copy_from_user((void*)&ctrlParms, (void*)arg,
            sizeof(ctrlParms)) == 0)
        {
            if( ctrlParms.string )
            {
                char p[NVRAM_BOARD_ID_STRING_LEN];
                kerSysNvRamGetBoardId(p);
                if( strlen(p) + 1 < ctrlParms.strLen )
                    ctrlParms.strLen = strlen(p) + 1;
                __copy_to_user(ctrlParms.string, p, ctrlParms.strLen);
            }

            ctrlParms.result = 0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
        }
        break;

    case BOARD_IOCTL_GET_MAC_ADDRESS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            ctrlParms.result = kerSysGetMacAddress( ucaMacAddr,
                ctrlParms.offset );

            if( ctrlParms.result == 0 )
            {
                __copy_to_user(ctrlParms.string, ucaMacAddr,
                    sizeof(ucaMacAddr));
            }

            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_RELEASE_MAC_ADDRESS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            if (copy_from_user((void*)ucaMacAddr, (void*)ctrlParms.string, \
                NVRAM_MAC_ADDRESS_LEN) == 0)
            {
                ctrlParms.result = kerSysReleaseMacAddress( ucaMacAddr );
            }
            else
            {
                ctrlParms.result = -EACCES;
            }

            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_GET_PSI_SIZE:
        {
            FLASH_ADDR_INFO fInfo;
            kerSysFlashAddrInfoGet(&fInfo);
            ctrlParms.result = fInfo.flash_persistent_length;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_BACKUP_PSI_SIZE:
        {
            FLASH_ADDR_INFO fInfo;
            kerSysFlashAddrInfoGet(&fInfo);
            // if number_blks > 0, that means there is a backup psi, but length is the same
            // as the primary psi (persistent).

            ctrlParms.result = (fInfo.flash_backup_psi_number_blk > 0) ?
                fInfo.flash_persistent_length : 0;
            printk("backup_psi_number_blk=%d result=%d\n", fInfo.flash_backup_psi_number_blk, fInfo.flash_persistent_length);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_SYSLOG_SIZE:
        {
            FLASH_ADDR_INFO fInfo;
            kerSysFlashAddrInfoGet(&fInfo);
            ctrlParms.result = fInfo.flash_syslog_length;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

//<< [CTFN-SYS-024] Arius Su: Support ROM & Whole Flash Image Backup from GUI for NOR Flash, 2012/02/15
    case BOARD_IOCTL_GET_BLOCK_COUNT:
        {
        	ctrlParms.result = (int) flash_get_numsectors();
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_BLOCK_SIZE:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
        	ctrlParms.result = (int) flash_get_sector_size(ctrlParms.offset);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;
//>> [CTFN-SYS-024] End

    case BOARD_IOCTL_GET_SDRAM_SIZE:
        ctrlParms.result = (int) g_ulSdramSize;
        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;

    case BOARD_IOCTL_GET_BASE_MAC_ADDRESS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            __copy_to_user(ctrlParms.string, g_pMacInfo->ucaBaseMacAddr, NVRAM_MAC_ADDRESS_LEN);
            ctrlParms.result = 0;

            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_GET_CHIP_ID:
        ctrlParms.result = kerSysGetChipId();


        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;

    case BOARD_IOCTL_GET_CHIP_REV:
        ctrlParms.result = (int) (PERF->RevID & REV_ID_MASK);
        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;

//<< [CTFN-SYS-024] Arius Su: Support ROM & Whole Flash Image Backup from GUI for NOR Flash, 2012/02/15
    case BOARD_IOCTL_GET_FLASH_TYPE:
        ctrlParms.result = (int)flash_get_flash_type();
        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;
//>> [CTFN-SYS-024] End

    case BOARD_IOCTL_GET_NUM_ENET_MACS:
    case BOARD_IOCTL_GET_NUM_ENET_PORTS:
        {
            ETHERNET_MAC_INFO EnetInfos[BP_MAX_ENET_MACS];
            int i, cnt, numEthPorts = 0;
            if (BpGetEthernetMacInfo(EnetInfos, BP_MAX_ENET_MACS) == BP_SUCCESS) {
                for( i = 0; i < BP_MAX_ENET_MACS; i++) {
                    if (EnetInfos[i].ucPhyType != BP_ENET_NO_PHY) {
                        bitcount(cnt, EnetInfos[i].sw.port_map);
                        numEthPorts += cnt;
                    }
                }
                ctrlParms.result = numEthPorts;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,  sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            }
            else {
                ret = -EFAULT;
            }
            break;
        }

    case BOARD_IOCTL_GET_CFE_VER:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            char vertag[CFE_VERSION_MARK_SIZE+CFE_VERSION_SIZE];
            kerSysCfeVersionGet(vertag, sizeof(vertag));
            if (ctrlParms.strLen < CFE_VERSION_SIZE) {
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = -EFAULT;
            }
            else if (strncmp(vertag, "cfe-v", 5)) { // no tag info in flash
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            }
            else {
                ctrlParms.result = 1;
                __copy_to_user(ctrlParms.string, vertag+CFE_VERSION_MARK_SIZE, CFE_VERSION_SIZE);
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            }
        }
        else {
            ret = -EFAULT;
        }
        break;

#if defined (WIRELESS)
    case BOARD_IOCTL_GET_WLAN_ANT_INUSE:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            unsigned short antInUse = 0;
            if (BpGetWirelessAntInUse(&antInUse) == BP_SUCCESS) {
                if (ctrlParms.strLen == sizeof(antInUse)) {
                    __copy_to_user(ctrlParms.string, &antInUse, sizeof(antInUse));
                    ctrlParms.result = 0;
                    __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                    ret = 0;
                } else
                    ret = -EFAULT;
            }
            else {
                ret = -EFAULT;
            }
            break;
        }
        else {
            ret = -EFAULT;
        }
        break;
#endif
    case BOARD_IOCTL_SET_TRIGGER_EVENT:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            BOARD_IOC *board_ioc = (BOARD_IOC *)flip->private_data;
            ctrlParms.result = -EFAULT;
            ret = -EFAULT;
            if (ctrlParms.strLen == sizeof(unsigned long)) {
                board_ioc->eventmask |= *((int*)ctrlParms.string);
#if defined (WIRELESS)
                if((board_ioc->eventmask & SES_EVENTS)) {
                    if(sesBtn_irq != BP_NOT_DEFINED) {
                        BcmHalInterruptEnable(sesBtn_irq);
                        ctrlParms.result = 0;
                        ret = 0;
                    }
                }
#endif
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            }
            break;
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_GET_TRIGGER_EVENT:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            BOARD_IOC *board_ioc = (BOARD_IOC *)flip->private_data;
            if (ctrlParms.strLen == sizeof(unsigned long)) {
                __copy_to_user(ctrlParms.string, &board_ioc->eventmask, sizeof(unsigned long));
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            } else
                ret = -EFAULT;

            break;
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_UNSET_TRIGGER_EVENT:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            if (ctrlParms.strLen == sizeof(unsigned long)) {
                BOARD_IOC *board_ioc = (BOARD_IOC *)flip->private_data;
                board_ioc->eventmask &= (~(*((int*)ctrlParms.string)));
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            } else
                ret = -EFAULT;

            break;
        }
        else {
            ret = -EFAULT;
        }
        break;
#if defined (WIRELESS)
    case BOARD_IOCTL_SET_SES_LED:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            if (ctrlParms.strLen == sizeof(int)) {
                sesLed_ctrl(*(int*)ctrlParms.string);
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            } else
                ret = -EFAULT;

            break;
        }
        else {
            ret = -EFAULT;
        }
        break;
#endif

    case BOARD_IOCTL_SET_MONITOR_FD:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {

           g_monitor_nl_pid =  ctrlParms.offset;
           printk(KERN_INFO "monitor task is initialized pid= %d \n",g_monitor_nl_pid);
        }
        break;

    case BOARD_IOCTL_WAKEUP_MONITOR_TASK:
        kerSysSendtoMonitorTask(MSG_NETLINK_BRCM_WAKEUP_MONITOR_TASK, NULL, 0);
        break;

    case BOARD_IOCTL_SET_CS_PAR:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            ret = ConfigCs(&ctrlParms);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_SET_GPIO:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            kerSysSetGpioState(ctrlParms.strLen, ctrlParms.offset);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else {
            ret = -EFAULT;
        }
        break;

#if defined(CONFIG_BCM_CPLD1)
    case BOARD_IOCTL_SET_SHUTDOWN_MODE:
        BcmCpld1SetShutdownMode();
        ret = 0;
        break;

    case BOARD_IOCTL_SET_STANDBY_TIMER:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            BcmCpld1SetStandbyTimer(ctrlParms.offset);
            ret = 0;
        }
        else {
            ret = -EFAULT;
        }
        break;
#endif

//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
#ifdef CTCONFIG_3G_FEATURE
    case BOARD_IOCTL_GET_USB_DONGLE_ID:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            ctrlParms.strLen = strlen(get_usb_id());
            __copy_to_user(ctrlParms.string, get_usb_id(), ctrlParms.strLen);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_SET_USB_DONGLE_STATE:
#if 0
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            //printk("in BOARD_IOCTL_SET_USB_DONGLE_STATE : %s\n" , ctrlParms.string);
            //printk("call BOARD_IOCTL_SET_USB_DONGLE_STATE\n");
            ThriGIntfState = ctrlParms.offset;
        }
        else
            ret = -EFAULT;
#endif
        break;
#endif
//>> [CTFN-3G-001] End

    case BOARD_IOCTL_BOOT_IMAGE_OPERATION:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            switch(ctrlParms.offset)
            {
            case BOOT_SET_PART1_IMAGE:
            case BOOT_SET_PART2_IMAGE:
            case BOOT_SET_PART1_IMAGE_ONCE:
            case BOOT_SET_PART2_IMAGE_ONCE:
            case BOOT_SET_OLD_IMAGE:
            case BOOT_SET_NEW_IMAGE:
            case BOOT_SET_NEW_IMAGE_ONCE:
                ctrlParms.result = kerSysSetBootImageState(ctrlParms.offset);
                break;

            case BOOT_GET_BOOT_IMAGE_STATE:
                ctrlParms.result = kerSysGetBootImageState();
                break;

            case BOOT_GET_IMAGE_VERSION:
                /* ctrlParms.action is parition number */
                ctrlParms.result = getImageVersion((int) ctrlParms.action,
                    ctrlParms.string, ctrlParms.strLen);
                break;

            case BOOT_GET_BOOTED_IMAGE_ID:
                /* ctrlParm.strLen == 1: partition or == 0: id (new or old) */
                ctrlParms.result = getBootedValue(ctrlParms.strLen);
                break;

            default:
                ctrlParms.result = -EFAULT;
                break;
            }
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_GET_TIMEMS:
        ret = jiffies_to_msecs(jiffies - INITIAL_JIFFIES);
        break;

    case BOARD_IOCTL_GET_DEFAULT_OPTICAL_PARAMS:
    {
        unsigned char ucDefaultOpticalParams[BP_OPTICAL_PARAMS_LEN];
            
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            ret = 0;
            if (BP_SUCCESS == (ctrlParms.result = BpGetDefaultOpticalParams(ucDefaultOpticalParams)))
            {
                __copy_to_user(ctrlParms.string, ucDefaultOpticalParams, BP_OPTICAL_PARAMS_LEN);

                if (__copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS)) != 0)
                {
                    ret = -EFAULT;
                }
            }                        
        }
        else
        {
            ret = -EFAULT;
        }

        break;
    }
    
    break;
    case BOARD_IOCTL_GET_GPON_OPTICS_TYPE:
     
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) 
        {
            unsigned short Temp=0;
            BpGetGponOpticsType(&Temp);
            *((UINT32*)ctrlParms.buf) = Temp;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }        
        ret = 0;

        break;

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM963268)
    case BOARD_IOCTL_SPI_SLAVE_INIT:  
        ret = 0;
        if (kerSysBcmSpiSlaveInit() != SPI_STATUS_OK)  
        {
            ret = -EFAULT;
        }        
        break;   
        
    case BOARD_IOCTL_SPI_SLAVE_READ:  
        ret = 0;
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
             if (kerSysBcmSpiSlaveRead(ctrlParms.offset, (unsigned long *)ctrlParms.buf, ctrlParms.strLen) != SPI_STATUS_OK)  
             {
                 ret = -EFAULT;
             } 
             else
             {
                   __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));    
             }
        }
        else
        {
            ret = -EFAULT;
        }                 
        break;    
        
    case BOARD_IOCTL_SPI_SLAVE_WRITE:  
        ret = 0;
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
             if (kerSysBcmSpiSlaveWrite(ctrlParms.offset, ctrlParms.result, ctrlParms.strLen) != SPI_STATUS_OK)  
             {
                 ret = -EFAULT;
             } 
             else
             {
                   __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));    
             }
        }
        else
        {
            ret = -EFAULT;
        }                 
        break;    
#endif

        
//<< [CTFN-SYS-016] MHTsai : Support Internet LED for ip extension, 2010/10/14
    case BOARD_IOCTL_SET_INT_LED:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            strcpy(intLedStatus , ctrlParms.string);
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;
//>> [CTFN-SYS-016] End

//<< [CTFN-DRV-001-1] Antony.Wu : Support auto-detect ETHWAN (external PHY) feature, 20111222
    case BOARD_IOCTL_GET_EXTPHY_STATUS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            ctrlParms.result = (gEthsw_get_extphy)?gEthsw_get_extphy():0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;
//>> [CTFN-DRV-001-1] End

//<< [CTFN-SYS-038] Lain : Support watchdog feature, 2012/09/28
#ifdef CTCONFIG_SYS_ENABLE_WATCH_DOG
    case BOARD_IOCTL_SET_WATCHDOG_TIME:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            int watch_time = 0;
            watch_time = ctrlParms.strLen;
            TIMER->WatchDogDefCount = watch_time * 1000000 * (FPERIPH/1000000);
            TIMER->WatchDogCtl = 0xFF00;
            TIMER->WatchDogCtl = 0x00FF;
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;
    case BOARD_IOCTL_CLOSE_WATCHDOG_TIME:
        {
            int watch_time;
            watch_time = 1000000;
            TIMER->WatchDogDefCount = watch_time * 1000000 * (FPERIPH/1000000);
            TIMER->WatchDogCtl = 0xEE00;
            TIMER->WatchDogCtl = 0x00EE;
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        break;    
#endif
//>> [CTFN-SYS-038] End

//<< [JAZ-TR69-004] David: Several TR-069 bug fixes and customizations, 2012/02/06
    case BOARD_IOCTL_GET_IMAGE_SEQUENCE:
        {
//<< jojopo : [JAZ-TR69-004-3] Event "7 Transfer Complete" not sent after TR-069 firmware download" for the model with NAND flash (TTS #3100) , 2012/08/31
#ifdef BRCM_NAND_FLASH
            int imageSeq = kerSysGetCferamSequence();
#else
            PFILE_TAG pTag = kerSysImageTagGet();
            int imageSeq;

            if (pTag)
                imageSeq = simple_strtoul(pTag->imageSequence, NULL, 10);
            else
                imageSeq = 0;

#endif
//>> jojopo : end
            ctrlParms.result = imageSeq;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;
//>> [JAZ-TR69-004] End

    default:
        ret = -EINVAL;
        ctrlParms.result = 0;
        printk("board_ioctl: invalid command %x, cmd %d .\n",command,_IOC_NR(command));
        break;

    } /* switch */

    return (ret);

} /* board_ioctl */

/***************************************************************************
* SES Button ISR/GPIO/LED functions.
***************************************************************************/
#if defined (WIRELESS)

static Bool sesBtn_pressed(void)
{
    if ((sesBtn_irq >= INTERRUPT_ID_EXTERNAL_0) && (sesBtn_irq <= INTERRUPT_ID_EXTERNAL_3)) {
        if (!(PERF->ExtIrqCfg & (1 << (sesBtn_irq - INTERRUPT_ID_EXTERNAL_0 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    else if ((sesBtn_irq >= INTERRUPT_ID_EXTERNAL_4) || (sesBtn_irq <= INTERRUPT_ID_EXTERNAL_5)) {
        if (!(PERF->ExtIrqCfg1 & (1 << (sesBtn_irq - INTERRUPT_ID_EXTERNAL_4 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#endif
    return 0;
}

static irqreturn_t sesBtn_isr(int irq, void *dev_id)
{
    if (sesBtn_pressed()){
        wake_up_interruptible(&g_board_wait_queue);
        return IRQ_RETVAL(1);
    } else {
        return IRQ_RETVAL(0);
    }
}

static void __init sesBtn_mapIntr(int context)
{
    if( BpGetWirelessSesExtIntr(&sesBtn_irq) == BP_SUCCESS )
    {
        printk("SES: Button Interrupt 0x%x is enabled\n", sesBtn_irq);
    }
    else
        return;

    sesBtn_irq = map_external_irq (sesBtn_irq) ;

    if (BcmHalMapInterrupt((FN_HANDLER)sesBtn_isr, context, sesBtn_irq)) {
        printk("SES: Interrupt mapping failed\n");
    }
    BcmHalInterruptEnable(sesBtn_irq);
}


static unsigned int sesBtn_poll(struct file *file, struct poll_table_struct *wait)
{

    if (sesBtn_pressed()){
        return POLLIN;
    }
    return 0;
}

static ssize_t sesBtn_read(struct file *file,  char __user *buffer, size_t count, loff_t *ppos)
{
    volatile unsigned int event=0;
    ssize_t ret=0;

    if(!sesBtn_pressed()){
        BcmHalInterruptEnable(sesBtn_irq);
        return ret;
    }
    event = SES_EVENTS;
    __copy_to_user((char*)buffer, (char*)&event, sizeof(event));
    BcmHalInterruptEnable(sesBtn_irq);
    count -= sizeof(event);
    buffer += sizeof(event);
    ret += sizeof(event);
    return ret;
}

static void __init sesLed_mapGpio()
{
    if( BpGetWirelessSesLedGpio(&sesLed_gpio) == BP_SUCCESS )
    {
        printk("SES: LED GPIO 0x%x is enabled\n", sesLed_gpio);
    }
}

static void sesLed_ctrl(int action)
{
    char blinktype = ((action >> 24) & 0xff); /* extract blink type for SES_LED_BLINK  */

    BOARD_LED_STATE led;

    if(sesLed_gpio == BP_NOT_DEFINED)
        return;

    action &= 0xff; /* extract led */

    switch (action) {
    case SES_LED_ON:
        led = kLedStateOn;
        break;
    case SES_LED_BLINK:
        if(blinktype)
            led = blinktype;
        else
            led = kLedStateSlowBlinkContinues;           		
        break;
    case SES_LED_OFF:
    default:
        led = kLedStateOff;
    }

    kerSysLedCtrl(kLedSes, led);
}

static void __init ses_board_init()
{
    sesBtn_mapIntr(0);
    sesLed_mapGpio();
}
static void __exit ses_board_deinit()
{
    if(sesBtn_irq)
        BcmHalInterruptDisable(sesBtn_irq);
}
#endif

/***************************************************************************
* Dying gasp ISR and functions.
***************************************************************************/

//<< [CTFN-SYS-023] Jimmy Wu : Add macro definition to disable dying gasp
#ifndef CTCONFIG_SYS_DISABLE_DYING_GASP
static irqreturn_t kerSysDyingGaspIsr(int irq, void * dev_id)
{
    struct list_head *pos;
    CB_DGASP_LIST *tmp = NULL, *dslOrGpon = NULL;
	unsigned short usPassDyingGaspGpio;		// The GPIO pin to propogate a dying gasp signal

    UART->Data = 'D';
    UART->Data = '%';
    UART->Data = 'G';

#if defined (WIRELESS)
    kerSetWirelessPD(WLAN_OFF);
#endif
    /* first to turn off everything other than dsl or gpon */
    list_for_each(pos, &g_cb_dgasp_list_head->list) {
        tmp = list_entry(pos, CB_DGASP_LIST, list);
        if(strncmp(tmp->name, "dsl", 3) && strncmp(tmp->name, "gpon", 4)) {
            (tmp->cb_dgasp_fn)(tmp->context);
        }else {
            dslOrGpon = tmp;
        }
    }
	
    // Invoke dying gasp handlers
    if(dslOrGpon)
        (dslOrGpon->cb_dgasp_fn)(dslOrGpon->context);

    /* reset and shutdown system */

    /* Set WD to fire in 1 sec in case power is restored before reset occurs */
    TIMER->WatchDogDefCount = 1000000 * (FPERIPH/1000000);
    TIMER->WatchDogCtl = 0xFF00;
    TIMER->WatchDogCtl = 0x00FF;

	// If configured, propogate dying gasp to other processors on the board
	if(BpGetPassDyingGaspGpio(&usPassDyingGaspGpio) == BP_SUCCESS)
	    {
	    // Dying gasp configured - set GPIO
	    kerSysSetGpioState(usPassDyingGaspGpio, kGpioInactive);
	    }

    // If power is going down, nothing should continue!
    while (1);
    return( IRQ_HANDLED );
}

static void __init kerSysInitDyingGaspHandler( void )
{
    CB_DGASP_LIST *new_node;

    if( g_cb_dgasp_list_head != NULL) {
        printk("Error: kerSysInitDyingGaspHandler: list head is not null\n");
        return;
    }
    new_node= (CB_DGASP_LIST *)kmalloc(sizeof(CB_DGASP_LIST), GFP_KERNEL);
    memset(new_node, 0x00, sizeof(CB_DGASP_LIST));
    INIT_LIST_HEAD(&new_node->list);
    g_cb_dgasp_list_head = new_node;

    BcmHalMapInterrupt((FN_HANDLER)kerSysDyingGaspIsr, 0, INTERRUPT_ID_DG);
    BcmHalInterruptEnable( INTERRUPT_ID_DG );
} /* kerSysInitDyingGaspHandler */
#endif /* CTCONFIG_SYS_DISABLE_DYING_GASP */
//>> [CTFN-SYS-023] End

static void __exit kerSysDeinitDyingGaspHandler( void )
{
    struct list_head *pos;
    CB_DGASP_LIST *tmp;

    if(g_cb_dgasp_list_head == NULL)
        return;

    list_for_each(pos, &g_cb_dgasp_list_head->list) {
        tmp = list_entry(pos, CB_DGASP_LIST, list);
        list_del(pos);
        kfree(tmp);
    }

    kfree(g_cb_dgasp_list_head);
    g_cb_dgasp_list_head = NULL;

} /* kerSysDeinitDyingGaspHandler */

void kerSysRegisterDyingGaspHandler(char *devname, void *cbfn, void *context)
{
    CB_DGASP_LIST *new_node;

    // do all the stuff that can be done without the lock first
    if( devname == NULL || cbfn == NULL ) {
        printk("Error: kerSysRegisterDyingGaspHandler: register info not enough (%s,%x,%x)\n", devname, (unsigned int)cbfn, (unsigned int)context);
        return;
    }

    if (strlen(devname) > (IFNAMSIZ - 1)) {
        printk("Warning: kerSysRegisterDyingGaspHandler: devname too long, will be truncated\n");
    }

    new_node= (CB_DGASP_LIST *)kmalloc(sizeof(CB_DGASP_LIST), GFP_KERNEL);
    memset(new_node, 0x00, sizeof(CB_DGASP_LIST));
    INIT_LIST_HEAD(&new_node->list);
    strncpy(new_node->name, devname, IFNAMSIZ-1);
    new_node->cb_dgasp_fn = (cb_dgasp_t)cbfn;
    new_node->context = context;

    // OK, now acquire the lock and insert into list
    mutex_lock(&dgaspMutex);
    if( g_cb_dgasp_list_head == NULL) {
        printk("Error: kerSysRegisterDyingGaspHandler: list head is null\n");
        kfree(new_node);
    } else {
        list_add(&new_node->list, &g_cb_dgasp_list_head->list);
        printk("dgasp: kerSysRegisterDyingGaspHandler: %s registered \n", devname);
    }
    mutex_unlock(&dgaspMutex);

    return;
} /* kerSysRegisterDyingGaspHandler */

void kerSysDeregisterDyingGaspHandler(char *devname)
{
    struct list_head *pos;
    CB_DGASP_LIST *tmp;
    int found=0;

    if(devname == NULL) {
        printk("Error: kerSysDeregisterDyingGaspHandler: devname is null\n");
        return;
    }

    printk("kerSysDeregisterDyingGaspHandler: %s is deregistering\n", devname);

    mutex_lock(&dgaspMutex);
    if(g_cb_dgasp_list_head == NULL) {
        printk("Error: kerSysDeregisterDyingGaspHandler: list head is null\n");
    } else {
        list_for_each(pos, &g_cb_dgasp_list_head->list) {
            tmp = list_entry(pos, CB_DGASP_LIST, list);
            if(!strcmp(tmp->name, devname)) {
                list_del(pos);
                kfree(tmp);
                found = 1;
                printk("kerSysDeregisterDyingGaspHandler: %s is deregistered\n", devname);
                break;
            }
        }
        if (!found)
            printk("kerSysDeregisterDyingGaspHandler: %s not (de)registered\n", devname);
    }
    mutex_unlock(&dgaspMutex);

    return;
} /* kerSysDeregisterDyingGaspHandler */


/***************************************************************************
 *
 *
 ***************************************************************************/
static int ConfigCs (BOARD_IOCTL_PARMS *parms)
{
    int                     retv = 0;
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    int                     cs, flags;
    cs_config_pars_t        info;

    if (copy_from_user(&info, (void*)parms->buf, sizeof(cs_config_pars_t)) == 0)
    {
        cs = parms->offset;

        MPI->cs[cs].base = ((info.base & 0x1FFFE000) | (info.size >> 13));

        if ( info.mode == EBI_TS_TA_MODE )     // syncronious mode
            flags = (EBI_TS_TA_MODE | EBI_ENABLE);
        else
        {
            flags = ( EBI_ENABLE | \
                (EBI_WAIT_STATES  & (info.wait_state << EBI_WTST_SHIFT )) | \
                (EBI_SETUP_STATES & (info.setup_time << EBI_SETUP_SHIFT)) | \
                (EBI_HOLD_STATES  & (info.hold_time  << EBI_HOLD_SHIFT )) );
        }
        MPI->cs[cs].config = flags;
        parms->result = BP_SUCCESS;
        retv = 0;
    }
    else
    {
        retv -= EFAULT;
        parms->result = BP_NOT_DEFINED;
    }
#endif
    return( retv );
}


//<< [CTFN-SYS-016] Dylan, jojopo : Support Internet LED for 6328 product
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96318)
static void SetInternetLed(int gpio, GPIO_STATE_t state)
{
    unsigned short gpio_state;

    if (((gpio & BP_ACTIVE_LOW) && (state == kGpioActive)) ||
        (!(gpio & BP_ACTIVE_LOW) && (state == kGpioInactive)))
        gpio_state = 0;
    else
        gpio_state = 1;

    /* Enable LED controller to drive this GPIO */
    if (!(gpio & BP_GPIO_SERIAL))
        GPIO->GPIOMode |= GPIO_NUM_TO_MASK(gpio);

	    LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(gpio));

	    if( gpio_state )
	        LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(gpio));
	    else
	        LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(gpio));
}
#endif
//>> [CTFN-SYS-016] End

/***************************************************************************
* Handle push of restore to default button
***************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static void restore_to_default_thread(struct work_struct *work)
#else
static void restore_to_default_thread(void *arg)
#endif
{
    char buf[256];

    memset(buf, 0, sizeof(buf));

    // Do this in a kernel thread so we don't have any restriction
    printk("Restore to Factory Default Setting ***\n\n");
    kerSysPersistentSet( buf, sizeof(buf), 0 );

//<< [CTFN-SYS-007-1] Charles Wei: add erasing backup PSI data
#ifdef SUPPORT_BACKUP_PSI
    printk("*** Restore to Factory Default Setting (backup psi)");
	kerSysBackupPsiSet( buf, sizeof(buf), 0);
    printk(" done. ***\n\n");
#endif
//>> [CTFN-SYS-007-1] End
//<< [JAZ-TR69-014] Save CR Username/Password in scratch pad, 2012/08/28
    // clear some scratch pad data
    kerSysScratchPadSet("tr69_crUsername", NULL, 0);
    kerSysScratchPadSet("tr69_crPassword", NULL, 0);
//>> [JAZ-TR69-014] End

    // kernel_restart is a high level, generic linux way of rebooting.
    // It calls a notifier list and lets sub-systems know that system is
    // rebooting, and then calls machine_restart, which eventually
    // calls kerSysMipsSoftReset.
    kernel_restart(NULL);
    return;
}

//<< [CTFN-SYS-007] Jimmy Wu : support reset to default button
#ifdef CTCONFIG_SYS_RESET_BTN_GPIO
static irqreturn_t reset_isr(int irq, void *dev_id)
{
    printk("\n***reset button press detected***\n\n");

    INIT_WORK(&restoreDefaultWork, restore_to_default_thread);
    schedule_work(&restoreDefaultWork);
    return IRQ_HANDLED;
}

static int proc_get_resetbtn(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int r = 0;
    unsigned short gpio;
    if ( BpGetResetToDefaultExtIntr(&gpio) == BP_SUCCESS )
    {
        volatile uint64 *gpio_reg = &GPIO->GPIOio;
        uint64 gpio_mask =GPIO_NUM_TO_MASK(gpio);
        r += sprintf(page+r, "%d\n", (bool)(*gpio_reg & gpio_mask));
    }
    *eof = 1;
    return (r < cnt)? r: 0;
}
#elif defined CTCONFIG_SYS_RESET_BTN_IRQ
/* global variables */
static struct timer_list reset_Timer;
static int reset_irq_times = 0;
static unsigned long reset_prev = 0;
static int reset_enabled = 1;

static void enable_reset_irq(void)
{
    unsigned short rstToDflt_irq;

    //printk("*** IRQ had been enabled ***\n");
    if ( BpGetResetToDefaultExtIntr(&rstToDflt_irq) == BP_SUCCESS )
    {
        rstToDflt_irq = map_external_irq (rstToDflt_irq) ;
        BcmHalInterruptEnable(rstToDflt_irq);
	}
}

static irqreturn_t reset_isr(int irq, void *dev_id)
{
    //printk("\n*** Restore btn is pressed irq_times=%d ***\n", reset_irq_times);
    /* Check time elapse since last irq came */
    if (time_after_eq(jiffies, reset_prev) && ((jiffies - reset_prev) <= (HZ >> 5)))// tolerance ~ 31ms
    {
        reset_irq_times ++;
//<< [CTFN-SYS-008] Jimmy Wu : support hold time for reset to default button
        if(reset_enabled && reset_irq_times == CTCONFIG_SYS_RESET_DEFAULT_TIME)
//>> [CTFN-SYS-008] End
        {
            printk("\n*** ");
            INIT_WORK(&restoreDefaultWork, restore_to_default_thread);
            schedule_work(&restoreDefaultWork);
            return IRQ_HANDLED;
        }
    }
    else
        reset_irq_times = 0;

    init_timer(&reset_Timer);
    reset_Timer.function = (void*)enable_reset_irq;
    reset_Timer.expires = jiffies + HZ;        // timer expires in ~1sec
    reset_prev = reset_Timer.expires;
    add_timer (&reset_Timer);

	return IRQ_HANDLED;
}

static int proc_get_reset(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int r = 0;
    r += sprintf(page+r, "%d\n", reset_enabled);
    *eof = 1;
    return (r < cnt)? r: 0;
}

static int proc_set_reset(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
    int i = 0;
    int r = cnt;

    if ((cnt > 32) || (copy_from_user(input, buf, cnt) != 0))
        return -EFAULT;

    for (i = 0; i < r; i ++){
        if (!isxdigit(input[i])){
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    if ((input[0] - '0'))
        reset_enabled = 1;
    else
        reset_enabled = 0;

    return cnt;
}
#endif /* CTCONFIG_SYS_RESET_BTN_GPIO/IRQ */
//>> [CTFN-SYS-007] End

//<< [CTFN-WIFI-001] Jimmy Wu : support wireless button
#ifdef CTCONFIG_WIFI_BUTTON
static int proc_get_wlbtn(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int r = 0;
    unsigned short gpio;
#if defined(CONFIG_BCM96328)
    volatile unsigned long *gpio_reg = &GPIO->GPIOio;
    unsigned long gpio_mask;
#else
    volatile uint64 *gpio_reg = &GPIO->GPIOio;
    uint64 gpio_mask;
#endif

    if((BpGetCtWirelessBtnGpio(&gpio)) == BP_SUCCESS)
    {
        if (gpio != BP_NOT_DEFINED)
        {
            gpio_mask =GPIO_NUM_TO_MASK(gpio);
            r += sprintf(page+r, "%d\n", (bool)(*gpio_reg & gpio_mask));
            *eof = 1;
        }
    }

    return (r < cnt)? r: 0;
}
#endif /* CTCONFIG_WIFI_BUTTON */
//>> [CTFN-WIFI-001] End

#if defined(WIRELESS)
/***********************************************************************
* Function Name: kerSysScreenPciDevices
* Description  : Screen Pci Devices before loading modules
***********************************************************************/
static void __init kerSysScreenPciDevices(void)
{
    unsigned short wlFlag;

    if((BpGetWirelessFlags(&wlFlag) == BP_SUCCESS) && (wlFlag & BP_WLAN_EXCLUDE_ONBOARD)) {
        /*
        * scan all available pci devices and delete on board BRCM wireless device
        * if external slot presents a BRCM wireless device
        */
        int foundPciAddOn = 0;
        struct pci_dev *pdevToExclude = NULL;
        struct pci_dev *dev = NULL;

        while((dev=pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev))!=NULL) {
            printk("kerSysScreenPciDevices: 0x%x:0x%x:(slot %d) detected\n", dev->vendor, dev->device, PCI_SLOT(dev->devfn));
            if((dev->vendor == BRCM_VENDOR_ID) &&
                (((dev->device & 0xff00) == BRCM_WLAN_DEVICE_IDS)|| 
                ((dev->device/1000) == BRCM_WLAN_DEVICE_IDS_DEC))) {
                    if(PCI_SLOT(dev->devfn) != WLAN_ONBOARD_SLOT) {
                        foundPciAddOn++;
                    } else {
                        pdevToExclude = dev;
                    }                
            }
        }

        if(((wlFlag & BP_WLAN_EXCLUDE_ONBOARD_FORCE) || foundPciAddOn) && pdevToExclude) {
            printk("kerSysScreenPciDevices: 0x%x:0x%x:(onboard) deleted\n", pdevToExclude->vendor, pdevToExclude->device);
            pci_remove_bus_device(pdevToExclude);
        }
    }
}

/***********************************************************************
* Function Name: kerSetWirelessPD
* Description  : Control Power Down by Hardware if the board supports
***********************************************************************/
static void kerSetWirelessPD(int state)
{
    unsigned short wlanPDGpio;
    if((BpGetWirelessPowerDownGpio(&wlanPDGpio)) == BP_SUCCESS) {
        if (wlanPDGpio != BP_NOT_DEFINED) {
            if(state == WLAN_OFF)
                kerSysSetGpioState(wlanPDGpio, kGpioActive);
            else
                kerSysSetGpioState(wlanPDGpio, kGpioInactive);
        }
    }
}

#endif


#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
/***********************************************************************
* Function Name: kerSysCheckPowerDownPcie
* Description  : Power Down PCIe if no device enumerated
*                Otherwise enable Power Saving modes
***********************************************************************/
static void __init kerSysCheckPowerDownPcie(void)
{
    struct pci_dev *dev = NULL;
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
    unsigned long GPIOOverlays;
#endif

    while ((dev=pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev))!=NULL) {
        if(BCM_BUS_PCIE_DEVICE == dev->bus->number) {
            /* Enable PCIe L1 PLL power savings */
            PCIEH_BLK_1800_REGS->phyCtrl[1] |= REG_POWERDOWN_P1PLL_ENA;
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
            /* Enable PCIe CLKREQ# power savings */
            if( (BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS) && (GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ)) {
                PCIEH_BRIDGE_REGS->pcieControl |= PCIE_BRIDGE_CLKREQ_ENABLE;
            }
#endif
            return;
        }
    }
            
    printk("PCIe: No device found - Powering down\n");
    /* pcie clock disable*/
#if defined(PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ)
    PCIEH_MISC_HARD_REGS->hard_eco_ctrl_hard |= PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ;
#endif

    PERF->blkEnables &= ~PCIE_CLK_EN;
#if defined(CONFIG_BCM963268)
    MISC->miscLcpll_ctrl |= MISC_CLK100_DISABLE;
#endif

    /* pcie serdes disable */
#if defined(CONFIG_BCM96816)   
    GPIO->SerdesCtl &= ~(SERDES_PCIE_ENABLE|SERDES_PCIE_EXD_ENABLE);
#elif defined(CONFIG_BCM96818) 
    GPIO->SerdesCtl &= ~SERDES_PCIE_ENABLE;
#elif !defined(CONFIG_BCM96318)
    MISC->miscSerdesCtrl &= ~(SERDES_PCIE_ENABLE|SERDES_PCIE_EXD_ENABLE);
#endif

    /* pcie disable additional clocks */
#if defined(PCIE_UBUS_CLK_EN)
    PLL_PWR->PllPwrControlActiveUbusPorts &= ~PORT_ID_PCIE;
    PERF->blkEnablesUbus &= ~PCIE_UBUS_CLK_EN;
#endif

#if defined(PCIE_25_CLK_EN)
    PERF->blkEnables &= ~PCIE_25_CLK_EN;
#endif 

#if defined(PCIE_ASB_CLK_EN)
    PERF->blkEnables &= ~PCIE_ASB_CLK_EN;
#endif

#if defined(SOFT_RST_PCIE) && defined(SOFT_RST_PCIE_EXT) && defined(SOFT_RST_PCIE_CORE)
    /* pcie and ext device */
    PERF->softResetB &= ~(SOFT_RST_PCIE|SOFT_RST_PCIE_EXT|SOFT_RST_PCIE_CORE);
#endif    

#if defined(SOFT_RST_PCIE_HARD)
    PERF->softResetB &= ~SOFT_RST_PCIE_HARD;
#endif

#if defined(IDDQ_PCIE)
    PLL_PWR->PllPwrControlIddqCtrl |= IDDQ_PCIE;
#endif

}
#endif

extern unsigned char g_blparms_buf[];

/***********************************************************************
 * Function Name: kerSysBlParmsGetInt
 * Description  : Returns the integer value for the requested name from
 *                the boot loader parameter buffer.
 * Returns      : 0 - success, -1 - failure
 ***********************************************************************/
int kerSysBlParmsGetInt( char *name, int *pvalue )
{
    char *p2, *p1 = g_blparms_buf;
    int ret = -1;

    *pvalue = -1;

    /* The g_blparms_buf buffer contains one or more contiguous NULL termianted
     * strings that ends with an empty string.
     */
    while( *p1 )
    {
        p2 = p1;

        while( *p2 != '=' && *p2 != '\0' )
            p2++;

        if( *p2 == '=' )
        {
            *p2 = '\0';

            if( !strcmp(p1, name) )
            {
                *p2++ = '=';
                *pvalue = simple_strtol(p2, &p1, 0);
                if( *p1 == '\0' )
                    ret = 0;
                break;
            }

            *p2 = '=';
        }

        p1 += strlen(p1) + 1;
    }

    return( ret );
}

/***********************************************************************
 * Function Name: kerSysBlParmsGetStr
 * Description  : Returns the string value for the requested name from
 *                the boot loader parameter buffer.
 * Returns      : 0 - success, -1 - failure
 ***********************************************************************/
int kerSysBlParmsGetStr( char *name, char *pvalue, int size )
{
    char *p2, *p1 = g_blparms_buf;
    int ret = -1;

    /* The g_blparms_buf buffer contains one or more contiguous NULL termianted
     * strings that ends with an empty string.
     */
    while( *p1 )
    {
        p2 = p1;

        while( *p2 != '=' && *p2 != '\0' )
            p2++;

        if( *p2 == '=' )
        {
            *p2 = '\0';

            if( !strcmp(p1, name) )
            {
                *p2++ = '=';
                strncpy(pvalue, p2, size);
                ret = 0;
                break;
            }

            *p2 = '=';
        }

        p1 += strlen(p1) + 1;
    }

    return( ret );
}

static int add_proc_files(void)
{
#define offset(type, elem) ((int)&((type *)0)->elem)

    static int BaseMacAddr[2] = {offset(NVRAM_DATA, ucaBaseMacAddr), NVRAM_MAC_ADDRESS_LEN};
//<< [CTFN-WIFI-012] Antony.Wu : Set default authentication mode and and default Wireless key, 2011/11/25
    static int WlanPasswd[2] = {((int)(&((NVRAM_DATA *)0)->ctData.szWlanPasswd)), NVRAM_WLAN_PASSWD_LEN};
//>> [CTFN-WIFI-012] End
    struct proc_dir_entry *p0;
    struct proc_dir_entry *p1;

    p0 = proc_mkdir("nvram", NULL);

    if (p0 == NULL)
    {
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }

    p1 = create_proc_entry("BaseMacAddr", 0644, p0);

    if (p1 == NULL)
    {
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }

    p1->data        = BaseMacAddr;
    p1->read_proc   = proc_get_param;
    p1->write_proc  = proc_set_param;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	//New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif

//<< [CTFN-WIFI-012] Antony.Wu : Set default authentication mode and and default Wireless key, 2011/11/25
    p1 = create_proc_entry("WlanPasswd", 0644, p0);
    if (p1 == NULL){
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }
    p1->data        = WlanPasswd;
    p1->read_proc   = proc_get_string;
    p1->write_proc  = proc_set_string;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	//New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif
//>> [CTFN-WIFI-012] End

    p1 = create_proc_entry("led", 0644, NULL);
    if (p1 == NULL)
        return -1;

    p1->write_proc  = proc_set_led;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	//New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif

//<< [CTFN-SYS-007] Jimmy Wu : support reset to default button
#ifdef CTCONFIG_SYS_RESET_BTN_GPIO
    p1 = create_proc_entry("ResetBtn", 0644, NULL);
    if (p1 == NULL){
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }
    p1->read_proc   = proc_get_resetbtn;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif
#elif defined CTCONFIG_SYS_RESET_BTN_IRQ
    p1 = create_proc_entry("ResetBtnEnbl", 0644, NULL);
    if (p1 == NULL){
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }
    p1->read_proc   = proc_get_reset;
    p1->write_proc  = proc_set_reset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif
#endif /* CTCONFIG_SYS_RESET_BTN_GPIO/IRQ */
//>> [CTFN-SYS-007] End

//<< [CTFN-WIFI-001] Jimmy Wu : support wireless button
#ifdef CTCONFIG_WIFI_BUTTON
    p1 = create_proc_entry("WlBtn", 0644, NULL);
    if (p1 == NULL){
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }
    p1->read_proc   = proc_get_wlbtn;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    //New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif
#endif /* CTCONFIG_WIFI_BUTTON */
//>> [CTFN-WIFI-001] End

    return 0;
}

static int del_proc_files(void)
{
    remove_proc_entry("nvram", NULL);
    remove_proc_entry("led", NULL);

//<< [CTFN-SYS-007] Jimmy Wu : support reset to default button
#ifdef CTCONFIG_SYS_RESET_BTN_GPIO
    remove_proc_entry("ResetBtn", NULL);
#elif defined CTCONFIG_SYS_RESET_BTN_IRQ
    remove_proc_entry("ResetBtnEnbl", NULL);
#endif /* CTCONFIG_SYS_RESET_BTN_GPIO/IRQ */
//>> [CTFN-SYS-007] End

//<< [CTFN-WIFI-001] Jimmy Wu : support wireless button
#ifdef CTCONFIG_WIFI_BUTTON
    remove_proc_entry("WlBtn", NULL);
#endif /* CTCONFIG_WIFI_BUTTON */
//>> [CTFN-WIFI-001] End

    return 0;
}

static void str_to_num(char* in, char* out, int len)
{
    int i;
    memset(out, 0, len);

    for (i = 0; i < len * 2; i ++)
    {
        if ((*in >= '0') && (*in <= '9'))
            *out += (*in - '0');
        else if ((*in >= 'a') && (*in <= 'f'))
            *out += (*in - 'a') + 10;
        else if ((*in >= 'A') && (*in <= 'F'))
            *out += (*in - 'A') + 10;
        else
            *out += 0;

        if ((i % 2) == 0)
            *out *= 16;
        else
            out ++;

        in ++;
    }
    return;
}

static int proc_get_param(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int i = 0;
    int r = 0;
    int offset  = ((int *)data)[0];
    int length  = ((int *)data)[1];
    NVRAM_DATA *pNvramData;

    *eof = 1;

    if ((offset < 0) || (offset + length > sizeof(NVRAM_DATA)))
        return 0;

    if (NULL != (pNvramData = readNvramData()))
    {
        for (i = 0; i < length; i ++)
            r += sprintf(page + r, "%02x ", ((unsigned char *)pNvramData)[offset + i]);
    }

    r += sprintf(page + r, "\n");
    if (pNvramData)
        kfree(pNvramData);
    return (r < cnt)? r: 0;
}

static int proc_set_param(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    NVRAM_DATA *pNvramData;
    char input[32];

    int i = 0;
    int r = cnt;
    int offset  = ((int *)data)[0];
    int length  = ((int *)data)[1];

    if ((offset < 0) || (offset + length > sizeof(NVRAM_DATA)))
        return 0;

    if ((cnt > 32) || (copy_from_user(input, buf, cnt) != 0))
        return -EFAULT;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    mutex_lock(&flashImageMutex);

    if (NULL != (pNvramData = readNvramData()))
    {
        str_to_num(input, ((char *)pNvramData) + offset, length);
        writeNvramDataCrcLocked(pNvramData);
    }
    else
    {
        cnt = 0;
    }

    mutex_unlock(&flashImageMutex);

    if (pNvramData)
        kfree(pNvramData);

    return cnt;
}

//<< [CTFN-WIFI-012] Antony.Wu : Set default authentication mode and and default Wireless key, 2011/11/25
// get the string with the length smaller than 32 bytes
static int proc_get_string(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int i = 0;
    int r = 0;
    int offset  = ((int *)data)[0];
    int length  = ((int *)data)[1];
    NVRAM_DATA *pNvramData;

    *eof = 1;

    if ((offset < 0) || (offset + length > sizeof(NVRAM_DATA)))
        return 0;

    if (NULL != (pNvramData = readNvramData()))
    {
        for (i = 0; i < length; i ++)
            r += sprintf(page + r, "%c", ((char *)pNvramData)[offset + i]);
    }

    r += sprintf(page + r, "\n");
    if (pNvramData)
        kfree(pNvramData);
    return (r < cnt)? r: 0;
}
// set the string with the length smaller than 32 bytes
static int proc_set_string(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    NVRAM_DATA *pNvramData;
    char input[32] = {0};

    int offset  = ((int *)data)[0];
    int length  = ((int *)data)[1];
    int i;
    if ((offset < 0) || (offset + length > sizeof(NVRAM_DATA)))
        return 0;

    if ((cnt > 32) || (copy_from_user(input, buf, cnt) != 0))
        return -EFAULT;

    for(i = 0; i < cnt; i++){// check if the char is printable
        if((input[i]>0 && input[i] < 33)||(input[i]>126)){ // printable
            break;
        }
    }
    memset(&input[i], 0, (32 - i));

    mutex_lock(&flashImageMutex);

    if (NULL != (pNvramData = readNvramData()))
    {
        memcpy(((char *)pNvramData) + offset, input, length);
        writeNvramDataCrcLocked(pNvramData);
    }
    else
    {
        cnt = 0;
    }

    mutex_unlock(&flashImageMutex);

    if (pNvramData)
        kfree(pNvramData);

    return cnt;
}
//>> [CTFN-WIFI-012] End

/*
 * This function expect input in the form of:
 * echo "xxyy" > /proc/led
 * where xx is hex for the led number
 * and   yy is hex for the led state.
 * For example,
 *     echo "0301" > led
 * will turn on led 3
 */
static int proc_set_led(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char leddata[16];
    char input[32];
    int i;
    int r;
    int num_of_octets;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if (num_of_octets != 2)
        return -EFAULT;

    str_to_num(input, leddata, num_of_octets);
    kerSysLedCtrl (leddata[0], leddata[1]);
    return cnt;
}


#if defined(CONFIG_BCM96368)

#define DSL_PHY_PHASE_CNTL      ((volatile uint32* const) 0xb00012a8)
#define DSL_CPU_PHASE_CNTL      ((volatile uint32* const) 0xb00012ac)
#define MIPS_PHASE_CNTL         ((volatile uint32* const) 0xb00012b0)
#define DDR1_2_PHASE_CNTL       ((volatile uint32* const) 0xb00012b4)
#define DDR3_4_PHASE_CNTL       ((volatile uint32* const) 0xb00012b8)

// The direction bit tells the automatic counters to count up or down to the
// desired value.
#define PI_VALUE_WIDTH 14
#define PI_COUNT_UP    ( 1 << PI_VALUE_WIDTH )
#define PI_MASK        ( PI_COUNT_UP - 1 )

// Turn off sync mode.  Set bit 28 of CP0 reg 22 sel 5.
static void TurnOffSyncMode( void )
{
    uint32 value;

    value = __read_32bit_c0_register( $22, 5 ) | (1<<28);
    __write_32bit_c0_register( $22, 5, value );
    //    Print( "Sync mode %x\n", value );
    value = DDR->MIPSPhaseCntl;

    // Reset the PH_CNTR_CYCLES to 7.
    // Set the phase counter cycles (bits 16-19) back to 7.
    value &= ~(0xf<<16);
    value |= (7<<16);

    // Set the LLMB counter cycles back to 7.
    value &= ~(0xf<<24);
    value |= (7<<24);
    // Set the UBUS counter cycles back to 7.
    value &= ~(0xf<<28);
    value |= (7<<28);

    // Turn off the LLMB counter, which is what maintains sync mode.
    // Clear bit 21, which is LLMB_CNTR_EN.
    value &= ~(1 << 21);
    // Turn off UBUS LLMB CNTR EN
    value &= ~(1 << 23);

    DDR->MIPSPhaseCntl = value;

    // Reset the MIPS phase to 0.
    PI_lower_set( MIPS_PHASE_CNTL, 0 );

    //Clear Count Bit
    value &= ~(1 << 14);
    DDR->MIPSPhaseCntl = value;

}

// Write the specified value in the lower half of a PI control register.  Each
// 32-bit register holds two values, but they can't be addressed separately.
static void
PI_lower_set( volatile uint32  *PI_reg,
             int               newPhaseInt )
{
    uint32  oldRegValue;
    uint32  saveVal;
    int32   oldPhaseInt;
    int32   newPhase;
    uint32  newVal;
    int     equalCount      = 0;

    oldRegValue = *PI_reg;
    // Save upper 16 bits, which is the other PI value.
    saveVal     = oldRegValue & 0xffff0000;

    // Sign extend the lower PI value, and shift it down into the lower 16 bits.
    // This gives us a 32-bit signed value which we can compare to the newPhaseInt
    // value passed in.

    // Shift the sign bit to bit 31
    oldPhaseInt = oldRegValue << ( 32 - PI_VALUE_WIDTH );
    // Sign extend and shift the lower value into the lower 16 bits.
    oldPhaseInt = oldPhaseInt >> ( 32 - PI_VALUE_WIDTH );

    // Take the low 10 bits as the new phase value.
    newPhase = newPhaseInt & PI_MASK;

    // If our new value is larger than the old value, tell the automatic counter
    // to count up.
    if ( newPhaseInt > oldPhaseInt )
    {
        newPhase = newPhase | PI_COUNT_UP;
    }

    // Or in the value originally in the upper 16 bits.
    newVal  = newPhase | saveVal;
    *PI_reg = newVal;

    // Wait until we match several times in a row.  Only the low 4 bits change
    // while the counter is working, so we can get a false "done" indication
    // when we read back our desired value.
    do
    {
        if ( *PI_reg == newVal )
        {
            equalCount++;
        }
        else
        {
            equalCount = 0;
        }

    } while ( equalCount < 3 );

}

// Write the specified value in the upper half of a PI control register.  Each
// 32-bit register holds two values, but they can't be addressed separately.
static void
PI_upper_set( volatile uint32  *PI_reg,
             int               newPhaseInt )
{
    uint32  oldRegValue;
    uint32  saveVal;
    int32   oldPhaseInt;
    int32   newPhase;
    uint32  newVal;
    int     equalCount      = 0;

    oldRegValue = *PI_reg;
    // Save lower 16 bits, which is the other PI value.
    saveVal     = oldRegValue & 0xffff;

    // Sign extend the upper PI value, and shift it down into the lower 16 bits.
    // This gives us a 32-bit signed value which we can compare to the newPhaseInt
    // value passed in.

    // Shift the sign bit to bit 31
    oldPhaseInt = oldRegValue << ( 16 - PI_VALUE_WIDTH );
    // Sign extend and shift the upper value into the lower 16 bits.
    oldPhaseInt = oldPhaseInt >> ( 32 - PI_VALUE_WIDTH );

    // Take the low 10 bits as the new phase value.
    newPhase = newPhaseInt & PI_MASK;

    // If our new value is larger than the old value, tell the automatic counter
    // to count up.
    if ( newPhaseInt > oldPhaseInt )
    {
        newPhase = newPhase | PI_COUNT_UP;
    }

    // Shift the new phase value into the upper 16 bits, and restore the value
    // originally in the lower 16 bits.
    newVal = (newPhase << 16) | saveVal;
    *PI_reg = newVal;

    // Wait until we match several times in a row.  Only the low 4 bits change
    // while the counter is working, so we can get a false "done" indication
    // when we read back our desired value.
    do
    {
        if ( *PI_reg == newVal )
        {
            equalCount++;
        }
        else
        {
            equalCount = 0;
        }

    } while ( equalCount < 3 );

}

// Reset the DDR PI registers to the default value of 0.
static void ResetPiRegisters( void )
{
    volatile int delay;
    uint32 value;

    //Skip This step for now load_ph should be set to 0 for this anyways.
    //Print( "Resetting DDR phases to 0\n" );
    //PI_lower_set( DDR1_2_PHASE_CNTL, 0 ); // DDR1 - Should be a NOP.
    //PI_upper_set( DDR1_2_PHASE_CNTL, 0 ); // DDR2
    //PI_lower_set( DDR3_4_PHASE_CNTL, 0 ); // DDR3 - Must remain at 90 degrees for normal operation.
    //PI_upper_set( DDR3_4_PHASE_CNTL, 0 ); // DDR4

    // Need to have VDSL back in reset before this is done.
    // Disable VDSL Mip's
    GPIO->VDSLControl = GPIO->VDSLControl & ~VDSL_MIPS_RESET;
    // Disable VDSL Core
    GPIO->VDSLControl = GPIO->VDSLControl & ~(VDSL_CORE_RESET | 0x8);


    value = DDR->DSLCpuPhaseCntr;

    // Reset the PH_CNTR_CYCLES to 7.
    // Set the VDSL Mip's phase counter cycles (bits 16-19) back to 7.
    value &= ~(0xf<<16);
    value |= (7<<16);

    // Set the VDSL PHY counter cycles back to 7.
    value &= ~(0xf<<24);
    value |= (7<<24);
    // Set the VDSL AFE counter cycles back to 7.
    value &= ~(0xf<<28);
    value |= (7<<28);

    // Turn off the VDSL MIP's PHY auto counter
    value &= ~(1 << 20);
    // Clear bit 21, which is VDSL PHY CNTR_EN.
    value &= ~(1 << 21);
    // Turn off the VDSL AFE auto counter
    value &= ~(1 << 22);

    DDR->DSLCpuPhaseCntr = value;

    // Reset the VDSL MIPS phase to 0.
    PI_lower_set( DSL_PHY_PHASE_CNTL, 0 ); // VDSL PHY - should be NOP
    PI_upper_set( DSL_PHY_PHASE_CNTL, 0 ); // VDSL AFE - should be NOP
    PI_lower_set( DSL_CPU_PHASE_CNTL, 0 ); // VDSL MIP's

    //Clear Count Bits for DSL CPU
    value &= ~(1 << 14);
    DDR->DSLCpuPhaseCntr = value;
    //Clear Count Bits for DSL Core
    DDR->DSLCorePhaseCntl &= ~(1<<30);
    DDR->DSLCorePhaseCntl &= ~(1<<14);
    // Allow some settle time.
    delay = 100;
    while (delay--);

    printk("\n****** DDR->DSLCorePhaseCntl=%lu ******\n\n", (unsigned long)
        DDR->DSLCorePhaseCntl);

    // Turn off the automatic counters.
    // Clear bit 20, which is PH_CNTR_EN.
    DDR->MIPSPhaseCntl &= ~(1<<20);
    // Turn Back UBUS Signals to reset state
    DDR->UBUSPhaseCntl = 0x00000000;
    DDR->UBUSPIDeskewLLMB0 = 0x00000000;
    DDR->UBUSPIDeskewLLMB1 = 0x00000000;

}

static void ChipSoftReset(void)
{
    TurnOffSyncMode();
    ResetPiRegisters();
}
#endif


/***************************************************************************
 * Function Name: kerSysGetUbusFreq
 * Description  : Chip specific computation.
 * Returns      : the UBUS frequency value in MHz.
 ***************************************************************************/
unsigned long kerSysGetUbusFreq(unsigned long miscStrapBus)
{
   unsigned long ubus = UBUS_BASE_FREQUENCY_IN_MHZ;

#if defined(CONFIG_BCM96362)
   /* Ref RDB - 6362 */
   switch (miscStrapBus) {

      case 0x4 :
      case 0xc :
      case 0x14:
      case 0x1c:
      case 0x15:
      case 0x1d:
         ubus = 100;
         break;
      case 0x2 :
      case 0xa :
      case 0x12:
      case 0x1a:
         ubus = 96;
         break;
      case 0x1 :
      case 0x9 :
      case 0x11:
      case 0xe :
      case 0x16:
      case 0x1e:
         ubus = 200;
         break;
      case 0x6:
         ubus = 183;
         break;
      case 0x1f:
         ubus = 167;
         break;
      default:
         ubus = 160;
         break;
   }
#endif

   return (ubus);

}  /* kerSysGetUbusFreq */


/***************************************************************************
 * Function Name: kerSysGetChipId
 * Description  : Map id read from device hardware to id of chip family
 *                consistent with  BRCM_CHIP
 * Returns      : chip id of chip family
 ***************************************************************************/
int kerSysGetChipId() { 
        int r;
        r = (int) ((PERF->RevID & CHIP_ID_MASK) >> CHIP_ID_SHIFT);
        /* Force BCM681x variants to be be BCM6816) */
        if( (r & 0xfff0) == 0x6810 )
            r = 0x6816;

        /* Force BCM6369 to be BCM6368) */
        if( (r & 0xfffe) == 0x6368 )
            r = 0x6368;

        /* Force BCM63168, BCM63169, and BCM63269 to be BCM63268) */
        if( ( (r & 0xffffe) == 0x63168 )
          || ( (r & 0xffffe) == 0x63268 ))
            r = 0x63268;

        return(r);
}

/***************************************************************************
 * Function Name: kerSysGetDslPhyEnable
 * Description  : returns true if device should permit Phy to load
 * Returns      : true/false
 ***************************************************************************/
int kerSysGetDslPhyEnable() {
        int id;
        int r = 1;
        id = (int) ((PERF->RevID & CHIP_ID_MASK) >> CHIP_ID_SHIFT);
        if ((id == 0x63169) || (id == 0x63269)) {
	    r = 0;
        }
        return(r);
}

/***************************************************************************
* MACRO to call driver initialization and cleanup functions.
***************************************************************************/
module_init( brcm_board_init );
module_exit( brcm_board_cleanup );

EXPORT_SYMBOL(dumpaddr);
EXPORT_SYMBOL(kerSysGetChipId);
EXPORT_SYMBOL(kerSysGetMacAddressType);
EXPORT_SYMBOL(kerSysGetMacAddress);
EXPORT_SYMBOL(kerSysReleaseMacAddress);
EXPORT_SYMBOL(kerSysGetGponSerialNumber);
EXPORT_SYMBOL(kerSysGetGponPassword);
EXPORT_SYMBOL(kerSysGetSdramSize);
EXPORT_SYMBOL(kerSysGetDslPhyEnable);
EXPORT_SYMBOL(kerSysSetOpticalPowerValues);
EXPORT_SYMBOL(kerSysGetOpticalPowerValues);
#if defined(CONFIG_BCM96368)
EXPORT_SYMBOL(kerSysGetSdramWidth);
#endif
EXPORT_SYMBOL(kerSysLedCtrl);
EXPORT_SYMBOL(kerSysRegisterDyingGaspHandler);
EXPORT_SYMBOL(kerSysDeregisterDyingGaspHandler);
EXPORT_SYMBOL(kerSysSendtoMonitorTask);
EXPORT_SYMBOL(kerSysGetWlanSromParams);
EXPORT_SYMBOL(kerSysGetAfeId);
EXPORT_SYMBOL(kerSysGetUbusFreq);
#if defined(CONFIG_BCM96816)
EXPORT_SYMBOL(kerSysBcmSpiSlaveRead);
EXPORT_SYMBOL(kerSysBcmSpiSlaveReadReg32);
EXPORT_SYMBOL(kerSysBcmSpiSlaveWrite);
EXPORT_SYMBOL(kerSysBcmSpiSlaveWriteReg32);
EXPORT_SYMBOL(kerSysBcmSpiSlaveWriteBuf);
#endif
EXPORT_SYMBOL(BpGetBoardId);
EXPORT_SYMBOL(BpGetBoardIds);
EXPORT_SYMBOL(BpGetGPIOverlays);
EXPORT_SYMBOL(BpGetFpgaResetGpio);
EXPORT_SYMBOL(BpGetEthernetMacInfo);
EXPORT_SYMBOL(BpGetDeviceOptions);
#if defined(CONFIG_BCM963268) && (CONFIG_BCM_EXT_SWITCH)
EXPORT_SYMBOL(BpGetPortConnectedToExtSwitch);
#endif
EXPORT_SYMBOL(BpGetRj11InnerOuterPairGpios);
EXPORT_SYMBOL(BpGetRtsCtsUartGpios);
EXPORT_SYMBOL(BpGetAdslLedGpio);
EXPORT_SYMBOL(BpGetAdslFailLedGpio);
EXPORT_SYMBOL(BpGetWanDataLedGpio);
EXPORT_SYMBOL(BpGetWanErrorLedGpio);
EXPORT_SYMBOL(BpGetVoipLedGpio);
EXPORT_SYMBOL(BpGetPotsLedGpio);
EXPORT_SYMBOL(BpGetVoip2FailLedGpio);
EXPORT_SYMBOL(BpGetVoip2LedGpio);
EXPORT_SYMBOL(BpGetVoip1FailLedGpio);
EXPORT_SYMBOL(BpGetVoip1LedGpio);
EXPORT_SYMBOL(BpGetDectLedGpio);
EXPORT_SYMBOL(BpGetMoCALedGpio);
EXPORT_SYMBOL(BpGetMoCAFailLedGpio);
EXPORT_SYMBOL(BpGetWirelessSesExtIntr);
EXPORT_SYMBOL(BpGetWirelessSesLedGpio);
EXPORT_SYMBOL(BpGetWirelessFlags);
EXPORT_SYMBOL(BpGetWirelessPowerDownGpio);
EXPORT_SYMBOL(BpUpdateWirelessSromMap);
EXPORT_SYMBOL(BpGetSecAdslLedGpio);
EXPORT_SYMBOL(BpGetSecAdslFailLedGpio);
EXPORT_SYMBOL(BpGetDslPhyAfeIds);
EXPORT_SYMBOL(BpGetExtAFEResetGpio);
EXPORT_SYMBOL(BpGetExtAFELDPwrGpio);
EXPORT_SYMBOL(BpGetExtAFELDModeGpio);
EXPORT_SYMBOL(BpGetIntAFELDPwrGpio);
EXPORT_SYMBOL(BpGetIntAFELDModeGpio);
EXPORT_SYMBOL(BpGetAFELDRelayGpio);
EXPORT_SYMBOL(BpGetExtAFELDDataGpio);
EXPORT_SYMBOL(BpGetExtAFELDClkGpio);
EXPORT_SYMBOL(BpGetUart2SdoutGpio);
EXPORT_SYMBOL(BpGetUart2SdinGpio);
EXPORT_SYMBOL(BpGet6829PortInfo);
EXPORT_SYMBOL(BpGetEthSpdLedGpio);
EXPORT_SYMBOL(BpGetLaserDisGpio);
EXPORT_SYMBOL(BpGetLaserTxPwrEnGpio);
EXPORT_SYMBOL(BpGetVregSel1P2);
EXPORT_SYMBOL(BpGetGponOpticsType);
EXPORT_SYMBOL(BpGetDefaultOpticalParams);
EXPORT_SYMBOL(BpGetMiiOverGpioFlag);
//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2009/02/12
EXPORT_SYMBOL(get_dsl_state);
EXPORT_SYMBOL(get_eth_state);
//>> [CTFN-SYS-016] End

#if defined (CONFIG_BCM_ENDPOINT_MODULE)
EXPORT_SYMBOL(BpGetVoiceBoardId);
EXPORT_SYMBOL(BpGetVoiceBoardIds);
EXPORT_SYMBOL(BpGetVoiceParms);
#endif
//<< [CTFN-DRV-001] Antony.Wu : Support 1. Phyid to Ethx mapping and 2. IP101_1001 of ICPLUS, 20111205
EXPORT_SYMBOL(BpGetCtEthPhyMap);
//>> [CTFN-DRV-001] End
