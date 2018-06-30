#ifndef __FAP_TM_H_INCLUDED__
#define __FAP_TM_H_INCLUDED__

/*
 <:copyright-BRCM:2007:DUAL/GPL:standard
 
    Copyright (c) 2007 Broadcom Corporation
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

/*
 *******************************************************************************
 * File Name  : fap_tm.h
 *
 * Description: This file contains the FAP Traffic Manager API.
 *
 *******************************************************************************
 */

#if defined(CC_FAP4KE_TM)

/* This MUST be kept in sync with fapIoctl_tmMode_t */
typedef enum {
    FAP_TM_MODE_AUTO=0,
    FAP_TM_MODE_MANUAL,
    FAP_TM_MODE_MAX
} fapTm_mode_t;

/* Management API */
void fapTm_masterConfig(int enable);
int fapTm_portConfig(uint8 port, fapTm_mode_t mode, int kbps, int mbs);
int fapTm_portMode(uint8 port, fapTm_mode_t mode);
int fapTm_portApply(uint8 port, int enable);

/* Debuggging */
void fapTm_status(void);
void fapTm_stats(int port);
void fapTm_dumpMaps(void);

/* Others */
void fapTm_setFlowInfo(fap4kePkt_flowInfo_t *flowInfo_p, uint32 virtDestPortMask);
void fapTm_ioctl(unsigned long arg);
void fapTm_init(void);

#endif /* CC_FAP4KE_TM */

#endif /* __FAP_TM_H_INCLUDED__ */
