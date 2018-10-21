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

extern uint16_t kbd_subsytem_id();

static uint8_t keycode_buffer[4096];
static uint16_t read_head = 0;
static uint16_t write_head = 0;
static int kbd_device = 0;
static thread_t* decoder_thread;
/*
 * 0: xF0 Flag (Release)
 * 1: xE0 Flag
 * 2: xE1 Flag
 * 3: Finish Flag
 */
static uint8_t key_state_machine = 0;
static uint8_t ps2set2_translation[] = {PS2_SET2_MAP};
static bool command_successful = false;

static void keycode_push(uint8_t keycode)
{
    if(write_head >= 4096) write_head = 0;
    keycode_buffer[write_head++] = keycode;
}

static uint8_t keycode_pop()
{
    if(read_head == write_head) return 0x00;
    else if(read_head >= 4096) read_head = 0;
    return keycode_buffer[read_head++];
}

static bool send_command(uint8_t command, uint8_t subcommand)
{
    command_successful = false;
    ps2_device_write(kbd_device, true, command);
    if(!command_successful && ps2_device_read(kbd_device, true) != 0xFA) return false;
    command_successful = false;

    ps2_device_write(kbd_device, true, subcommand);
    if(!command_successful && ps2_device_read(kbd_device, true) != 0xFA) return false;
    command_successful = false;
    return true;
}

static irq_ret_t ps2_keyboard_isr(struct irq_handler* handler)
{
    taskswitch_disable();
    uint8_t data = ps2_device_read(kbd_device, false);
    
    if(data < 0xF0)
    {
        insert_data:
        keycode_push(data);
        sched_unblock_thread(decoder_thread);
    } else
    {
        switch(data)
        {
            case 0xF0:
                goto insert_data;
                break;
            case 0xFA:
                command_successful = true;
                break;
            default:
                break;
        }
    }
    
    taskswitch_enable();
    return IRQ_HANDLED;
}

static void keycode_decoder()
{
    uint8_t data = 0;
    
    while(1)
    {
        keep_consume:
        data = keycode_pop();
        
        if(data == 0x00)
        {
            sched_block_thread(STATE_SUSPENDED);
            goto keep_consume;
        }

        switch(data)
        {
            case 0xF0: key_state_machine |= 0b0001; break;
            case 0xE0: key_state_machine |= 0b0010; break;
            case 0xE1: key_state_machine |= 0b0100; break;
            case 0x77:
                if(key_state_machine == 0b0101)
                {
                    key_state_machine |= 0b1000;
                    break;
                }
            case 0x7C:
                if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 0)
                {
                    key_state_machine |= 0b1000;
                    break;
                } else if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 1)
                {
                    key_state_machine &= ~0b1011;
                    break;
                }
            case 0x12:
                if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 1)
                {
                    key_state_machine |= 0b1000;
                    data = 0x7C;
                    break;
                }
                else if((key_state_machine & 0b0010) && (key_state_machine & 0b0001) == 0)
                {
                    key_state_machine &= ~0b1011;
                    break;
                }
            default:
                if((key_state_machine & 0b0100) == 0)
                    key_state_machine |= 0b1000;
                break;
        }
        if((key_state_machine & 0b1000) == 0) goto keep_consume;

        uint8_t keycode = KEY_RESERVED;
        if(key_state_machine & 0b0110)
            keycode = ps2set2_translation[data + 0x80];
        else
            keycode = ps2set2_translation[data + 0x00];

        if(kbd_handle_key(keycode, key_state_machine & 0x1))
            send_command(0xED, kbd_getmods() & 0x7);
        key_state_machine = 0;
    }
}

void ps2kbd_init(int device)
{
    // PS2 Side
    kbd_device = device;
    decoder_thread = thread_create(sched_active_process(), keycode_decoder, PRIORITY_KERNEL, "keydecoder_ps2", NULL);

    if(!send_command(0xF4, 0x00))
        klog_logln(kbd_subsytem_id(), INFO, "MF2 Scanning enable failed");

    klog_logln(kbd_subsytem_id(), DEBUG, "MF2: Clearing buffer");
    memset(keycode_buffer, 0x00, 4096);

    ps2_handle_device(kbd_device, ps2_keyboard_isr);
    send_command(0xF0, 0x02);
    send_command(0xF3, 0x2B);
    send_command(0xED, 0x00);

    // Reset heads
    read_head = 0;
    write_head = 0;
}
