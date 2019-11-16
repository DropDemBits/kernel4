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

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <common/hal.h>
#include <common/io/kbd.h>
#include <common/io/ps2.h>
#include <common/io/keycodes.h>
#include <common/sched/sched.h>
#include <common/util/kfuncs.h>

#define MOD_SCROLL_LOCK 0x01
#define MOD_NUM_LOCK 0x02
#define MOD_CAPS_LOCK 0x04
#define MOD_SHIFT 0x80

#define BUFFER_SIZE 4096

// Decoding states
#define DECODE_IDLE      0
#define DECODE_WAIT_CODE 1
#define DECODE_PROCESS   2

static uint8_t data_buffer[BUFFER_SIZE];
static uint16_t read_head = 0;
static uint16_t write_head = 0;
static int kbd_device = 0;
static thread_t* decoder_thread;

static uint8_t keycode_map[] = {PS2_SET2_MAP};
static bool command_successful = false;

static void keycode_push(uint8_t data)
{
    if(write_head >= BUFFER_SIZE) write_head = 0;
    data_buffer[write_head++] = data;
}

static uint8_t keycode_pop()
{
    if(read_head == write_head) return 0x00;
    else if(read_head >= BUFFER_SIZE) read_head = 0;

    return data_buffer[read_head++];
}

static bool send_command(uint8_t command, uint8_t subcommand)
{
    // TODO: Rework to fit with current system of interrupts
    uint8_t rdVal = 0;

    command_successful = false;
    ps2_device_write(kbd_device, true, command);
    rdVal = ps2_device_read(kbd_device, true);
    if(!command_successful && rdVal != 0xFA) return false;
    command_successful = false;

    rdVal = ps2_device_read(kbd_device, true);
    if(!command_successful && rdVal != 0xFA) return false;
    command_successful = false;
    return true;
}

static irq_ret_t ps2_keyboard_isr(struct irq_handler* handler)
{
    taskswitch_disable();
    uint8_t data = ps2_device_read(kbd_device, false);

    switch(data)
    {
        case 0xFA:
            // Decoder thread isn't woken up by this...
            // Why not integrate command processing into the queue?
            command_successful = true;
            break;
        default:
            keycode_push(data);
            sched_unblock_thread(decoder_thread);
            break;
    }
    
    taskswitch_enable();
    return IRQ_HANDLED;
}

static void keycode_decoder()
{
    uint8_t data = 0;
    // State machine
    // Current state of the decoder machine:
    // 0: IDLE       (Doing nothing)
    // 1: WAIT_CODE  (Waiting for a key code)
    // 2: PROCESS    (Processing key)
    uint8_t key_state_machine = DECODE_IDLE;
    // Currently processed key code
    uint16_t keycode = 0;
    // Number of bytes to read before processing
    uint8_t read_count = 1;
    // If the key was released
    bool released = false;
    // If the key is an extended code
    bool extended = false;
    // If there is a pending status light update
    bool pending_lights_update = false;

    while(1)
    {
        keep_processing:
        data = keycode_pop();
        
        if(data == 0x00)
        {
            // Wait for more data
            sched_block_thread(STATE_SUSPENDED);
            continue;
        }

        // Deal with the current data byte
        if (key_state_machine == DECODE_IDLE || key_state_machine == DECODE_WAIT_CODE)
        {
            switch (data)
            {
            case 0xF0:
                // Break code
                // Keep read count the same
                released = true;
                // Keep extended state the same
                key_state_machine = DECODE_WAIT_CODE;
                break;
            case 0xE0:
                // Extended 1 code
                read_count = 1;
                // Keep released the same
                extended = true;
                key_state_machine = DECODE_WAIT_CODE;
                break;
            case 0xE1:
                // Extended 2 code
                read_count = 2;
                // Keep released the same
                extended = true;
                key_state_machine = DECODE_WAIT_CODE;
                break;
            default:
                // Data code, can process now
                --read_count;
                // Keep released the same
                // Keep extended the same
                key_state_machine = DECODE_PROCESS;
                break;
            }

            // If more data needs to be accepted, keep going
            if (key_state_machine == DECODE_WAIT_CODE)
                goto keep_processing;
        }

        if (key_state_machine == DECODE_PROCESS)
        {
            // Process the current key code

            // Merge current code into current keycode
            keycode <<= 8;
            keycode |= data;

            // Check for special cases of continues
            // PrintScr Press
            if (extended && !released && data == 0x12)
                read_count = 1;
            // PrintScr Release
            if (extended && released && data == 0x7C)
                read_count = 1;

            if (read_count > 0)
            {
                // Still more keys to go, do a thing
                key_state_machine = DECODE_WAIT_CODE;
                goto keep_processing;
            }

            // Compute the effective keycode
            // Print screen
            if (keycode == 0x7C12 || keycode == 0x127C)
                keycode = 0x7C;
            if (keycode == 0x1477)
                keycode = 0x77;

            if (extended)
                keycode += 0x80;
            
            pending_lights_update = kbd_handle_key(keycode_map[keycode], released);

            // Key processed, reset state
            released = false;
            extended = false;
            read_count = 1;
            keycode = 0;
            key_state_machine = DECODE_IDLE;
            goto keep_processing;
        }
    }
}

void ps2kbd_init(int device)
{
    // TODO: Merge ps2kbd & atkbd
    // Only difference is xlation map

    // PS2 Side
    kbd_device = device;
    decoder_thread = thread_create(sched_active_process(), keycode_decoder, PRIORITY_KERNEL, "keydecoder_ps2", NULL);

    if(!send_command(0xF4, 0x00))
        klog_logln(LVL_INFO, "MF2 Scanning enable failed");

    klog_logln(LVL_DEBUG, "MF2: Clearing buffer");
    memset(data_buffer, 0x00, BUFFER_SIZE);

    ps2_handle_device(kbd_device, ps2_keyboard_isr);
    // Set scan set to Set 2
    send_command(0xF0, 0x02);
    // Repeat rate @ 2hz
    send_command(0xF3, 0x2B);
    // Reset lights
    send_command(0xED, 0x00);

    // Reset heads
    read_head = 0;
    write_head = 0;
}
