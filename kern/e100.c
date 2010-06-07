#include <kern/e100.h>
#include <inc/assert.h>
#include <inc/x86.h>

static uint16_t csr; // Base of Control/Status Registers (CSR)
static uint8_t irq;

#define PORT_OFFSET 8

// Copied from console.c, delays 5us.
static void
delay(void)
{
	inb(0x84);
	inb(0x84);
	inb(0x84);
	inb(0x84);
}

void e100_init(struct pci_func *pcif) {
  // Store parameters
  int i, found_io = 0;
  for (i = 0; i < 6; i++) {
    // I/O port numbers are 16 bits, so they should be between 0 and 0xffff.
    if (pcif->reg_base[i] <= 0xffff) {
      csr = pcif->reg_base[i];
      assert(pcif->reg_size[i] == 64);  // CSR is 64-byte
      found_io = 1;
      break;
    }
  }
  if (!found_io)
    panic("Fail to find a valid I/O port base for E100.");
  irq = pcif->irq_line;

  // Reset device by writing the PORT DWORD
  outl(csr+PORT_OFFSET, 0);
}

