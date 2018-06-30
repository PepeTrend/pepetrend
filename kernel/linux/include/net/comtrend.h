/*
 * Comtrend 
 * Author : Joba Yang
 * linux/net/inet/comtrend.h
 */
#ifndef _COMTREND_H
#define _COMTREND_H
#include <linux/skbuff.h>

extern void comtrend_init(void);	 

#define CTID_LEN 20
#define COMTREND_OPEN 0x01
#define COMTREND_CLOSE 0x02
#define COMTREND_HDR_LEN sizeof(struct comtrendhdr)
struct comtrendhdr
{
	__be16 	 ar_hrd;	 /* format of hardware address	 */
	__be16 	 ar_pro;	 /* format of protocol address	 */
	unsigned char	ar_hln;		/* length of hardware address	*/
	unsigned char	ar_pln;		/* length of protocol address	*/	
	unsigned char		comtrend_id[CTID_LEN];	/* format of comtrend ID */
	__be16 	 command;	/* format of command 0x01 for open */
};

#endif	/* _COMTREND_H */

