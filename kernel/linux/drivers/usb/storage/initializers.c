/* Special Initializers for certain USB Mass Storage devices
 *
 * Current development and maintenance by:
 *   (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/errno.h>

#include "usb.h"
#include "initializers.h"
#include "debug.h"
#include "transport.h"

//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
typedef struct _h21SwitchModeReq
{
  unsigned char header[4];
  unsigned char data[12];
}h21SwitchModeReq;

//<<add from "usb_modeswitch" freeware
int hex2num(char c)
{
	if (c >= '0' && c <= '9')
	return c - '0';
	if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
	return -1;
}

int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
	return -1;
	b = hex2num(*hex++);
	if (b < 0)
	return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, char *buffer, int len)
{
	int i;
	int a;
	const char *ipos = hex;
	char *opos = buffer;
//    printf("Debug: hexstr2bin bytestring is ");

	for (i = 0; i < len; i++) {
	a = hex2byte(ipos);
//        printf("%02X", a);
	if (a < 0)
		return -1;
	*opos++ = a;
	ipos += 2;
	}
//    printf(" \n");
	return 0;
}
//>>add from "usb_modeswitch" free SW

//Added eject routine, modeled from zy driver
static int eject_installer(struct usb_interface *intf , char* msgbody)
{
       struct usb_device *udev = interface_to_usbdev(intf);
       struct usb_host_interface *iface_desc = &intf->altsetting[0];
       struct usb_endpoint_descriptor *endpoint;
       unsigned char *cmd;
       u8 bulk_out_ep;
       int r;

//	char ByteString[31];
//	char buffer[4096];

       /* Find bulk out endpoint */
       endpoint = &iface_desc->endpoint[1].desc;
       if ((endpoint->bEndpointAddress & USB_TYPE_MASK) == USB_DIR_OUT &&
           (endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
           USB_ENDPOINT_XFER_BULK) {
               bulk_out_ep = endpoint->bEndpointAddress;
       } else {
               dev_err(&udev->dev,
                       "zd1211rw: Could not find bulk out endpoint\n");
               return -ENODEV;
       }

	printk("bulk_out_ep = %d\n" , bulk_out_ep);

       cmd = kzalloc(31, GFP_KERNEL);
       if (cmd == NULL)
               return -ENODEV;

	if ( hexstr2bin(msgbody, cmd, strlen(msgbody)/2) == -1) {
			printk("Error: MessageContent %s\n is not a hex string. Aborting.\n\n", msgbody);
			return -ENODEV;
	}
#if 0 /* for Novatel MC950D/990D */
       /* USB bulk command block */
       cmd[0] = 0x55;  /* bulk command signature */
       cmd[1] = 0x53;  /* bulk command signature */
       cmd[2] = 0x42;  /* bulk command signature */
       cmd[3] = 0x43;  /* bulk command signature */
       cmd[14] = 6;    /* command length */

       cmd[15] = 0x1b; /* SCSI command: START STOP UNIT */
       cmd[19] = 0x2;  /* eject disc */
#endif

       dev_info(&udev->dev, "Ejecting virtual installer media...\n");
       r = usb_bulk_msg(udev, usb_sndbulkpipe(udev, bulk_out_ep),
               cmd, 31, NULL, 2000);
       kfree(cmd);
       if (r)
               return r;

       /* At this point, the device disconnects and reconnects with the real
        * ID numbers. */

       usb_set_intfdata(intf, NULL);
       return 0;
}
//>> [CTFN-3G-001] End

/* This places the Shuttle/SCM USB<->SCSI bridge devices in multi-target
 * mode */
int usb_stor_euscsi_init(struct us_data *us)
{
	int result;

	US_DEBUGP("Attempting to init eUSCSI bridge...\n");
	us->iobuf[0] = 0x1;
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			0x0C, USB_RECIP_INTERFACE | USB_TYPE_VENDOR,
			0x01, 0x0, us->iobuf, 0x1, 5*HZ);
	US_DEBUGP("-- result is %d\n", result);

	return 0;
}

/* This function is required to activate all four slots on the UCR-61S2B
 * flash reader */
int usb_stor_ucr61s2b_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = "\xec\x0a\x06\x00$PCCHIPS";

	US_DEBUGP("Sending UCR-61S2B initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(init_string) - 1;
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string) - 1);

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);
	if(res)
		return res;

	US_DEBUGP("Getting status packet...\n");
	res = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
			US_BULK_CS_WRAP_LEN, &partial);

	return (res ? -1 : 0);
}

/* This places the HUAWEI E220 devices in multi-port mode */
int usb_stor_huawei_e220_init(struct us_data *us)
{
	int result;

//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
printk("######### usb_stor_huawei_init ########\n");
	us->iobuf[0] = 0x1;
//>> [CTFN-3G-001] End
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
				      USB_REQ_SET_FEATURE,
				      USB_TYPE_STANDARD | USB_RECIP_DEVICE,
//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
				      0x01, 0x0, us->iobuf, 0x1, 1000);
//				      0x01, 0x0, NULL, 0x0, 1000);
	US_DEBUGP("usb_control_msg performing result is %d\n", result);
	return (result ? 0 : -ENODEV);
//	return (result ? 0 : -1);
//>> [CTFN-3G-001] End
}

