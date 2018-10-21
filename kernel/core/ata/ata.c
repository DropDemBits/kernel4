#include <common/ata/ata.h>
#include <common/mm/liballoc.h>
#include <common/io/pci.h>
#include <common/util/kfuncs.h>

#ifdef __X86__
extern void pata_init(uint16_t ata_subsys);
#else
void pata_init(uint16_t ata_subsys) {}
#endif

static uint16_t ata_subsys = 0;

void ata_init()
{
    ata_subsys = klog_add_subsystem("ATA");
    klog_logln(ata_subsys, INFO, "Setting up driver");
    
    pata_init(ata_subsys);
}
