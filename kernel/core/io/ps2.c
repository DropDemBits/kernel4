/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <stdio.h>

#include <common/sched/sched.h>
#include <common/io/ps2.h>
#include <common/util/kfuncs.h>

#define STATUS_DATA_READY 0x01
#define STATUS_WRITEREADY 0x02
/* Corresponds to 10ms */
#define DEFAULT_TIMEOUT 10

#define WAIT_TIMEOUT_CHECK(condition, timeout, timedout) \
do { \
    int cval = (timeout); \
    do { \
        sched_sleep_ms(1); \
    } while((condition) && --cval); \
    if(!cval) (timedout) = true; \
} while(0); \

#define WAIT_TIMEOUT(condition, timeout) \
do { \
    int cval = (timeout); \
    do { \
        sched_sleep_ms(1); \
    } while((condition) && --cval); \
} while(0); \

/*
   External methods

   extern void ps2_wait();
   extern void ps2_controller_writeb(uint8_t data);
   extern void ps2_device_writeb(uint8_t data);
   extern uint8_t ps2_read_status();
   extern uint8_t ps2_read_data();
   extern uint8_t* ps2_device_irqs();
 */

extern void ps2kbd_init(int device);
extern void atkbd_init(int device);
//extern void ps2mouse_init(int device);

bool controller_exists = true;
static const char* type2name[] = {
    "None",
    "AT Keyboard",
    "MF2 Keyboard",
    "Translated MF2 Keyboard",
    "2-button Mouse",
    "3-button Mouse",
    "5-button Mouse"
};
struct ps2_device devices[2];

static void send_controller_command(uint8_t command)
{
    WAIT_TIMEOUT((ps2_read_status() & STATUS_WRITEREADY), DEFAULT_TIMEOUT)
    ps2_controller_writeb(command);
}

static void send_dev_command(int device, uint8_t command)
{
    ps2_device_write(device, true, command);
    if(ps2_device_read(device, true) != 0xFA) return;
}

static void wait_write_data(uint8_t data)
{
    WAIT_TIMEOUT((ps2_read_status() & STATUS_WRITEREADY), DEFAULT_TIMEOUT)
    ps2_device_writeb(data);
}

static uint8_t wait_read_data()
{
    WAIT_TIMEOUT((ps2_read_status() & STATUS_DATA_READY) == 0, DEFAULT_TIMEOUT)
    return ps2_read_data();
}

static uint8_t wait_read_data_timeout()
{
    bool timedout = false;
    WAIT_TIMEOUT_CHECK((ps2_read_status() & STATUS_DATA_READY) == 0, DEFAULT_TIMEOUT, timedout)
    if(timedout)
        return 0xFF;
    return ps2_read_data();
}

static void controller_init()
{
    // Temporary until we can read ACPI Tables
    if(ps2_read_status() == 0xFF)
    {
        controller_exists = false;
        return;
    }

    uint8_t usable_bitmask = 0b01;
    bool modify_cfg = true;

    // Nom on bytes
    while(ps2_read_status() & STATUS_DATA_READY)
    {
        ps2_read_data();
        busy_wait();
    }

    // Disable devices
    send_controller_command(0xAD); // Device 1
    send_controller_command(0xA7); // Device 2

    // Clear output buffer
    while(ps2_read_status() & STATUS_DATA_READY)
    {
        ps2_read_data();
        busy_wait();
    }

    // Set configuration byte
    send_controller_command(0x20);
    uint8_t cfg_byte = wait_read_data();
    cfg_byte &= 0b10111100;
    send_controller_command(0x60);
    wait_write_data(cfg_byte);
    if(cfg_byte & 0x20) usable_bitmask |= 0b10;

    // Check for poor PS/2 controller emulation
    /*send_controller_command(0x20);
    if(wait_read_data() != cfg_byte)
    {
        klog_logln(LVL_INFO, "Unable to change config byte");
        modify_cfg = false;
        goto device_init;
    }*/

    // Perform self test
    send_controller_command(0xAA);
    uint8_t ret_byte = wait_read_data();
    if(ret_byte == 0xFC)
    {
        klog_logln(LVL_INFO, "Self test failed (code %#x)", ret_byte);
        return;
    }
    klog_logln(LVL_INFO, "Self test success (code %#x)", ret_byte);

    // Test ports
    device_init:
    send_controller_command(0xAB);
    uint8_t retval = wait_read_data();
    if(retval != 0x00)
    {
        usable_bitmask &= ~0b01;
        klog_logln(LVL_INFO, "Unable to use device on port 1 (code %#x)\n", retval);
    }

    if(usable_bitmask & 0b10)
    {
        send_controller_command(0xA9);
        uint8_t retval = wait_read_data();
        if(retval != 0x00)
        {
            usable_bitmask &= ~0b10;
            klog_logln(LVL_INFO, "Unable to use device on port 2 (code %#x)\n", retval);
        }
    }

    if(usable_bitmask == 0)
    {
        klog_logln(LVL_INFO, "No usable devices");
        return;
    }

    if(usable_bitmask & 0b01)
    {
        // Reset device on port 1
        ps2_device_write(0, true, 0xFF);
        uint8_t reset_result = ps2_device_read(0, true);

        if(reset_result == 0xFC)
            usable_bitmask &= ~0b01;
        else if(reset_result == 0x00)
            devices[0].present = 1;
        else
            devices[0].present = 0;

        if(devices[0].present)
            klog_logln(LVL_DEBUG, "Detected PS/2 device on port 1 (code %#x)", reset_result);
        else
            klog_logln(LVL_DEBUG, "No device on port 1 (code %#x)", reset_result);
    }

    if(usable_bitmask & 0b10)
    {
        // Reset device on port 2
        ps2_device_write(1, true, 0xFF);
        uint8_t reset_result = ps2_device_read(1, true);

        if(reset_result == 0xFC)
            usable_bitmask &= ~0b10;
        else if(reset_result == 0x00)
            devices[1].present = 1;
        else
            devices[1].present = 0;

        if(devices[1].present)
            klog_logln(LVL_DEBUG, "Detected PS/2 device on port 2 (code %#x)", reset_result);
        else
            klog_logln(LVL_DEBUG, "No device on port 2 (code %#x)", reset_result);
    }

    if(!modify_cfg) return;

    // Commit changes
    send_controller_command(0x20);
    cfg_byte = wait_read_data();
    cfg_byte |= usable_bitmask;
    send_controller_command(0x60);
    wait_write_data(cfg_byte);

}

