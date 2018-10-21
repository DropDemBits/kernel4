
#include <common/acpi.h>
#include <common/util/klog.h>

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

    klog_early_logln(DEBUG, "ACPI Tables:");
    char sig[5];
    char oem_id[9];
    char oemtable_id[9];
    char asl_id[5];
    memset(sig, 0, 5);
    memset(oem_id, 0, 9);
    memset(oemtable_id, 0, 9);
    memset(asl_id, 0, 5);

    for(size_t i = 0; i < ACPI_MAX_EARLY_TABLES; i++)
    {
        ACPI_TABLE_HEADER* table = acpi_early_get_table(i);

        if(table == NULL)
            break;
        
        memcpy(sig, table->Signature, 4);
        memcpy(oem_id, table->OemId, 8);
        memcpy(oemtable_id, table->OemTableId, 8);
        memcpy(asl_id, table->AslCompilerId, 4);

        klog_early_logln(DEBUG, "[%s] %s %s %s", sig, oem_id, oemtable_id, asl_id);
    }

    return (status);
}

ACPI_TABLE_HEADER* acpi_early_get_table(uint32_t idx)
{    
    ACPI_TABLE_DESC* retable;
    ACPI_STATUS Status = AcpiGetTableByIndex(idx, &retable);

    if(ACPI_FAILURE(Status));
        return NULL;
    return retable;
}

/**
 * @brief  Initalizes the full ACPI subsystem
 * @note   
 * @retval AE_OK if everything was initialized properly
 */
ACPI_STATUS acpi_init()
{
    // Full blown init of subsystem
    ACPI_STATUS status;

    acpi_subsys = klog_add_subsystem("ACPI");

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

    klog_logln(acpi_subsys, INFO, "Initialized ACPI subsystem");
    return (AE_OK);
}

uint16_t acpi_subsys_id()
{
    return acpi_subsys_id;
}
