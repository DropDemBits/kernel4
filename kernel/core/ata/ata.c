#include <common/ata/ata.h>
#include <common/mm/liballoc.h>
#include <common/io/pci.h>
#include <common/util/kfuncs.h>

#ifdef __X86__
extern void pata_init();
#else
void pata_init() {}
#endif

void ata_init()
{
    klog_logln(INFO, "Setting up driver");
    
    pata_init();
}
