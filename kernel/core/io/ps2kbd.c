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
#define QUEUE_SIZE 32

// Decoding states
#define DECODE_IDLE      0
#define DECODE_WAIT_CODE 1
#define DECODE_PROCESS   2

// Data buffer
static uint8_t data_buffer[BUFFER_SIZE];
static uint16_t read_head = 0;
static uint16_t write_head = 0;

// Command queue
static uint16_t command_queue[QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;

static uint8_t response_tail = 0;
static uint8_t response_buffer[4];

// Associated device
static int kbd_device = 0;
static thread_t* decoder_thread;

// Decoding maps
static uint8_t keycode_map[] = { PS2_SET2_MAP };
static uint8_t unxlate[] = { 
    0x00, 0x76, 0x16, 0x1e, 0x26, 0x25, 0x2e, 0x36, 0x3d, 0x3e, 0x46, 0x45, 0x4e, 0x55, 0x66, 0x0d, 
    0x15, 0x1d, 0x24, 0x2d, 0x2c, 0x35, 0x3c, 0x43, 0x44, 0x4d, 0x54, 0x5b, 0x5a, 0x14, 0x1c, 0x1b, 
    0x23, 0x2b, 0x34, 0x33, 0x3b, 0x42, 0x4b, 0x4c, 0x52, 0x0e, 0x12, 0x5d, 0x1a, 0x22, 0x21, 0x2a, 
    0x32, 0x31, 0x3a, 0x41, 0x49, 0x4a, 0x59, 0x7c, 0x11, 0x29, 0x58, 0x05, 0x06, 0x04, 0x0c, 0x03, 
    0x0b, 0x02, 0x0a, 0x01, 0x09, 0x77, 0x7e, 0x6c, 0x75, 0x7d, 0x7b, 0x6b, 0x73, 0x74, 0x79, 0x69, 
    0x72, 0x7a, 0x70, 0x71, 0x7f, 0x60, 0x61, 0x78, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0x3f, 
    0x47, 0x4f, 0x56, 0x5e, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x57, 0x6f, 
    0x13, 0x19, 0x39, 0x51, 0x53, 0x5c, 0x5f, 0x62, 0x63, 0x64, 0x65, 0x67, 0x68, 0x6a, 0x6d, 0x6e, 
    0x80, 0x81, 0x82, 0x16, 0x6e, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0x00,
};

static bool translated = false;

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

static bool send_command(uint16_t command, uint16_t subcommand)
{
    // Enqueue command & subcommand

    if (queue_tail >= QUEUE_SIZE)
        queue_tail = 0;

    taskswitch_disable();
    command_queue[queue_tail++] = (subcommand << 8) | command;

    // Wake up event processor
    sched_unblock_thread(decoder_thread);
    taskswitch_enable();

    return true;
}

static uint16_t dequeue_command()
{
    if (queue_head == queue_tail)
        return 0xFFFF;
    if (queue_head >= QUEUE_SIZE)
        queue_head = 0;
    
    return command_queue[queue_head++];
}

static irq_ret_t ps2_keyboard_isr(struct irq_handler* handler)
{
    taskswitch_disable();
    uint8_t data = ps2_device_read(kbd_device, false);

    switch(data)
    {
    case 0xAA: // Self-Test pass
    case 0xEE: // Echo Response
    case 0xFA: // Command Ack
    case 0xFC: // Self-Test fail
    case 0xFD:
    case 0xFE: // Resend
    case 0xFF: // Key detect error
        if (response_tail < 4)
            response_buffer[response_tail++] = data;
    default:
        if (translated)
        {
            if (!(data == 0xE0 || data == 0xE1))
            {
                if (data & 0x80)
                    keycode_push(0xF0);
                keycode_push(unxlate[data & 0x7F]);
            }
            else
            {
                // Preserve 0xE0 & 0xE1
                keycode_push(data);
            }
        }
        else
        {
            keycode_push(data);
        }
        break;
    }
    sched_unblock_thread(decoder_thread);
    
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

    // Command processing state
    uint8_t command = 0xFF;
    uint8_t subcommand = 0xFF;
    bool send_subcmd = false;

    // Decoding State
    // Currently processed key code
    uint16_t keycode = 0;
    // Number of bytes to read before processing
    uint8_t read_count = 1;
    // If the key was released
    bool released = false;
    // If the key is an extended code
    bool extended = false;

    while(1)
    {
        // Process a command
        if (key_state_machine == DECODE_IDLE)
        {
            uint16_t queued_command = dequeue_command();
            if (queued_command != 0xFFFF)
            {
                command = (uint8_t)((queued_command >> 0) & 0xFF);
                subcommand = (uint8_t)((queued_command >> 8) & 0xFF);

                // The current system is not great, buf is functional
                // TODO: Rework so that key decoding can happen at the same time
                // Send the command and wait for a response
                response_buffer[0] = 0xFF;
                response_tail = 0;
                ps2_device_write(kbd_device, false, command);
                while (!response_tail)
                    sched_block_thread(STATE_SUSPENDED);
                if (response_buffer[0] != 0xFA)
                {
                    klog_logln(LVL_ERROR, "ATKBD: Command failed to send %02x:%02x (Code %02x)", command, subcommand, response_buffer[0]);
                    continue;
                }

                if (command == 0xF4)
                    continue;

                // Send the argument and wait for a response
                response_buffer[0] = 0xFF;
                response_tail = 0;
                ps2_device_write(kbd_device, false, subcommand);
                while (!response_tail)
                    sched_block_thread(STATE_SUSPENDED);
                if (response_buffer[0] != 0xFA)
                {
                    klog_logln(LVL_ERROR, "ATKBD: Argument failed to send %02x:%02x (Code %02x)", command, subcommand, response_buffer[0]);
                    continue;
                }
            }
        }

        keep_processing:
        data = keycode_pop();
        
        if(data == 0x00)
        {
            // Wait for more data
            sched_block_thread(STATE_SUSPENDED);
            continue;
        }

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

            // Weird keys to note (from scancode.doc):
            // When NumLock is on:  Shift code is  pressed for numpad
            // When NumLock is off: Shift code is released for numpad
            // Num_/: Inverts the shift state (regardless of numpad)

            // PrSc = E0 14 7C
            // Ctrl + PrSc = E0 7C
            // Alt + PrSc = 84 (SysReq)

            // Ctrl + Pause = 7E (Break)

            if (read_count > 0)
            {
                // Still more keys to go, do a thing
                key_state_machine = DECODE_WAIT_CODE;
                goto keep_processing;
            }

            // Compute the effective keycode
            // Pause
            if (extended && keycode == 0x1477)
                keycode = 0x77;

            if (extended)
                keycode += 0x80;

            // Update status lights
            if(kbd_handle_key(keycode_map[keycode], released))
                send_command(0xED, kbd_getmods() & 0x7);

            // Key processed, reset state
            released = false;
            extended = false;
            read_count = 1;
            keycode = 0;
            key_state_machine = DECODE_IDLE;
            continue;
        }
    }
}

void ps2kbd_init(int device, bool xlated)
{
    translated = xlated;

    // PS2 Side
    kbd_device = device;
    decoder_thread = thread_create(sched_active_process(), keycode_decoder, PRIORITY_KERNEL, "keydecoder_ps2", NULL);

    memset(command_queue, 0xFF, QUEUE_SIZE*2);
    queue_head = 0;
    queue_tail = 0;

    klog_logln(LVL_DEBUG, "PS2: Clearing buffer");
    memset(data_buffer, 0x00, BUFFER_SIZE);

    ps2_handle_device(kbd_device, ps2_keyboard_isr);

    if(!translated || ps2_device_get_type(device) == TYPE_MF2_KBD_TRANS)
        // Force the keyboard into scancode set 2 (preventing double translation)
        send_command(0xF0, 0x02);
    else
        // Normal AT keyboard
        send_command(0xF0, 0x01);

    // Repeat rate @ 2hz
    send_command(0xF3, 0x2B);
    // Reset lights
    send_command(0xED, 0x00);
    // Enable scanning
    send_command(0xF4, 0x00);

    // Reset heads
    read_head = 0;
    write_head = 0;
}
