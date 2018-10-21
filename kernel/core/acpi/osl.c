#include <common/acpi.h>
#include <common/hal.h>
#include <common/mb2parse.h>
#include <common/mm/mm.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/util/kfuncs.h>

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

/**** Memory Management ****/

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
    // TODO: Build local heap
    return NULL;
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length)
{

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
