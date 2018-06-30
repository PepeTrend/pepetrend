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
/***********************************************************************/
/*                                                                     */
/*   MODULE:  board.h                                                  */
/*   PURPOSE: Board specific information.  This module should include  */
/*            all base device addresses and board specific macros.     */
/*                                                                     */
/***********************************************************************/
#ifndef _BOARD_H
#define _BOARD_H

#include "bcm_hwdefs.h"

//<< [CTFN-CMN-009] Jimmy Wu : auto-generate ct_autoconf.h, 2008/08/21
#include "ct_autoconf.h"
//>> [CTFN-CMN-009] End

#ifdef __cplusplus
extern "C" {
#endif

/* BOARD_H_API_VER increases when other modules (such as PHY) depend on */
/* a new function in the board driver or in boardparms.h                */

#define BOARD_H_API_VER 3

/*****************************************************************************/
/*          board ioctl calls for flash, led and some other utilities        */
/*****************************************************************************/
/* Defines. for board driver */
#define BOARD_IOCTL_MAGIC       'B'
#define BOARD_DRV_MAJOR          206

#define BOARD_IOCTL_FLASH_WRITE                 _IOWR(BOARD_IOCTL_MAGIC, 0, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_FLASH_READ                  _IOWR(BOARD_IOCTL_MAGIC, 1, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_DUMP_ADDR                   _IOWR(BOARD_IOCTL_MAGIC, 2, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_MEMORY                  _IOWR(BOARD_IOCTL_MAGIC, 3, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_MIPS_SOFT_RESET             _IOWR(BOARD_IOCTL_MAGIC, 4, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_LED_CTRL                    _IOWR(BOARD_IOCTL_MAGIC, 5, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_PSI_SIZE                _IOWR(BOARD_IOCTL_MAGIC, 6, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_SDRAM_SIZE              _IOWR(BOARD_IOCTL_MAGIC, 7, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_ID                      _IOWR(BOARD_IOCTL_MAGIC, 8, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_CHIP_ID                 _IOWR(BOARD_IOCTL_MAGIC, 9, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_CHIP_REV                _IOWR(BOARD_IOCTL_MAGIC, 10, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_CFE_VER                 _IOWR(BOARD_IOCTL_MAGIC, 11, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_BASE_MAC_ADDRESS        _IOWR(BOARD_IOCTL_MAGIC, 12, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_MAC_ADDRESS             _IOWR(BOARD_IOCTL_MAGIC, 13, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_RELEASE_MAC_ADDRESS         _IOWR(BOARD_IOCTL_MAGIC, 14, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_NUM_ENET_MACS           _IOWR(BOARD_IOCTL_MAGIC, 15, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_NUM_ENET_PORTS          _IOWR(BOARD_IOCTL_MAGIC, 16, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_MONITOR_FD              _IOWR(BOARD_IOCTL_MAGIC, 17, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_WAKEUP_MONITOR_TASK         _IOWR(BOARD_IOCTL_MAGIC, 18, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_TRIGGER_EVENT           _IOWR(BOARD_IOCTL_MAGIC, 19, BOARD_IOCTL_PARMS)        
#define BOARD_IOCTL_GET_TRIGGER_EVENT           _IOWR(BOARD_IOCTL_MAGIC, 20, BOARD_IOCTL_PARMS)        
#define BOARD_IOCTL_UNSET_TRIGGER_EVENT         _IOWR(BOARD_IOCTL_MAGIC, 21, BOARD_IOCTL_PARMS) 
#define BOARD_IOCTL_GET_WLAN_ANT_INUSE          _IOWR(BOARD_IOCTL_MAGIC, 22, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_SES_LED                 _IOWR(BOARD_IOCTL_MAGIC, 23, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_CS_PAR                  _IOWR(BOARD_IOCTL_MAGIC, 25, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_GPIO                    _IOWR(BOARD_IOCTL_MAGIC, 26, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_FLASH_LIST                  _IOWR(BOARD_IOCTL_MAGIC, 27, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_BACKUP_PSI_SIZE         _IOWR(BOARD_IOCTL_MAGIC, 28, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_SYSLOG_SIZE             _IOWR(BOARD_IOCTL_MAGIC, 29, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_SHUTDOWN_MODE           _IOWR(BOARD_IOCTL_MAGIC, 30, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_STANDBY_TIMER           _IOWR(BOARD_IOCTL_MAGIC, 31, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_BOOT_IMAGE_OPERATION        _IOWR(BOARD_IOCTL_MAGIC, 32, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_TIMEMS                  _IOWR(BOARD_IOCTL_MAGIC, 33, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_GPON_OPTICS_TYPE        _IOWR(BOARD_IOCTL_MAGIC, 34, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_DEFAULT_OPTICAL_PARAMS  _IOWR(BOARD_IOCTL_MAGIC, 35, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SPI_SLAVE_INIT              _IOWR(BOARD_IOCTL_MAGIC, 36, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SPI_SLAVE_READ              _IOWR(BOARD_IOCTL_MAGIC, 37, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SPI_SLAVE_WRITE             _IOWR(BOARD_IOCTL_MAGIC, 38, BOARD_IOCTL_PARMS)
//<< [CTFN-SYS-016] ChyanLong : Support Internet LED (implemented by Comtrend), 2008/12/22
#define BOARD_IOCTL_SET_INTF            _IOWR(BOARD_IOCTL_MAGIC, 39, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_REL_INTF            _IOWR(BOARD_IOCTL_MAGIC, 40, BOARD_IOCTL_PARMS)
//>> [CTFN-SYS-016] End
//<< MHTsai : Support Internet LED for ip extension, 2010/10/14
#define BOARD_IOCTL_SET_INT_LED          _IOWR(BOARD_IOCTL_MAGIC, 41, BOARD_IOCTL_PARMS)
//>> MHTsai : End
//<< [CTFN-DRV-001-1] Antony.Wu : Support auto-detect ETHWAN (external PHY) feature, 20111222
#define BOARD_IOCTL_GET_EXTPHY_STATUS	_IOWR(BOARD_IOCTL_MAGIC, 42, BOARD_IOCTL_PARMS)
//>> [CTFN-DRV-001-1] End
//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
#ifdef CTCONFIG_3G_FEATURE
#define BOARD_IOCTL_GET_USB_DONGLE_ID   _IOWR(BOARD_IOCTL_MAGIC, 43 BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_USB_DONGLE_STATE _IOWR(BOARD_IOCTL_MAGIC, 44, BOARD_IOCTL_PARMS)
#endif
//>> [CTFN-3G-001] End
//<< [JAZ-TR69-004] David: Several TR-069 bug fixes and customizations, 2012/02/06
#define BOARD_IOCTL_GET_IMAGE_SEQUENCE  _IOWR(BOARD_IOCTL_MAGIC, 45, BOARD_IOCTL_PARMS)
//>> [JAZ-TR69-004] End
//<< [CTFN-SYS-024] Arius Su: Support ROM & Whole Flash Image Backup from GUI for NOR Flash, 2012/02/15
#define BOARD_IOCTL_GET_BLOCK_COUNT            _IOWR(BOARD_IOCTL_MAGIC, 46, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_BLOCK_SIZE            _IOWR(BOARD_IOCTL_MAGIC, 47, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_GET_FLASH_TYPE         _IOWR(BOARD_IOCTL_MAGIC, 48, BOARD_IOCTL_PARMS)
//>> [CTFN-SYS-024] End
//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
#define BOARD_IOCTL_SET_USBLED            _IOWR(BOARD_IOCTL_MAGIC, 49, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_REL_USBLED            _IOWR(BOARD_IOCTL_MAGIC, 50, BOARD_IOCTL_PARMS)
//>> [JAZ-026] End
//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
#define BOARD_IOCTL_SET_WANCONNSTATUS         _IOWR(BOARD_IOCTL_MAGIC, 51, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_SET_WANIPV6CONNSTATUS         _IOWR(BOARD_IOCTL_MAGIC, 52, BOARD_IOCTL_PARMS)
//>> [CTFN-SYS-016-2] End
//<< [CTFN-SYS-038] Lain : Support watchdog feature, 2012/09/28
#define BOARD_IOCTL_SET_WATCHDOG_TIME     _IOWR(BOARD_IOCTL_MAGIC, 53, BOARD_IOCTL_PARMS)
#define BOARD_IOCTL_CLOSE_WATCHDOG_TIME   _IOWR(BOARD_IOCTL_MAGIC, 54, BOARD_IOCTL_PARMS)
//>> [CTFN-SYS-038] End

typedef enum
{
    XDSL_WAN_TYPE,
    ETH_WAN_TYPE,
    THIR_G_WAN_TYPE
} WAN_PHY_TYPE;


typedef struct inetLedRegParms
{
    char *l3IfName;
    char *l2IfName;
    //unsigned char isLayer2;
    //char *buf;
    int l3IfNameLen;
    int l2IfNameLen;
    WAN_PHY_TYPE wanType;
    //BOARD_IOCTL_ACTION  action;        /* flash read/write: nvram, persistent, bcm image */
    //int result;
    int isEdit;
} INET_LED_REG_PARMS;
//>> [CTFN-SYS-016] End

//<< [CTFN-SYS-016-2] Lucien Huang : Support Internet LED for IPv6, 2012/09/05
// Refer it from wan_states in wanrouter.h
typedef enum
{
    WAN_UNCONFIGURED,	  /* link/channel is not configured */
    WAN_DISCONNECTED,	  /* link/channel is disconnected */
    WAN_CONNECTING,      /* connection is in progress */
    WAN_CONNECTED,	  /* link/channel is operational */
    WAN_DISCONNECTING
}WAN_CONN_STATUS;

typedef struct wanConnStatusRegParms
{
    WAN_CONN_STATUS wanConnStatus;
} WAN_CONN_STATUS_REG_PARMS;

typedef struct wanIPv6ConnStatusRegParms
{
    WAN_CONN_STATUS wanIPv6ConnStatus;
} WAN_IPV6_CONN_STATUS_REG_PARMS;
//>> [CTFN-SYS-016-2] End

//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
typedef enum 
{
	usbLedInitial, //do nothing 
	usbLedOn,
	usbLedOff,
	usbLedFail
}USB_LED_STATUS;
typedef struct usbLedRegParms
{
	USB_LED_STATUS usbLedStatus;
} USB_LED_REG_PARMS;
//>> [JAZ-026] End

// for the action in BOARD_IOCTL_PARMS for flash operation
typedef enum 
{
    PERSISTENT,
    NVRAM,
    BCM_IMAGE_CFE,
    BCM_IMAGE_FS,
    BCM_IMAGE_KERNEL,
    BCM_IMAGE_WHOLE,
    SCRATCH_PAD,
    FLASH_SIZE,
//<< [CTFN-SYS-024] Arius Su: Support ROM & Whole Flash Image Backup from GUI for NOR Flash, 2012/02/15
    FLASH_BLOCK,
    LOCK_FLASH,
    UNLOCK_FLASH,
//>> [CTFN-SYS-024] End
// << [CTFN-SYS-024-1] jojopo : Support Flash Image Backup from GUI for NAND Flash , 2012/05/30
    DUMP_NAND_FLASH_DATA,
    READ_NAND_FLASH_PAGE,
    SHOW_NAND_FLASH_BBT,
    GET_NAND_FLASH_BADBLOCK_NUMBER,
    GET_NAND_FLASH_BADBLOCK,
    GET_NAND_FLASH_TOTALSIZE,
    GET_NAND_FLASH_BLOCKSIZE,
    GET_NAND_FLASH_PAGESIZE,
    GET_NAND_FLASH_OOBSIZE,
// >> [CTFN-SYS-024-1] end
    SET_CS_PARAM,
    BACKUP_PSI,
    SYSLOG
} BOARD_IOCTL_ACTION;
    
typedef struct boardIoctParms
{
    char *string;
    char *buf;
    int strLen;
    int offset;
    BOARD_IOCTL_ACTION  action;        /* flash read/write: nvram, persistent, bcm image */
    int result;
} BOARD_IOCTL_PARMS;

// LED defines 
typedef enum
{   
    kLedAdsl,
    kLedSecAdsl,
    kLedWanData,
    kLedSes,
    kLedVoip,
    kLedVoip1,
    kLedVoip2,
    kLedPots,
    kLedDect,
    kLedGpon,
    kLedMoCA,
    kLedEth0Duplex,
    kLedEth0Spd100,
    kLedEth0Spd1000,
    kLedEth1Duplex,
    kLedEth1Spd100,
    kLedEth1Spd1000,
//<< [CTFN-SYS-015] Jimmy Wu : Support to flash LED when CPE is upgrading the software
#ifdef CTCONFIG_SYS_UPGRADE_FLASH_LED
    kLedPower,
#endif /* CTCONFIG_SYS_UPGRADE_FLASH_LED */
//>> [CTFN-SYS-015] End
//<< [CTFN-SYS-016-1] Camille : Support Internet LED for 963268 product, 2012/01/17
#if defined(CONFIG_BCM963268)
    kLedInetOn,
    kLedInetFail,
#endif
//>> [CTFN-SYS-016-1] End
//<< [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific
    kLedUsbOn,
    kLedUsbFail,
//>> [JAZ-026] End
    kLedEnd                             // NOTE: Insert the new led name before this one.
} BOARD_LED_NAME;

typedef enum
{
    kLedStateOff,                        /* turn led off */
    kLedStateOn,                         /* turn led on */
    kLedStateFail,                       /* turn led on red */
    kLedStateSlowBlinkContinues,         /* slow blink continues at 2HZ interval */
    kLedStateFastBlinkContinues,         /* fast blink continues at 4HZ interval */
    kLedStateUserWpsInProgress,          /* 200ms on, 100ms off */
    kLedStateUserWpsError,               /* 100ms on, 100ms off */
    kLedStateUserWpsSessionOverLap       /* 100ms on, 100ms off, 5 times, off for 500ms */                     
} BOARD_LED_STATE;

typedef void (*HANDLE_LED_FUNC)(BOARD_LED_STATE ledState);

typedef enum
{
    kGpioInactive,
    kGpioActive
} GPIO_STATE_t;

// FLASH_ADDR_INFO is now defined in flash_common.h
#include "flash_common.h"

typedef struct cs_config_pars_tag
{
  int   mode;
  int   base;
  int   size;
  int   wait_state;
  int   setup_time;
  int   hold_time;
} cs_config_pars_t;

#define UBUS_BASE_FREQUENCY_IN_MHZ  160

#define IF_NAME_ETH    "eth"
#define IF_NAME_USB    "usb"
#define IF_NAME_WLAN   "wl"
#define IF_NAME_MOCA   "moca"
#define IF_NAME_ATM    "atm"
#define IF_NAME_PTM    "ptm"
#define IF_NAME_GPON   "gpon"
#define IF_NAME_EPON   "epon"
#define IF_NAME_VEIP    "veip"

#define MAC_ADDRESS_ANY         (unsigned long) -1

/* A unique mac id is required for a WAN interface to request for a MAC address.
 * The 32bit mac id is formated as follows:
 *     bit 28-31: MAC Address IF type (MAC_ADDRESS_*)
 *     bit 20-27: the unit number. 0 for atm0, 1 for atm1, ...
 *     bit 12-19: the connection id which starts from 1.
 *     bit 0-11:  not used. should be zero.
 */
 #define MAC_ADDRESS_EPON        0x40000000
#define MAC_ADDRESS_GPON        0x80000000
#define MAC_ADDRESS_ETH         0xA0000000
#define MAC_ADDRESS_USB         0xB0000000
#define MAC_ADDRESS_WLAN        0xC0000000
#define MAC_ADDRESS_MOCA        0xD0000000
#define MAC_ADDRESS_ATM         0xE0000000
#define MAC_ADDRESS_PTM         0xF0000000

/*****************************************************************************/
/*          Function Prototypes                                              */
/*****************************************************************************/
#if !defined(__ASM_ASM_H)
void dumpaddr( unsigned char *pAddr, int nLen );

extern void kerSysEarlyFlashInit( void );
extern int kerSysGetChipId( void );
extern int kerSysGetDslPhyEnable( void );
extern void kerSysFlashInit( void );
extern void kerSysFlashAddrInfoGet(PFLASH_ADDR_INFO pflash_addr_info);
extern int kerSysCfeVersionGet(char *string, int stringLength);

extern int kerSysNvRamSet(const char *string, int strLen, int offset);
extern void kerSysNvRamGet(char *string, int strLen, int offset);
extern void kerSysNvRamGetBootline(char *bootline);
extern void kerSysNvRamGetBootlineLocked(char *bootline);
extern void kerSysNvRamGetBoardId(char *boardId);
extern void kerSysNvRamGetBoardIdLocked(char *boardId);
extern void kerSysNvRamGetBaseMacAddr(unsigned char *baseMacAddr);
extern unsigned long kerSysNvRamGetVersion(void);

extern int kerSysPersistentGet(char *string, int strLen, int offset);
extern int kerSysPersistentSet(char *string, int strLen, int offset);
extern int kerSysBackupPsiGet(char *string, int strLen, int offset);
extern int kerSysBackupPsiSet(char *string, int strLen, int offset);
extern int kerSysSyslogGet(char *string, int strLen, int offset);
extern int kerSysSyslogSet(char *string, int strLen, int offset);
//<< [CTFN-SYS-024] Arius Su: Support ROM & Whole Flash Image Backup from GUI for NOR Flash, 2012/02/15
extern int kerSysFlashBlockGet(char *string, int strLen, int offset);
extern int kerSysGetFlashLock(void);
extern int kerSysFreeFlashLock(void);
extern int kerSysWholeFlashGet(char *string, int strLen, int offset);
//>> [CTFN-SYS-024] End
//<< jojopo : [JAZ-TR69-004-3] Event "7 Transfer Complete" not sent after TR-069 firmware download" for the model with NAND flash (TTS #3100) , 2012/08/31
extern unsigned int kerSysGetPartOffset( char *name);
extern unsigned int kerSysGetCferamSequence( void );
//>> jojopo : end
// << [CTFN-SYS-024-1] jojopo : Support Flash Image Backup from GUI for NAND Flash , 2012/05/30
extern int kerSysGetNANDFlashTotalSize( void );
extern int kerSysGetNANDFlashBlockSize( void );
extern int kerSysGetNANDFlashPageSize( void );
extern int kerSysGetNANDFlashOobSize( void );
extern int kerSysShowNANDFlashBBT( void );
extern int kerSysGetNANDFlashBadBlockNumber( void );
extern int kerSysGetNANDFlashBadBlock(char *string, int strLen, int offset);
extern int kerSysReadNANDFlashPage(char *string, int strLen, int offset);
extern int kerSysDumpNANDFlashData(char *string, int strLen, int offset);
// >> [CTFN-SYS-024-1] end
extern int kerSysScratchPadList(char *tokBuf, int tokLen);
extern int kerSysScratchPadGet(char *tokName, char *tokBuf, int tokLen);
extern int kerSysScratchPadSet(char *tokName, char *tokBuf, int tokLen);
extern int kerSysScratchPadClearAll(void);
extern int kerSysBcmImageSet( int flash_start_addr, char *string, int size,
    int should_yield);
extern int kerSysBcmNandImageSet( char *rootfs_part, char *string, int img_size,
    int should_yield );
extern int kerSysSetBootImageState( int state );
extern int kerSysGetBootImageState( void );
extern int kerSysSetOpticalPowerValues(unsigned short rxReading, unsigned short rxOffset, 
    unsigned short txReading);
extern int kerSysGetOpticalPowerValues(unsigned short *prxReading, unsigned short *prxOffset, 
    unsigned short *ptxReading);
extern int kerSysBlParmsGetInt( char *name, int *pvalue );
extern int kerSysBlParmsGetStr( char *name, char *pvalue, int size );
extern unsigned long kerSysGetMacAddressType( unsigned char *ifName );
extern int kerSysGetMacAddress( unsigned char *pucaAddr, unsigned long ulId );
extern int kerSysReleaseMacAddress( unsigned char *pucaAddr );
extern void kerSysGetGponSerialNumber( unsigned char *pGponSerialNumber);
extern void kerSysGetGponPassword( unsigned char *pGponPassword);
extern int kerSysGetSdramSize( void );
#if defined(CONFIG_BCM96368)
extern unsigned int kerSysGetSdramWidth( void );
#endif
extern void kerSysGetBootline(char *string, int strLen);
extern void kerSysSetBootline(char *string, int strLen);
extern void kerSysMipsSoftReset(void);
extern void kerSysLedCtrl(BOARD_LED_NAME, BOARD_LED_STATE);
extern int kerSysFlashSizeGet(void);
extern int kerSysMemoryMappedFlashSizeGet(void);
extern unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr, unsigned long len );
extern void kerSysRegisterDyingGaspHandler(char *devname, void *cbfn, void *context);
extern void kerSysDeregisterDyingGaspHandler(char *devname);    
extern int kerConfigCs(BOARD_IOCTL_PARMS *parms);

