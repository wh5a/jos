#include <net/ns.h>
#include <inc/lib.h>

/* The output helper environment's goal is to accept NSREQ_OUTPUT IPC
   messages from the core network server and send the packets
   accompanying these IPC message to the network device driver using
   the system call you added above. The NSREQ_OUTPUT IPC's are sent by
   the low_level_output function in net/lwip/jos/jif/jif.c, which
   glues the lwIP stack to JOS's network system. Each IPC will include
   a page consisting of a struct jif_pkt.
*/

#define PKTMAP		0x10000000

// Modeled after timer.c
void
output(envid_t ns_envid)
{
  binaryname = "ns_output";
  struct jif_pkt *p = (struct jif_pkt *)PKTMAP;
  envid_t whom;
  int32_t req, perm;
        
  // 	- read a packet from the network server
  //	- send the packet to the device driver
  while (1) {
    req = ipc_recv(&whom, p, &perm);

    if (!(perm & PTE_P)) {
      cprintf("Invalid request from %08x: no argument page\n",
              whom);
      continue;
    }

    if (ns_envid != whom) {
      cprintf("NS OUTPUT: got IPC message from env %x not NS\n", whom);
      continue;
    }

    if (req != NSREQ_OUTPUT) {
      cprintf("NS OUTPUT: got IPC message %x from env %x not NSREQ_OUTPUT\n", req, whom);
      continue;
    }

    if ((req = sys_net_txbuf(p->jp_data, p->jp_len)))
      cprintf("Couldn't transmit output: %e\n", req);
    sys_page_unmap(0, p);
  }
}
