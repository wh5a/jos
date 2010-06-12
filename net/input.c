#include "ns.h"

/* Poll the device driver, using your receive system call, for
   received packets. You will also need to pass each packet to the
   core network server environment using the NSREQ_INPUT IPC message.
*/

#define RX_SLOTS	64
#define PKTMAP		0x10000000

static void *rx_slot[RX_SLOTS];
static int head;
static int tail;

static void
fill_rxbuf(void)
{
	int i, r;
	void *p;

	while (head - tail < RX_SLOTS) {
		i = head % RX_SLOTS;
		p = (void *) PKTMAP + PGSIZE * i;
		if ((r = sys_page_alloc(0, p, PTE_P|PTE_W|PTE_U))) {
			cprintf("Input couldn't page_alloc: %e\n", r);
			break;
		}
		
		if ((r = sys_net_rxbuf(p, PGSIZE)))
			panic("Input couldn't rxbuf: %e", r);

		rx_slot[i] = p;
		head++;
	}
}

static void
forward_rxbuf(envid_t ns_envid)
{
	struct jif_pkt *pkt;
	int i;

	i = tail % RX_SLOTS;
	pkt = rx_slot[i];

	while (*((volatile int *)&pkt->jp_len) == 0)
		sys_yield();

	if (pkt->jp_len > 0)
		ipc_send(ns_envid, NSREQ_INPUT, pkt, PTE_P|PTE_W|PTE_U);
	else 
		cprintf("Input rx'ed a bad packet\n");
	rx_slot[i] = 0;
	sys_page_unmap(0, pkt);

	tail++;
}

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
// 	- read a packet from the device driver
//	- send it to the network server
	for (;;) {
		fill_rxbuf();
		forward_rxbuf(ns_envid);
	}
}
