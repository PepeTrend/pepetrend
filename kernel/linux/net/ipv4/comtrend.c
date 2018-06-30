/*
 * Comtrend 
 * Author : Joba Yang
 * linux/net/ipv4/comtrend.c
 */ 
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/capability.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/mm.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/fddidevice.h>
#include <linux/if_arp.h>
#include <linux/trdevice.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/rcupdate.h>
#include <linux/jhash.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif
 
#include <net/net_namespace.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/protocol.h>
#include <net/tcp.h>
#include <net/sock.h>
#include <net/arp.h>
#include <net/ax25.h>
#include <net/netrom.h>
#if defined(CONFIG_ATM_CLIP) || defined(CONFIG_ATM_CLIP_MODULE)
#include <net/atmclip.h>
 struct neigh_table *clip_tbl_hook;
#endif
 
#include <asm/system.h>
#include <asm/uaccess.h>
 
#include <linux/netfilter_arp.h>
#include <net/comtrend.h>

static unsigned char comtrend_id[CTID_LEN + 1] = {"N"};
static __be16 comtrend_cmd = 0;
struct comtrendhdr *comtrend_hdr(const struct sk_buff *skb)
{
	return (struct comtrendhdr *)skb_network_header(skb);
}
 
static int comtrend_rcv(struct sk_buff *skb, struct net_device *dev,
			struct packet_type *pt, struct net_device *orig_dev)
{
	struct comtrendhdr *comtrend;
	if (!pskb_may_pull(skb, COMTREND_HDR_LEN))
		goto freeskb;

	comtrend = comtrend_hdr(skb);
	if (comtrend->ar_hln != dev->addr_len ||
	    dev->flags & IFF_NOARP ||
	    skb->pkt_type == PACKET_OTHERHOST ||
	    skb->pkt_type == PACKET_LOOPBACK ||
	    comtrend->ar_pln != 4)
		goto freeskb;

	memcpy((void *)comtrend_id, (void *)comtrend->comtrend_id, CTID_LEN);
	comtrend_id[CTID_LEN] = '\0';
	comtrend_cmd = comtrend->command;
freeskb:
	kfree_skb(skb);
	return 0;
}
 
/*
 *  Called once on startup.
 */
 
static struct packet_type comtrend_packet_type __read_mostly = {
	.type = cpu_to_be16(ETH_P_CT),
	.func = comtrend_rcv,
};
 
static int comtrend_proc_init(void);
 
 void __init comtrend_init(void)
 {
	 dev_add_pack(&comtrend_packet_type);
	 comtrend_proc_init();
 }

#define comtrend_PROC_DIR "comtrend"
int comtrend_read_proc(char *buf, char **start , off_t offset, 
						int count, int *eof, void *data)
{
	int len;
	len = sprintf(buf, "%s %d\n", comtrend_id, comtrend_cmd);
	*eof = 1;
	return len;
} 
static int __init comtrend_proc_init(void)
{
	struct proc_dir_entry *comtrend_proc_read;
	comtrend_proc_read = create_proc_read_entry(comtrend_PROC_DIR, S_IRUGO, NULL, comtrend_read_proc, NULL);
	if(!comtrend_proc_read){
		remove_proc_entry(comtrend_PROC_DIR, NULL);
		printk("Create PROC /proc/%s fail\n", comtrend_PROC_DIR);
	}
	return 0;
}

