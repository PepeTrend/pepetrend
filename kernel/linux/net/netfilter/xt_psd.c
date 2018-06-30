/* Kernel module to match Port Scan Detect. */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
//#include <linux/netfilter/xt_mac.h>
#include <linux/netfilter/x_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Comtrend");
MODULE_DESCRIPTION("Xtables: PSD list match");
MODULE_ALIAS("ipt_psd");

#include <linux/list.h>
#include <linux/ip.h>
//struct list_head my_list;

struct addr_entry {
        struct list_head hook;
        int addr;
        char s_addr[16];
        unsigned short port[256];
        unsigned short use_count;
        unsigned short last_jiffies;
        unsigned short safe_timer;
        bool block;
};

struct my_share {
   struct semaphore lock;
   struct list_head my_list;
};
extern struct my_share * Get_share_list(void);
struct my_share *share;

static bool psd_mt(const struct sk_buff *skb, const struct xt_match_param *par)
{
    //const struct xt_mac_info *info = par->matchinfo;

    struct iphdr *iph = ip_hdr(skb);
    struct list_head *pos;
    struct addr_entry *entry;

    bool found = false;

    if (down_trylock(&share->lock) == 0)
    {
        list_for_each(pos , &share->my_list)
        {
            entry = list_entry(pos ,  struct addr_entry , hook);
            if( (entry->block) && (entry->addr == iph->saddr) )
            {
                //printk("match %s" , entry->s_addr);
                found = true;
            }
        }
        up(&share->lock);
    }
    return found;
}

static struct xt_match psd_mt_reg __read_mostly = {
	.name      = "psd",
	.revision  = 0,
	.family    = NFPROTO_UNSPEC,
	.match     = psd_mt,
	//.matchsize = sizeof(struct xt_mac_info),
	.matchsize = 0,
	.hooks     = (1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_LOCAL_IN) |
	             (1 << NF_INET_FORWARD),
	.me        = THIS_MODULE,
};

static int __init psd_mt_init(void)
{
	share = Get_share_list();
	return xt_register_match(&psd_mt_reg);
}

static void __exit psd_mt_exit(void)
{
	xt_unregister_match(&psd_mt_reg);
}

module_init(psd_mt_init);
module_exit(psd_mt_exit);
