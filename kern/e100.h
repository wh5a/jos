#ifndef JOS_KERN_E100_H
#define JOS_KERN_E100_H

#include <inc/types.h>
#include <kern/pci.h>

struct pci_func;
struct Page;

int  e100_txbuf(struct Page *pp, unsigned int size, unsigned int offset);
int  e100_rxbuf(struct Page *pp, unsigned int size, unsigned int offset);
void e100_intr(void);
void e100_init(struct pci_func *);

extern uint8_t e100_irq;

#endif	// JOS_KERN_E100_H
