/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
 * <:label-BRCM:2011:DUAL/GPL:standard
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
:>
 *
 ************************************************************************/


#ifndef __CMS_BOARDCMDS_H__
#define __CMS_BOARDCMDS_H__

#include <sys/ioctl.h>
#include "cms.h"

/*!\file cms_boardcmds.h
 * \brief Header file for the Board Control Command API.
 *
 * These functions are the simple board control functions that other apps,
 * including GPL apps, may need.  These functions are mostly just wrappers
 * around devCtl_boardIoctl().  The nice things about this file is that
 * it does not require the program to link against additional bcm kernel
 * driver header files.
 *
 */


/** Get the board's br0 interface mac address.
 * 
 * @param macAddrNum (OUT) The user must pass in an array of UINT8 of at least
 *                         MAC_ADDR_LEN (6) bytes long.
 * 
 * @return CmsRet enum.
 */
CmsRet devCtl_getBaseMacAddress(UINT8 *macAddrNum);

//<< [CTFN-SYS-036] Lain: Add mac command to display and set the base MAC address in debug mode, 2012/05/18
CmsRet devCtl_setBaseMacAddress(char *str_macAddr);
CmsRet devCtl_getNvramMacAddress(void);
//>> [CTFN-SYS-036] End
/** Get the available interface mac address.
 * 
 * @param macAddrNum (OUT) The user must pass in an array of UINT8 of at least
 *                         MAC_ADDR_LEN (6) bytes long.
 * 
 * @return CmsRet enum.
 */
CmsRet devCtl_getMacAddress(UINT8 *macAddrNum);


/** Release the interface mac address that is not used anymore
 * 
 * @param macAddrNum (OUT) The user must pass in an array of UINT8 of at least
 *                         MAC_ADDR_LEN (6) bytes long.
 * 
 * @return CmsRet enum.
 */
CmsRet devCtl_releaseMacAddress(UINT8 *macAddrNum);


/** Get the number of ethernet MACS on the system.
 * 
 * @return number of ethernet MACS.
 */
UINT32 devCtl_getNumEnetMacs(void);


/** Get the number of ethernet ports on the system.
 * 
 * @return number of ethernet ports.
 */
UINT32 devCtl_getNumEnetPorts(void);


/** Get SDRAM size on the system.
 * 
 * @return SDRAM size in number of bytes.
 */
UINT32 devCtl_getSdramSize(void);


/** Get the chipId.
 *
 * This info is used in various places, including CLI and writing new
 * flash image.  It may be accessed by GPL apps, so it cannot be put
 * exclusively in the data model.
 *  
 * @param chipId (OUT) The chip id returned by the kernel.
 * @return CmsRet enum.
 */
CmsRet devCtl_getChipId(UINT32 *chipId);


/** Set the boot image state.
 *
 * @param state (IN)   BOOT_SET_NEW_IMAGE, BOOT_SET_OLD_IMAGE,
 *                     BOOT_SET_NEW_IMAGE_ONCE,
 *                     BOOT_SET_PART1_IMAGE, BOOT_SET_PART2_IMAGE,
 *                     BOOT_SET_PART1_IMAGE_ONCE, BOOT_SET_PART2_IMAGE_ONCE
 *
 * @return CmsRet enum.
 */
CmsRet devCtl_setImageState(int state);

/** Get the boot image state.
 *
 * @return             BOOT_SET_PART1_IMAGE, BOOT_SET_PART2_IMAGE,
 *                     BOOT_SET_PART1_IMAGE_ONCE, BOOT_SET_PART2_IMAGE_ONCE
 *
 */
int devCtl_getImageState(void);


/** Get image version string.
 *
 * @return number of bytes copied into verStr
 */
int devCtl_getImageVersion(int partition, char *verStr, int verStrSize);
 
 
/** Get the booted image partition.
 *
 * @return             BOOTED_PART1_IMAGE, BOOTED_PART2_IMAGE
 */
int devCtl_getBootedImagePartition(void);


/** Get the booted image id.
 *
 * @return             BOOTED_NEW_IMAGE, BOOTED_OLD_IMAGE
 */
int devCtl_getBootedImageId(void);

//<< [CTFN-WIFI-012] Antony.Wu : Set default authentication mode and and default Wireless key, 2011/11/25
#ifdef CTCONFIG_WIFI_KEY_PRESET_IN_CFE
CmsRet devCtl_getWlanPasswd(char *WlanPasswd);
CmsRet devCtl_setWlanPasswd(char *WlanPasswd);
#endif /* CTCONFIG_WIFI_KEY_PRESET_IN_CFE */
//>> [CTFN-WIFI-012] End

//<< [CTFN-SYS-017] Antony.Wu : Support Serial Number in NVRAM
#ifdef CTCONFIG_SYS_SERIAL_NUMBER
CmsRet devCtl_getSerialNumber(char *SerialNumber);
CmsRet devCtl_setSerialNumber(char *SerialNumber);
#endif /* CTCONFIG_SYS_SERIAL_NUMBER */
//>> [CTFN-SYS-017] End

//<< [CTFN-SYS-018] Jimmy Wu : Add "modelname" command, 2009/02/20
CmsRet devCtl_getBoardID(char *boardId, int length);
//>> [CTFN-SYS-018] End

//<< [CTFN-DRV-001-1] Antony.Wu : Support auto-detect ETHWAN (external PHY) feature, 20111222
CmsRet devCtl_getExtPhyStatus(UINT32 *exist);
//>> [CTFN-DRV-001-1] End

//<< [JAZ-TR69-004] David: Several TR-069 bug fixes and customizations, 2012/02/06
/** Get the Image Sequence from running Firmware.
 *
 * @return image sequence.
 */
UINT32 devCtl_getCurrentImageSequence(void);
//>> [JAZ-TR69-004] End

//<< [CTFN-SYS-038-1] Support watchdog feature, 2013-03-27
#ifdef CTCONFIG_SYS_ENABLE_WATCH_DOG
extern CmsRet devCtl_closeWatchDogTime(void);
#endif
//>> [CTFN-SYS-038-1] End

#endif /* __CMS_BOARDCMDS_H__ */
