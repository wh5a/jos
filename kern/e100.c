#include <kern/e100.h>
#include <inc/assert.h>

static uint32_t reg_base[6];
static uint32_t reg_size[6];
static uint8_t irq_line;

void e100_store_params(struct pci_func *pcif) {
  int i;
  for (i = 0; i < 6; i++) {
    reg_base[i] = pcif->reg_base[i];
    reg_size[i] = pcif->reg_size[i];
  }
  irq_line = pcif->irq_line;
}

