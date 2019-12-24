
#include <common/acpi.h>
#include <common/mm/liballoc.h>
#include <common/util/klog.h>
#include <common/util/kfuncs.h>

// Status flags
#define ACPI_EC_OBF     0x01
#define ACPI_EC_IBF     0x02
#define ACPI_EC_CMD     0x08
#define ACPI_EC_BURST   0x10
#define ACPI_EC_SCI_EVT 0x20
#define ACPI_EC_SMI_EVT 0x40

// Commands
#define RD_EC 0x80 // Read from EC
#define WR_EC 0x81 // Write to EC
#define BE_EC 0x82 // Burst Enable
#define BD_EC 0x83 // Burst Disable
#define QR_EC 0x84 // Query EC

struct ECDT_CONTEXT
{
    unsigned long Control;
    unsigned long Data;
    bool IsPortSpace;
    uint8_t GpeBit;
};

static struct ECDT_CONTEXT ec_context = {};

static void ec_wait_on (uint8_t wait_mask)
{
    uint32_t sts = 0;
    uint32_t expected_wait = 0;

    // Build the expected to wait on mask
    if (wait_mask & ACPI_EC_IBF)
        expected_wait |= ACPI_EC_IBF;
    if (wait_mask & ACPI_EC_OBF)
        expected_wait |= 0;

    AcpiOsReadPort(ec_context.Control, &sts, 8);
    while((sts & wait_mask) == expected_wait)
    {
        busy_wait();
        AcpiOsReadPort(ec_context.Control, &sts, 8);
    }
}

static void acpi_ec_read(uint8_t Address, void* Store)
{
    // Send Read (0x80) Command and Address to EC
    AcpiOsWritePort(ec_context.Control, RD_EC, 8);
    ec_wait_on(ACPI_EC_IBF);

    // Send the address, wait for it to be processed
    AcpiOsWritePort(ec_context.Data, Address, 8);
    ec_wait_on(ACPI_EC_IBF);

    // Wait for the output buffer to be full
    ec_wait_on(ACPI_EC_OBF);
    AcpiOsReadPort(ec_context.Data, (uint32_t*)Store, 8);
}

static void acpi_ec_write(uint8_t Address, uint8_t Value)
{
    // Send Write (0x81) Command and Address to EC
    AcpiOsWritePort(ec_context.Control, WR_EC, 8);
    ec_wait_on(ACPI_EC_IBF);

    // Send the address, wait until it has been processed
    AcpiOsWritePort(ec_context.Data, Address, 8);
    ec_wait_on(ACPI_EC_IBF);

    // Write the data, wait until it has been committed
    AcpiOsWritePort(ec_context.Data, Value, 8);
    ec_wait_on(ACPI_EC_IBF);
}

static ACPI_STATUS acpi_ec_space_handler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address, UINT32 BitWidth, UINT64 *Value, void *HandlerContext, void *RegionContext)
{
    ACPI_STATUS status = AE_OK;

    switch (Function)
    {
        case ACPI_READ:
            acpi_ec_read((uint8_t)Address, Value);
            status = AE_OK;
            break;

        case ACPI_WRITE:
            acpi_ec_write((uint8_t)Address, (uint8_t)*Value);
            status = AE_OK;
            break;
    
        default:
            status = AE_BAD_PARAMETER;
    }

    klog_logln(LVL_DEBUG, "ECACC %s: %02x = %02x (%02x, %02x)", Function ? "WR" : "RD", Address, *Value, (uint8_t)ec_context.Control, (uint8_t)ec_context.Data);

    return status;
}

static ACPI_STATUS null_callback(ACPI_HANDLE Object, UINT32 NestingLevel, void *Context, void **ReturnValue)
{
    *ReturnValue = Object;
    return AE_CTRL_TERMINATE;
}


