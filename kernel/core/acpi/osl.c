#include <stdarg.h>

#include <common/acpi.h>
#include <common/hal.h>
#include <common/mb2parse.h>
#include <common/io/pci.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/util/kfuncs.h>

#include <arch/iobase.h>
#include <arch/io.h>

struct acpi_handler
{
    struct acpi_handler* next;
    struct irq_handler* root_handler;
    uint32_t irq_level;
    ACPI_OSD_HANDLER function;
    void* context;
};

// Working within the API provided, there is a list of IRQ handlers to accomodate a variable list of handlers
static struct acpi_handler* handler_head = KNULL;
static process_t* acpid_process = KNULL;

static uintptr_t mapping_base = MMIO_MAP_BASE + 0x1000; // Add a page to skip over MMIO temporary mapping
static uint8_t*  mmio_mapping = (uint8_t*)MMIO_MAP_BASE;

/**
 * @brief  Reserves a section of memory to be mapped
 * @note   
 * @param  bytes: The number of bytes to map
 * @retval The base of the allocation
 */
static size_t page_alloc(size_t bytes)
{
    size_t page_bytes = (bytes + 0xFFF) & ~0xFFF;
    size_t base = mapping_base;
    mapping_base += page_bytes;
    return base;
}

static irq_ret_t acpi_handler_wrapper(struct irq_handler* handler)
{
    struct acpi_handler* node = handler_head;
    if(node == KNULL);
        return IRQ_NOT_HANDLED;

    while(node != KNULL)
    {
        if(node->root_handler == handler)
        {
            // If this is the appropriate interrupt level, fire it
            if(node->function(node->context) == ACPI_INTERRUPT_HANDLED)
                return IRQ_HANDLED;
        }
        node = node->next;
    }

    return IRQ_NOT_HANDLED;
}

/**** Environment & ACPI Tables ****/

ACPI_STATUS AcpiOsInitialize (void)
{
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate (void)
{
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    return (ACPI_PHYSICAL_ADDRESS)mb_rsdp_addr;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
    // We don't implement this
    return AE_NOT_IMPLEMENTED;
}

/**** Memory Management ****/

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
    // TODO: Replace with actual memory management allocation
    // Watermark allocate the pages
    size_t dynamic_base = page_alloc(Length);
    size_t num_pages = (Length + 0xFFF) & ~0xFFF;

    for(size_t off = 0; off < num_pages; off += 0x1000)
        mmu_map_direct(dynamic_base + off, PhysicalAddress + off);

    return (void*)dynamic_base;
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length)
{
    // Will not unmap memory for now...
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    if(LogicalAddress == NULL || PhysicalAddress == NULL)
        return AE_BAD_PARAMETER;

    if(!mmu_is_usable((uintptr_t)LogicalAddress))
        return AE_ERROR;

    *PhysicalAddress = 0;
}

// Heap allocations
void *AcpiOsAllocate(ACPI_SIZE Size)
{
    return kmalloc(Size);
}

void AcpiOsFree(void *Memory)
{
    kfree(Memory);
}

BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length)
{
    for(size_t offset = 0; offset < (Length + 0xFFF) & ~0xFFF; offset += 0x1000)
    {
        if(!mmu_is_usable((uintptr_t)Memory + offset))
            return FALSE;
    }

    return TRUE;
}

BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length)
{
    // TODO: Check if the pages are writeable too
    for(size_t offset = 0; offset < (Length + 0xFFF) & ~0xFFF; offset += 0x1000)
    {
        if(!mmu_is_usable((uintptr_t)Memory + offset))
            return FALSE;
    }

    return TRUE;
}

/**** Multithreading & Scheduling ****/

ACPI_THREAD_ID AcpiOsGetThreadId()
{
    if(sched_active_thread() != KNULL)
        return (ACPI_THREAD_ID)(sched_active_thread()->tid);
    
    return 0;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    if(acpid_process == KNULL)
        acpid_process = process_create();
    
    thread_create(acpid_process, Function, PRIORITY_KERNEL, "ACPID Thread", Context);
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    sched_sleep_ms(Milliseconds);
}

void AcpiOsStall(UINT32 Microseconds)
{
    uint64_t end_time = timer_read_counter(0) + (Microseconds * ACPI_NSEC_PER_USEC);
    volatile uint64_t now = timer_read_counter(0);

    while(end_time > now)
    {
        busy_wait();
        now = timer_read_counter(0);
    }
}

void AcpiOsWaitEventsComplete(void)
{
    // TODO: Wait until all pending GPE threads have executed
}

