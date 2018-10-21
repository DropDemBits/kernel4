#ifndef __ACPI_OSL_H__
#define __ACPI_OSL_H__ 1

#include <acpi.h>

#define ACPI_MAX_EARLY_TABLES 16

/**
 * @brief  Initializes the early parts of the ACPI subsystem (ie table traversal)
 * @note   
 * @retval AE_OK if everything was initialized properly
 */
ACPI_STATUS acpi_early_init();

/**
 * @brief  Gets the early copy of a table
 * @note   
 * @param  idx: The index into the early table array
 * @retval The pointer to the early table, or NULL if it doesn't exist
 */
ACPI_TABLE_HEADER* acpi_early_get_table(uint32_t idx);

/**
 * @brief  Initalizes the full ACPI subsystem
 * @note   
 * @retval AE_OK if everything was initialized properly
 */
ACPI_STATUS acpi_init();

uint16_t acpi_subsys_id();

#endif /* __ACPI_OSL_H__ */
