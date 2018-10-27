
#include <common/acpi.h>
#include <common/util/klog.h>
#include <common/util/kfuncs.h>

static ACPI_TABLE_DESC early_tables[ACPI_MAX_EARLY_TABLES];

// ACPI KLOG subsystem ID
static uint16_t acpi_subsys = 0;

ACPI_STATUS acpi_early_init()
{
    ACPI_STATUS status = AE_OK;

    // Gather all of the tables
    status = AcpiInitializeTables(early_tables, ACPI_MAX_EARLY_TABLES, TRUE);

    if(ACPI_FAILURE(status))
    {
        klog_early_logln(ERROR, "Early ACPI Error: %s", AcpiFormatException(status));
        return (status);
    }
    klog_early_logln(INFO, "Gathered ACPI tables");

    klog_early_logln(INFO, "ACPI Tables:");

    for(size_t i = 0; i < ACPI_MAX_EARLY_TABLES; i++)
    {
        ACPI_TABLE_HEADER* table;
        AcpiGetTableByIndex(i, &table);

        if(table == NULL)
            break;

        if(ACPI_COMPARE_NAME(table->Signature, ACPI_SIG_FACS))
            klog_early_logln(INFO, "\t%.4s (%8dB) v%02.2x",
                table->Signature,
                table->Length,
                table->Revision);
        else
            klog_early_logln(INFO, "\t%.4s (%8dB) v%02.2x OEM=%-8.8s %-8.8s %-4.4s",
                table->Signature,
                table->Length,
                table->Revision,
                table->OemId,
                table->OemTableId,
                table->AslCompilerId);
        
        AcpiPutTable(table);
    }

    return (status);
}

ACPI_STATUS acpi_init()
{
    // Full blown init of subsystem
    ACPI_STATUS status;

    acpi_subsys = klog_add_subsystem("ACPI");
    klog_logln(acpi_subsys, INFO, "Initializing ACPI subsystem");

    status = AcpiInitializeSubsystem();
    if(ACPI_FAILURE(status))
    {
        klog_logln(acpi_subsys, ERROR, "Failed to initialize ACPICA subsystem (%s)", AcpiFormatException(status));
        return (status);
    }

    // Move root table to virtual memory
    status = AcpiReallocateRootTable();
    if(ACPI_FAILURE(status))
    {
        klog_logln(acpi_subsys, ERROR, "Failed to move root table (%s)", AcpiFormatException(status));
        return (status);
    }

    // Load tables & build namespace
    status = AcpiLoadTables();
    if(ACPI_FAILURE(status))
    {
        klog_logln(acpi_subsys, ERROR, "Failed to load tables / build namespace (%s)", AcpiFormatException(status));
        return (status);
    }

    // Install local handlers (if need be)

    // Init ACPI hardware
    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if(ACPI_FAILURE(status))
    {
        klog_logln(acpi_subsys, ERROR, "Failed to enable ACPI hardware (%s)", AcpiFormatException(status));
        return (status);
    }

    // Finalize namespace object initialization
    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if(ACPI_FAILURE(status))
    {
        klog_logln(acpi_subsys, ERROR, "Failed to finalize namespace objects (%s)", AcpiFormatException(status));
        return (status);
    }

    return (AE_OK);
}

ACPI_TABLE_HEADER* acpi_early_get_table(char* sig, uint32_t instance)
{
    ACPI_TABLE_HEADER* table = NULL;
    AcpiGetTable(sig, instance, &table);
    return table;
}

ACPI_TABLE_HEADER* acpi_get_table(char* sig, uint32_t instance)
{    
    ACPI_TABLE_HEADER* table = NULL;
    ACPI_STATUS Status = AcpiGetTable(sig, instance, &table);

    if(ACPI_FAILURE(Status) && table == NULL);
        return NULL;
    return table;
}

void acpi_put_table(ACPI_TABLE_HEADER* table)
{
    AcpiPutTable(table);
}

ACPI_STATUS acpi_enter_sleep(uint8_t sleep_state)
{
    ACPI_STATUS Status;
    Status = AcpiEnterSleepStatePrep(sleep_state);
    if(ACPI_FAILURE(Status))
    {
        klog_logln(acpi_subsys, ERROR, "Error in preparing to enter S%d: %s", sleep_state, AcpiFormatException(Status));
        return Status;
    }

    // AcpiEnterSleepState MUST be executed without interrupts
    hal_disable_interrupts_raw();

    Status = AcpiEnterSleepState(sleep_state);
    if(ACPI_FAILURE(Status))
    {
        klog_logln(acpi_subsys, FATAL, "Failed to enter sleep state S%d: %s", sleep_state, AcpiFormatException(Status));
        return Status;
    }

    // We only return here if we entered S1 (Power-On-Suspend)
    return AE_OK;
}

ACPI_STATUS acpi_leave_sleep(uint8_t sleep_state)
{
    ACPI_STATUS Status;
    
    Status = AcpiLeaveSleepStatePrep(sleep_state);
    if(ACPI_FAILURE(Status))
    {
        klog_logln(acpi_subsys, ERROR, "Error in preparing to leave S%d: %s", sleep_state, AcpiFormatException(Status));
        return Status;
    }

    Status = AcpiLeaveSleepState(sleep_state);
    if(ACPI_FAILURE(Status))
    {
        klog_logln(acpi_subsys, ERROR, "Error in preparing to leave S%d: %s", sleep_state, AcpiFormatException(Status));
        return Status;
    }

    return AE_OK;
}

uint16_t acpi_subsys_id()
{
    return acpi_subsys;
}