/**** Mutual Exclusion & Synchronization ****/

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle)
{
    if(OutHandle == NULL);
        return AE_BAD_PARAMETER;

    mutex_t* mutex = mutex_create();
    if(mutex == NULL)
        return AE_NO_MEMORY;

    *OutHandle = (ACPI_MUTEX)mutex;
    
    return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle)
{
    if(Handle == NULL)
        return;

    mutex_destroy(Handle);
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
    if(OutHandle == NULL || InitialUnits > 0);
        return AE_BAD_PARAMETER;

    semaphore_t* semaphore = semaphore_create(MaxUnits);
    if(semaphore == NULL)
        return AE_NO_MEMORY;

    *OutHandle = semaphore;
    
    return AE_OK;
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout)
{
    if(Handle == NULL)
        return AE_BAD_PARAMETER;

    if(Timeout == ACPI_DO_NOT_WAIT && !mutex_can_acquire(Handle))
    {
        return AE_TIME;
    }
    else if(Timeout != ACPI_WAIT_FOREVER)
    {
        uint64_t timeout_break = timer_read_counter(0) + (Timeout * ACPI_NSEC_PER_MSEC);
        volatile uint64_t now = timer_read_counter(0);

        while(!mutex_can_acquire(Handle))
        {
            if(timeout_break < now)
                return AE_TIME;
            now = timer_read_counter(0);
        }
    }

    mutex_acquire(Handle);
    return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle)
{
    if(Handle == NULL)
        return;

    mutex_release(Handle);
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    semaphore_destroy(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if(Handle == NULL)
        return AE_BAD_PARAMETER;

    if(Timeout == ACPI_DO_NOT_WAIT && !semaphore_can_acquire(Handle))
    {
        return AE_TIME;
    }
    else if(Timeout != ACPI_WAIT_FOREVER)
    {
        uint64_t timeout_break = timer_read_counter(0) + (Timeout * ACPI_NSEC_PER_MSEC);
        volatile uint64_t now = timer_read_counter(0);

        while(!semaphore_can_acquire(Handle))
        {
            if(timeout_break < now)
                return AE_TIME;
            now = timer_read_counter(0);
        }
    }

    // TODO: Deal with non-unary units
    semaphore_acquire(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    if(Handle == NULL)
        return AE_BAD_PARAMETER;

    // TODO: Deal with non-unary units
    semaphore_release(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
    if(OutHandle == NULL);
        return AE_BAD_PARAMETER;

    spinlock_t* lock = spinlock_create();
    if(lock == NULL)
        return AE_NO_MEMORY;

    *OutHandle = lock;
    
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    spinlock_destroy(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    cpu_flags_t flags = hal_disable_interrupts();
    spinlock_acquire(Handle);
    return flags;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    spinlock_release(Handle);
    hal_enable_interrupts(Flags);
}

/**** Interrupt Handling ****/

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context)
{
    if(Handler == NULL)
        return AE_BAD_PARAMETER;

    // Check if the handler exists and get the tail
    struct acpi_handler* node = KNULL;
    struct acpi_handler* next = handler_head;
    while(next != KNULL)
    {
        if(next->irq_level == InterruptLevel)
            return AE_ALREADY_EXISTS;
        node = next;
        next = next->next;
    }

    // TODO: There must be a better way to handle this
    struct irq_handler* irq_handle = ic_irq_handle(InterruptLevel, LEGACY, acpi_handler_wrapper);

    // Should this fail, it's most likely out of range
    if(irq_handle == NULL)
        return AE_BAD_PARAMETER;
    
    struct acpi_handler* handler = kmalloc(sizeof(struct acpi_handler));

    handler->next = KNULL;
    handler->irq_level = InterruptLevel;
    handler->function = Handler;
    handler->context = Context;
    handler->root_handler = irq_handle;

    if(node != KNULL)
        node->next = handler;
    else
        handler_head = handler;

    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
    if(Handler == NULL)
        return AE_BAD_PARAMETER;

    // Find the handler
    struct acpi_handler* prev = KNULL;
    struct acpi_handler* node = handler_head;
    while(node != KNULL)
    {
        if(node->irq_level == InterruptNumber && (ACPI_OSD_HANDLER)(node->function) != Handler)
        {
            return AE_BAD_PARAMETER;
        }
        else if(node->irq_level == InterruptNumber && (ACPI_OSD_HANDLER)(node->function) == Handler)
        {
            // Actually free the handler & remove it from the list
            ic_irq_free(node->root_handler);

            if(prev != KNULL)
                prev->next = node->next;
            else
                handler_head = node->next;
            
            kfree(node);
            return AE_OK;
        }

        prev = node;
        node = node->next;
    }

    return AE_NOT_EXIST;
}

/**** MMIO Access ****/

// TODO: Put MMIO accesses behind io abstraction
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width)
{
    mmu_map_direct((uintptr_t)mmio_mapping, (Address & ~0xFFF));
    uint8_t* mmio_address = mmio_mapping + (Address & 0xFFF);

    switch(Width >> 3)
    {
    case 1:
        *Value = (uint64_t)(*mmio_address);
        break;
    case 2:
        *Value = (uint64_t)(*((uint16_t*)mmio_address));
        break;
    case 4:
        *Value = (uint64_t)(*((uint32_t*)mmio_address));
        break;
    case 8:
        *Value = *((uint64_t*)mmio_address);
        break;
    default:
        break;
    }
    mmu_unmap_direct((uintptr_t)mmio_mapping);
    return (AE_OK);
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    mmu_map_direct((uintptr_t)mmio_mapping, (Address & ~0xFFF));
    uint8_t* mmio_address = mmio_mapping + (Address & 0xFFF);

    switch(Width >> 3)
    {
        case 1:
            *mmio_address = (uint8_t)Value;
            break;
        case 2:
            *((uint16_t*)mmio_address) = (uint16_t)Value;
            break;
        case 4:
            *((uint32_t*)mmio_address) = (uint32_t)Value;
            break;
        case 8:
            *((uint64_t*)mmio_address) = (uint64_t)Value;
            break;
        default:
            break;
    }
    mmu_unmap_direct((uintptr_t)mmio_mapping);
    return (AE_OK);
}

/**** Port IO Access ****/

// TODO: Put Port IO accesses behind io abstraction

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
    switch(Width >> 3)
    {
        case 1:
            *Value = (uint32_t)inb((UINT16)Address);
            break;
        case 2:
            *Value = (uint32_t)inw((UINT16)Address);
            break;
        case 4:
            *Value = (uint32_t)inl((UINT16)Address);
            break;
        default:
            break;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    switch(Width >> 3)
    {
        case 1:
            outb((UINT16)Address, (uint8_t)Value);
            break;
        case 2:
            outw((UINT16)Address, (uint16_t)Value);
            break;
        case 4:
            outl((UINT16)Address, (uint32_t)Value);
            break;
        default:
            break;
    }

    return (AE_OK);
}

/**** PCI Configuration Access ****/

ACPI_STATUS AcpiOsReadPciConfiguration (ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width)
{
    if(Width < 64)
    {
        *Value = (uint64_t)pci_read_raw(PciId->Bus, PciId->Device, PciId->Function, Reg, Width >> 3);
    }
    else
    {
        uint64_t data = 0;
        data |= (uint64_t)pci_read_raw(PciId->Bus, PciId->Device, PciId->Function, Reg+0x4, Width >> 3);
        data <<= 32;
        data |= (uint64_t)pci_read_raw(PciId->Bus, PciId->Device, PciId->Function, Reg+0x0, Width >> 3);
        *Value = data;
    }
    return (AE_OK);
}

ACPI_STATUS AcpiOsWritePciConfiguration (ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
    if(Width < 64)
    {
        pci_write_raw(PciId->Bus, PciId->Device, PciId->Function, Reg, Width >> 3, (uint32_t)Value);
    }
    else
    {
        pci_write_raw(PciId->Bus, PciId->Device, PciId->Function, Reg+0x0, Width >> 3, (uint32_t)(Value >>  0));
        pci_write_raw(PciId->Bus, PciId->Device, PciId->Function, Reg+0x4, Width >> 3, (uint32_t)(Value >> 32));
    }
}

/**** Formatted Output ****/

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AcpiOsVprintf(format, args);
    va_end(args);
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsVprintf(const char* format, va_list args)
{
    if(!klog_is_init())
        klog_early_logfv(INFO, KLOG_FLAG_NO_HEADER, format, args);
    else
        klog_logv(acpi_subsys_id(), INFO, format, args);
}

void AcpiOsRedirectOutput(void* Destination) {}

/**** Misc. ACPI Functions ****/

UINT64 AcpiOsGetTimer(void)
{
    // ACPI needs the timer resolution to be in 100ns units
    return timer_read_counter(0) / 100;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void* Info)
{
    if(Function == ACPI_SIGNAL_FATAL)
        return AE_ERROR;
    else
        return AE_OK;
}

ACPI_STATUS AcpiOsGetLine(char* Buffer, UINT32 BufferLength, UINT32* BytesRead)
{
    *BytesRead = 0;
    return AE_OK;
}
