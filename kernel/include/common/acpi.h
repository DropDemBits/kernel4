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
 * @brief  Gets an ACPI table with the specified signature and instance
 *         The pointer returned should be put back using acpi_put_table()
 * @note   For tables that don't have multiple instances (not SSDT), instance should be 1
 *         This version ignores errors. For input validation, use acpi_get_table()
 * @param  sig: The signature of the table to find
 * @param  instance: The instance of the table to use. 1 based
 * @retval The pointer to the ACPI table, or NULL if an error occured
 */
ACPI_TABLE_HEADER* acpi_early_get_table(char* sig, uint32_t instance);

/**
 * @brief  Gets an ACPI table with the specified signature and instance
 *         The pointer returned should be put back using acpi_put_table()
 * @note   For tables that don't have multiple instances (not SSDT), instance should be 1
 * @param  sig: The signature of the table to find
 * @param  instance: The instance of the table to use. 1 based
 * @retval The pointer to the ACPI table, or NULL if an error occured
 */
ACPI_TABLE_HEADER* acpi_get_table(char* sig, uint32_t instance);

/**
 * @brief  Puts a table back into the ACPI subsystem
 * @note   
 * @param  table: The table to put back
 * @retval None
 */
void acpi_put_table(ACPI_TABLE_HEADER* table);

void acpi_get_pirt();

/**
 * @brief  Enters the specifed sleep state
 * @note   
 * @param  sleep_state: The sleep state to enter
 * @retval AE_OK if sleep was entered successfully, or the appropriate error code
 */
ACPI_STATUS acpi_enter_sleep(uint8_t sleep_state);

/**
 * @brief  Leaves from the last sleep state
 * @note   
 * @param  sleep_state: The sleep state to leave
 * @retval AE_OK if sleep was left successfully, or the appropriate error code
 */
ACPI_STATUS acpi_leave_sleep(uint8_t sleep_state);

/**
 * @brief  Initalizes the full ACPI subsystem
 * @note   
 * @retval AE_OK if everything was initialized properly
 */
ACPI_STATUS acpi_init();

uint16_t acpi_subsys_id();

#endif /* __ACPI_OSL_H__ */
