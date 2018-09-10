#include <common/ata/ata.h>
#include <common/mm/liballoc.h>
#include <common/io/pci.h>
#include <common/util/kfuncs.h>

#ifdef __x86__
extern void pata_init();
#else
void pata_init() {}
#endif

void ata_init()
{
    tty_prints("[ATA ] Setting up driver\n");
    
    pata_init();
}