/* private functions used within board driver only */
void stopOtherCpu(void);
void resetPwrmgmtDdrMips(void);

/*
 * Access to shared GPIO registers should be done by calling these
 * functions below, which will grab a spinlock while accessing the
 * GPIO register.  However, if your code needs to access a shared
 * GPIO register but cannot call these functions, you should still
 * acquire the spinlock.
 * (Technically speaking, I should include spinlock.h and declare extern here
 * but that breaks the C++ compile of xtm.)
 */
//#include <linux/spinlock.h>
//extern spinlock_t bcm_gpio_spinlock;
extern void kerSysSetGpioState(unsigned short bpGpio, GPIO_STATE_t state);
extern void kerSysSetGpioStateLocked(unsigned short bpGpio, GPIO_STATE_t state);
extern void kerSysSetGpioDir(unsigned short bpGpio);
extern void kerSysSetGpioDirLocked(unsigned short bpGpio);
// for the voice code, which has too many kernSysSetGpio to change
#define kerSysSetGpio kerSysSetGpioState


extern unsigned long kerSysGetUbusFreq(unsigned long miscStrapBus);
extern int kerSysGetAfeId( unsigned long *afeId );
#define __kerSysGetAfeId	kerSysGetAfeId

#if defined(CONFIG_BCM_CPLD1)
int BcmCpld1Initialize(void);
void BcmCpld1CheckShutdownMode(void);
void BcmCpld1SetShutdownMode(void);
void BcmCpld1SetStandbyTimer(unsigned int duration);
#endif

#if defined (CONFIG_BCM_AVS_PWRSAVE)
extern void kerSysBcmEnableAvs(int enable);
extern int kerSysBcmAvsEnabled(void);
#endif

#if defined(CONFIG_BCM_DDR_SELF_REFRESH_PWRSAVE)
#define CONFIG_BCM_PWRMNGT_DDR_SR_API
// The structure below is to be declared in ADSL PHY MIPS LMEM, if ADSL is compiled in
typedef struct _PWRMNGT_DDR_SR_CTRL_ {
  union {
   struct {
      unsigned char   phyBusy;
      unsigned char   tp0Busy;
      unsigned char   tp1Busy;
      unsigned char   reserved;
    };
    unsigned int      word;
  };
} PWRMNGT_DDR_SR_CTRL;

void BcmPwrMngtRegisterLmemAddr(PWRMNGT_DDR_SR_CTRL *pDdrSr);
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif /* _BOARD_H */