void acpi_ec_init()
{
    ACPI_STATUS status;
    ACPI_HANDLE ec_handle = NULL;
    ACPI_TABLE_ECDT* ecdt_table;
    ACPI_BUFFER ec_crs;

    klog_logln(LVL_DEBUG, "Initializing ACPI EC");

    // Procedure for finding embedded controller:
    // 1 - Use ECDT if it exists
    // 2 - Walk ACPI Namespace for an EC

    // Get ECDT Table
    ecdt_table = (ACPI_TABLE_ECDT*) acpi_get_table(ACPI_SIG_ECDT, 1);

    if(ecdt_table != NULL)
    {
        // Get EC device handle
        status = AcpiGetHandle(NULL, (ACPI_STRING)ecdt_table->Id, &ec_handle);
        if(ACPI_FAILURE(status))
        {
            klog_logln(LVL_ERROR, "Failed to get EC handle (%s)", AcpiFormatException(status));
            goto walk_namespace;
        }

        // Copy Context over
        ec_context.Control = ecdt_table->Control.Address;
        ec_context.Data = ecdt_table->Data.Address;
        ec_context.IsPortSpace = ecdt_table->Control.SpaceId == ACPI_ADR_SPACE_SYSTEM_IO;
        ec_context.GpeBit = ecdt_table->Gpe;

        acpi_put_table((ACPI_TABLE_HEADER*)ecdt_table);
        goto ec_resources_found;
    }

    walk_namespace:

    // Walk ACPI namespace to find EC
    status = AcpiGetDevices("PNP0C09", null_callback, NULL, &ec_handle);

    if(ec_handle == NULL)
        return;

    // Load EC_CONTEXT addresses from the EC CRS
    // Data port first, then command / status port
    ec_crs.Length = 0;

    retry:
    // Dynamically allocate the CRS
    status = AcpiGetCurrentResources(ec_handle, &ec_crs);
    if(ACPI_FAILURE(status) == AE_BUFFER_OVERFLOW)
    {
        ec_crs.Pointer = kmalloc(ec_crs.Length);
        goto retry;
    }
    else if(ACPI_FAILURE(status))
    {
        klog_logln(LVL_ERROR, "Failed to acquire the CRS for the ACPI EC (%s)", AcpiFormatException(status));
        goto ec_handle_failed;
    }

    // Parse CRS
    ACPI_RESOURCE* crs_res = ec_crs.Pointer;
    ACPI_RESOURCE_ADDRESS64 addr_res;
    size_t bytes = ec_crs.Length;
    size_t port_index = 2; // Current port index

    while(bytes)
    {
        switch(crs_res->Type)
        {
            case ACPI_RESOURCE_TYPE_IO:
            {
                uint16_t* ptr = (uint16_t*)((uintptr_t)&ec_context + (port_index - 1) * 8);
                *ptr = crs_res->Data.Io.Minimum;
                if(port_index == 2)
                    ec_context.Data = (unsigned long)crs_res->Data.Io.Minimum;
                else if(port_index == 1)
                    ec_context.Control = (unsigned long)crs_res->Data.Io.Minimum;
                klog_logln(LVL_DEBUG, "ECIsO: %p -> %p (%x), %ld", &ec_context.Data, ptr, *ptr, (uint32_t)port_index);
                port_index--;
                ec_context.IsPortSpace = true;
                break;
            }
            case ACPI_RESOURCE_TYPE_ADDRESS16:
            case ACPI_RESOURCE_TYPE_ADDRESS32:
            case ACPI_RESOURCE_TYPE_ADDRESS64:
                AcpiResourceToAddress64(crs_res, &addr_res);
                ((uint64_t*)&ec_context)[port_index - 1] = addr_res.Address.Minimum;
                port_index--;
                break;
            case ACPI_RESOURCE_TYPE_END_TAG:
                // Stop loop
                break;
        }

            klog_logln(LVL_DEBUG, "ECIO: %x, %x", (uint8_t)ec_context.Control, (uint8_t)ec_context.Data);
            if(!port_index)
                break;

            // Move to next resource
            bytes -= crs_res->Length;
            crs_res = (ACPI_RESOURCE*)((uintptr_t)crs_res + crs_res->Length);
        }

    // TODO: Acquire GPE from EC Device block

    ec_resources_found:
    // Add address space handler for Embedded Controller (EC)
    status = AcpiInstallAddressSpaceHandler(ec_handle, ACPI_ADR_SPACE_EC, acpi_ec_space_handler, NULL, NULL);
    if(ACPI_FAILURE(status))
    {
        klog_logln(LVL_ERROR, "Failed to install EC Address Space Handler (%s)", AcpiFormatException(status));
        goto ec_handle_failed;
    }

    klog_logln(LVL_INFO, "EC AS Handler Initialized (CMD/STS: 0x%02x, DAT: 0x%02x)", ec_context.Control, ec_context.Data);

    // TODO: Handle EC GPE Event (Last step preventing us from actually handling the EC)

    ec_handle_failed:
    kfree(ec_crs.Pointer);
}