static void detect_device(int device)
{
    // Disable Scanning
    while(ps2_read_status() & STATUS_DATA_READY)
        ps2_read_data();
    send_dev_command(device, 0xF5);
    while(ps2_read_status() & STATUS_DATA_READY)
        ps2_read_data();

    if(device >= 2 || device < 0 || !devices[device].present)
    {
        devices[device].type = TYPE_NONE;
        return;
    }

    uint16_t first_byte = 0xFF, second_byte = 0xFF;
    klog_logln(LVL_DEBUG, "Identifying device on port %d", device + 1);
    while(ps2_read_status() & STATUS_DATA_READY)
        ps2_read_data();
    send_dev_command(device, 0xF2);
    first_byte = wait_read_data_timeout();
    second_byte = wait_read_data_timeout();

         if(first_byte == 0xFA && second_byte == 0xFF) devices[device].type = TYPE_AT_KBD;
    else if(first_byte == 0xFF && second_byte == 0xFF) devices[device].type = TYPE_AT_KBD;
    else if(first_byte == 0x00 && second_byte == 0xFF) devices[device].type = TYPE_2B_MOUSE;
    else if(first_byte == 0x03 && second_byte == 0xFF) devices[device].type = TYPE_3B_MOUSE;
    else if(first_byte == 0x04 && second_byte == 0xFF) devices[device].type = TYPE_5B_MOUSE;
    else if(first_byte == 0xAB && second_byte == 0x41) devices[device].type = TYPE_MF2_KBD_TRANS;
    else if(first_byte == 0xAB && second_byte == 0xC1) devices[device].type = TYPE_MF2_KBD_TRANS;
    else if(first_byte == 0xAB && second_byte == 0x83) devices[device].type = TYPE_MF2_KBD;
    else klog_logln(LVL_DEBUG, "Identified unknown device on port %d: %#x %#x", device+1, first_byte, second_byte);
    klog_logln(LVL_DEBUG, "SigBytes(%d): %#x %#x", device+1, first_byte, second_byte);

    // The following combination isn't possible
    if((devices[device].type == TYPE_MF2_KBD_TRANS || devices[device].type == TYPE_AT_KBD) &&
        device == 1)
    {
        devices[device].type = TYPE_NONE;
        devices[device].present = 0;
    }

    if(device == 0) send_controller_command(0xAE);
    else if(device == 1) send_controller_command(0xA8);
}

void ps2_init()
{
    klog_logln(LVL_INFO, "Initialising PS/2 controller");
    controller_init();
    detect_device(0);
    detect_device(1);

    // Initialize devices
    for(int active_device = 0; active_device < 2; active_device++)
    {
        if(!devices[active_device].present || !devices[active_device].type) continue;

        if(devices[active_device].type == TYPE_MF2_KBD)
        {
            klog_logln(LVL_DEBUG, "Initialising MF2 keyboard");
            ps2kbd_init(active_device);
        } else if(devices[active_device].type == TYPE_MF2_KBD_TRANS ||
                devices[active_device].type == TYPE_AT_KBD)
        {
            if(devices[active_device].type == TYPE_MF2_KBD_TRANS)
                klog_logln(LVL_DEBUG, "Initialising AT Translated MF2 keyboard Scan Set 2");
            else
                klog_logln(LVL_DEBUG, "Initialising AT keyboard Scan Set 1");
            atkbd_init(active_device);
        }
        else
            klog_logln(LVL_DEBUG, "Device on port %d not initialized (%s)",
                active_device+1,
                type2name[devices[active_device].type]);
    }
}

void ps2_handle_device(int device, irq_function_t handler)
{
    if(device >= 2 || device < 0 || !devices[device].present) return;
    ic_irq_handle(ps2_device_irqs()[device], INT_EOI_FAST | INT_SRC_LEGACY, handler);
}

uint8_t ps2_device_read(int device, bool wait_for)
{
    if(device >= 2 || device < 0 || !devices[device].present) return 0x00;

    if(wait_for) return wait_read_data();
    else return ps2_read_data();
}

void ps2_device_write(int device, bool wait_for, uint8_t data)
{
    if(device >= 2 || device < 0 || !devices[device].present) return;
    else if(device == 1) ps2_controller_writeb(0xD4);

    if(wait_for) wait_write_data(data);
    else ps2_device_writeb(data);
}

enum ps2_devtype ps2_device_get_type(int device)
{
    if(device >= 2 || device < 0) return TYPE_NONE;
    return devices[device].type;
}

void ps2_controller_set_exist(bool exist_state)
{
    controller_exists = exist_state;
}

bool ps2_controller_exists()
{
    return controller_exists;
}
