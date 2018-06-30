/* This is a module which is used for Port Scan Detect. */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#if defined(CONFIG_MIPS_BRCM)
//#include <linux/blog.h>
#endif


/* for get ip and tcp header*/
#include <linux/ip.h>
#include <linux/tcp.h>

/* for proc file */
#define PROCFS_NAME "PSD_data"
static struct proc_dir_entry *Our_Proc_File;
int addProc(void);

/* for thread */
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/kthread.h>
struct task_struct *psd_thread = NULL;

/* for seq_print */
#include <linux/seq_file.h>

/* for linked list */
//DECLARE_MUTEX(list_lock)
//struct list_head my_list ;
struct my_share *share ;

struct my_share {
   struct semaphore lock;
   struct list_head my_list;
};

extern struct my_share * Get_share_list(void);

/* entry */
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Comtrend");
MODULE_DESCRIPTION("iptables Port Scan Detect module");
MODULE_ALIAS("ipt_PSD");


const unsigned short BLOCK_TIME = 300;
const unsigned short GENERAL_CLEAR_TIME = 30;
const unsigned short MAX_ENTRY_NUMBER = 300;

unsigned short entryCounter = 0;

// << jojopo : thread
static int psd_fun(void *data)
{

	while (! kthread_should_stop())
	{
		struct list_head *pos , *q;
                struct addr_entry *entry;
		if (down_trylock(&share->lock) == 0)
		{
			list_for_each_safe(pos , q , &share->my_list)
			{
				entry = list_entry(pos ,  struct addr_entry , hook);

				/* port scan detect condiction */
				if(!entry->block && entry->use_count > 45)
				{
					if(entry->last_jiffies >= 10 || entry->use_count >= 250)
					{
						entry->block = 1;
						entry->safe_timer = 0;
					}
				}


				if(entry->last_jiffies == 0)
				{
					entry->safe_timer += 1;

					/* blocked ip need more time to release */
					if((!entry->block && entry->safe_timer >= GENERAL_CLEAR_TIME) ||
					    (entry->block && entry->safe_timer >= BLOCK_TIME))
					{
#if 0
						/* only init entry , reserve addr */
						entry->block = 0;
						entry->safe_timer = 0;
						entry->use_count = 0;
						memset(entry->port , 0 , sizeof(entry->port));
						printk("[release] %s\n" , entry->s_addr);
#else
						/* delete entry for free memory */
						//printk("[delete] %s\n" , entry->s_addr);
						list_del(pos);
						kfree(entry);
						entryCounter--;
						continue;
#endif
					}
				}

				entry->last_jiffies = 0;
			}
			up(&share->lock);
		}

		schedule_timeout_interruptible(HZ * 1);
	}
	return 0;
}

static void init_timer_handle(void)
{
	if (IS_ERR(psd_thread = kthread_run(psd_fun, NULL, "PortScanDetect")))
	{
		psd_thread = NULL;
		printk("brcm flash: PortScanDetect thread start fail\n");
	}
	return;
}
// >> jojopo : thread end

static unsigned int
psd_tg(struct sk_buff *skb, const struct xt_target_param *par)
{
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = NULL;

        //printk("%d.%d.%d.%d protocol:%d sport:%d dport:%d\n" , iph->saddr>>24 & 0xFF , iph->saddr>>16 & 0xFF , iph->saddr>>8 & 0xFF , iph->saddr & 0xFF , iph->protocol , tcph->source , tcph->dest);

	if( (iph->protocol == IPPROTO_TCP) || (iph->protocol == IPPROTO_UDP) )
	{
		struct list_head *pos;
		struct addr_entry *entry , *add_entry;
		bool found = false;

		//srcip:iph->saddr dport: tcph->dest);
		tcph = (struct tcphdr *)  ((char *)iph + (iph->ihl)*4);

		if (down_trylock(&share->lock) == 0)
		{
			list_for_each(pos , &share->my_list)
			{
				entry = list_entry(pos ,  struct addr_entry , hook);

				if(entry->addr == iph->saddr)
				{
					int i = 0;
					found = true;

					for(i=0 ; i<256 ; i++)
					{
						if(entry->port[i] == tcph->dest)
						{
							//printk("repeat port %d\n" , tcph->dest);
							break;
						}
						else if(entry->port[i] == 0)
						{
							entry->port[i] = tcph->dest;
							entry->use_count ++;
							entry->last_jiffies ++;
							//printk("new port %s[%d]:%d\n" , entry->s_addr , i , entry->port[i]);
							break;
						}

						if(i == 255 && entry->block)
						{
							/* The bad guy still request new port , safe_timer stop tick */
							entry->last_jiffies ++;
						}
					}
					break;
				}
			}
			up(&share->lock);
		}

		if(!found)
		{
			if(entryCounter >= MAX_ENTRY_NUMBER)
			{
				if (down_trylock(&share->lock) == 0)
				{
					struct list_head *pos;
					struct addr_entry *entry;

					list_for_each(pos , &share->my_list)
					{
						entry = list_entry(pos ,  struct addr_entry , hook);
						list_del(pos);
						kfree(entry);
						entryCounter--;
						break;
					}
					up(&share->lock);
				}
			}

			if(entryCounter < MAX_ENTRY_NUMBER)
			{
				if (down_trylock(&share->lock) == 0)
				{
					add_entry = kmalloc(sizeof(struct addr_entry) , GFP_ATOMIC);
					memset(add_entry , 0 , sizeof(struct addr_entry));
					add_entry->addr = iph->saddr;
					add_entry->port[0] = tcph->dest;
					add_entry->use_count = 1;
					sprintf(add_entry->s_addr , "%d.%d.%d.%d" , iph->saddr>>24 & 0xFF , iph->saddr>>16 & 0xFF , iph->saddr>>8 & 0xFF , iph->saddr & 0xFF);

					list_add_tail( &(add_entry->hook) , &share->my_list);
					//printk("[add] new entry %s:%d\n" , add_entry->s_addr ,  add_entry->port[0]);
					entryCounter++;
					//printk("[add] new entry counter %d\n", entryCounter);
					up(&share->lock);
				}
				else
				{
					printk("[add fail] can't get lock for %d.%d.%d.%d dport:%d\n", iph->saddr>>24 & 0xFF , iph->saddr>>16 & 0xFF , iph->saddr>>8 & 0xFF , iph->saddr & 0xFF, tcph->dest);
					//kfree(add_entry);
				}
			}

		}
	}
	else
	{
		// do nothing
	}

	return XT_CONTINUE;
}