//<< [CTFN-3G-004] Dylan : Support Huawei E173 3G donlgle
int usb_stor_huawei_e173_init(struct us_data *us)
{
    int result;
    unsigned int partial;
    static char init_string[] =
	"\x55\x53\x42\x43\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x11"
	"\x06\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00";
    printk("######### %s ########\n" , __FUNCTION__);
    memcpy(us->iobuf, init_string, sizeof(init_string) - 1);
    result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
	us->iobuf,
	US_BULK_CB_WRAP_LEN,
        &partial);
        US_DEBUGP("Huawei mode set result is %d\n", result);
        return 0;
}
//>> [CTFN-3G-004] End

//<< [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
int usb_stor_benq_h21_init(struct us_data *us)
{
	int result;
	h21SwitchModeReq h21_usb_cm_req;
	h21_usb_cm_req.data[5]=0x03;

printk("######### usb_stor_benq_h21_init ########\n");
	//us->iobuf[0] = 0x1;  /*set 1 to qualcomm mode,set 2 to CDCACM mode*/
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
				      0x04,
				      0 | (0x02 << 5) | 0x00,
				      0x00, 0x0, (char *)&h21_usb_cm_req, sizeof(h21SwitchModeReq), 1000);

	US_DEBUGP("usb_control_msg performing result is %d\n", result);
	return (result ? 0 : -1);
}

int usb_stor_zte_mf626_mf628_init(struct us_data *us)
{
	char MessageContent[62];

	//strcpy(MessageContent , "5553424312345678000000000000061b000000030000000000000000000000");/* for ZTE MF628 */
	strcpy(MessageContent , "55534243123456782000000080000c85010101180101010101000000000000");/* for ZTE MF626 */
	//strcpy(MessageContent , "55534243123456780000000000000616000000000000000000000000000000");

	printk("######### usb_stor_zte_mf626_mf628_init ########\n");
#if 1
       if (0 == eject_installer(us->pusb_intf, MessageContent)) {
               printk(KERN_INFO USB_STORAGE "Ejected Unusual Device \n");
		 return 0;
       }
       else {
               printk(KERN_INFO USB_STORAGE "Eject Failed on Unusual");
		 return 1;
       }
#endif
}

//<< [CTFN-3G-009] Herbert: Support Huawei E1752C/E1752 and ZTE MF110/MF190 3G dongle, 2011/04/19
int usb_stor_zte_mf110_init(struct us_data *us)
{
	char MessageContent[62];

	//strcpy(MessageContent , "5553424312345678000000000000061b000000020000000000000000000000");/* for ZTE MF110 */
//	strcpy(MessageContent , "5553424312345678000000000000061b000000020000000000000000000000");/* for ZTE MF110 */
	strcpy(MessageContent , "55534243123456782400000080000685000000240000000000000000000000");/* for ZTE MF110 */
	//strcpy(MessageContent , "5553424312345678000000000000061b000000020000000000000000000000");

	printk("######### usb_stor_zte_mf110_init ########\n");
#if 1
       if (0 == eject_installer(us->pusb_intf, MessageContent)) {
               printk(KERN_INFO USB_STORAGE "Ejected Unusual Device \n");
		 return 0;
       }
       else {
               printk(KERN_INFO USB_STORAGE "Eject Failed on Unusual");
		 return 1;
       }
#endif
}
//>> [CTFN-3G-009] End

int usb_stor_novatel_mc950d_mc990d_init(struct us_data *us)
{
	char MessageContent[62];

	strcpy(MessageContent , "5553424312345678000000000000061b000000020000000000000000000000");/* for Novatel MC950D/MC990D */

	printk("######### usb_stor_novatel_mc950d_mc990d_init ########\n");
#if 1
       if (0 == eject_installer(us->pusb_intf, MessageContent)) {
               printk(KERN_INFO USB_STORAGE "Ejected Unusual Device \n");
		 return 0;
       }
       else {
               printk(KERN_INFO USB_STORAGE "Eject Failed on Unusual");
		 return 1;
       }
#endif
}

int usb_stor_longcheer_wm66_init(struct us_data *us)
{
	char MessageContent[62];

	strcpy(MessageContent , "55534243123456780000000000000606f50402527000000000000000000000");/* for LONGCHEER WM66 */

	printk("######### usb_stor_wm66_init ########\n");
#if 1
       if (0 == eject_installer(us->pusb_intf, MessageContent)) {
               printk(KERN_INFO USB_STORAGE "Ejected Unusual Device \n");
		 return 0;
       }
       else {
               printk(KERN_INFO USB_STORAGE "Eject Failed on Unusual");
		 return 1;
       }
#endif
}

int usb_stor_TandW_init(struct us_data *us)
{
	char MessageContent[62];

	strcpy(MessageContent , "5553424300000000000000000000061b000000020000000000000000000000");/* for  */

	printk("######### usb_stor_TandW_init ########\n");
#if 1
       if (0 == eject_installer(us->pusb_intf, MessageContent)) {
               printk(KERN_INFO USB_STORAGE "Ejected Unusual Device \n");
		 return 0;
       }
       else {
               printk(KERN_INFO USB_STORAGE "Eject Failed on Unusual");
		 return 1;
       }
#endif
}
//>> [CTFN-3G-001] End