static struct xt_target psd_tg_reg __read_mostly = {
	.name		= "PSD",
	.revision   = 0,
	.family		= NFPROTO_UNSPEC,
	.target		= psd_tg,
	.me		= THIS_MODULE,
};

#if 0
static int procfile_read(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int r=0;
	r += sprintf(page + r, "%s\n", "Port Scan Data List");

	if (down_trylock(&share->lock) == 0)
	{
		struct list_head *pos;
                struct addr_entry *entry;
		list_for_each(pos , &share->my_list)
		{
			entry = list_entry(pos ,  struct addr_entry , hook);
#if 0
			if(entry->block)
			{
			//	r += sprintf(page + r, "%s\n", entry->s_addr);
				r += sprintf(page + r, "%s block\n", entry->s_addr);
			}
			else r += sprintf(page + r, "%s\n", entry->s_addr);
#else
			r += sprintf(page + r, "%s portcount:%d jiffies:%d safe_timer:%d %s\n", entry->s_addr , entry->use_count , entry->last_jiffies , entry->safe_timer , entry->block ? "[block]" : "");

#endif
		}
		up(&share->lock);
	}

	*eof = 1;
	return r;
}
#endif

// << jojopo : for seq_print

/* start() method */
static void * psd_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct addr_entry *p;
	loff_t off = 0;

	seq_printf(seq, "%s\n", "Port Scan Data List");
	/* The iterator at the requested offset */
	list_for_each_entry(p, &share->my_list, hook) {
		if (*pos == off++) return p;
	}
	return NULL;
}

/* next() method */
static void * psd_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	/* 'v' is a pointer to the iterator returned by start() or
	 * by the previous invocation of next() */
	struct list_head *n = ((struct addr_entry *)v)->hook.next;

	++*pos; /* Advance position */
	/* Return the next iterator, which is the next node in the list */
	return(n != &share->my_list) ?
		list_entry(n, struct addr_entry, hook) : NULL;
}

/* show() method */
static int psd_seq_show(struct seq_file *seq, void *v)
{
	const struct addr_entry *p = v;

	/* Interpret the iterator, 'v' */
	//seq_printf(seq, p->info);
	seq_printf(seq, "%s portcount:%d jiffies:%d safe_timer:%d %s\n", p->s_addr , p->use_count , p->last_jiffies , p->safe_timer , p->block ? "[block]" : "");
	return 0;
}

/* stop() method */
static void psd_seq_stop(struct seq_file *seq, void *v)
{
	/* No cleanup needed in this example */
}

/* Define iterator operations */
static struct seq_operations psd_seq_ops = {
	.start = psd_seq_start,
	.next = psd_seq_next,
	.stop = psd_seq_stop,
	.show = psd_seq_show,
};

static int
psd_seq_open(struct inode *inode, struct file *file)
{
	/* Register the operators */
	return seq_open(file, &psd_seq_ops);
}

static struct file_operations psd_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = psd_seq_open,   /* User supplied */
	.read    = seq_read,       /* Built-in helper function */
	.llseek = seq_lseek,       /* Built-in helper function */
	.release = seq_release,    /* Built-in helper funciton */
};
// >> jojopo

int addProc()
{
	Our_Proc_File = create_proc_entry(PROCFS_NAME, 0644, NULL);
	if (Our_Proc_File == NULL)
	{
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",PROCFS_NAME);
		return -ENOMEM;
	}

	//Our_Proc_File->read_proc  = procfile_read;
	Our_Proc_File->proc_fops  = &psd_proc_fops;
	Our_Proc_File->mode       = S_IFREG | S_IRUGO;
	printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);
	return 0;
}

static int __init psd_tg_init(void)
{
	share = Get_share_list();
        addProc();
	init_timer_handle();
	//INIT_LIST_HEAD(&my_list);
	return xt_register_target(&psd_tg_reg);
}

static void __exit psd_tg_exit(void)
{
	if(psd_thread) kthread_stop(psd_thread);
	xt_unregister_target(&psd_tg_reg);

	if (down_trylock(&share->lock) == 0)
	{
		struct list_head *pos , *q;
		struct addr_entry *entry;
		list_for_each_safe(pos , q , &share->my_list)
		{
			entry = list_entry(pos ,  struct addr_entry , hook);
			printk("[free] %s\n" , entry->s_addr);
			list_del(pos);
			kfree(entry);
		}
		up(&share->lock);
	}

	remove_proc_entry(PROCFS_NAME , NULL);

}

module_init(psd_tg_init);
module_exit(psd_tg_exit);
