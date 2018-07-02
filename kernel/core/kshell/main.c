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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <common/keycodes.h>
#include <common/kbd.h>
#include <common/tty.h>
#include <common/mm.h>
#include <common/sched.h>
#include <common/fb.h>
#include <common/mb2parse.h>
#include <common/kshell/kshell.h>

/*
 * KSHELL: Basic command line interface
 *
 * Available commands:
 * help (?): Prints help information
 * helloworld (hw): Prints helloworld in various colours V
 * echo: Echos following command arguments V
 * clear (cls): Clears screen V
 */

#define INPUT_SIZE 255
#define ARG_DELIM (" \t")

static char* input_buffer = KNULL;
static int index = 0;
static bool input_done = false;
static bool should_run = true;
static uint8_t shell_text_clr = 0x7;
static uint8_t shell_bg_clr = 0x0;
static thread_t *test_wakeup = KNULL;
static thread_t *refresh_thread = KNULL;

void wakeup_task()
{
	while(1)
	{
		puts("I'M AWAKE NOW");
		sched_set_thread_state(test_wakeup, STATE_SLEEPING);
	}
}

void refresh_task()
{
	while(1)
	{
		if(tty_background_dirty())
		{
			if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
			{
				fb_clear();
			} else
			{
				for(int i = 0; i < fb_info.width * fb_info.height; i++)
					((uint16_t*)get_fb_address())[i] = 0x0700;
			}
		}
		tty_reshow();
		tty_make_clean();
		sched_set_thread_state(sched_active_thread(), STATE_SLEEPING);
	}
}

static void shell_readline()
{
	printf("> ");
	input_done = false;
	index = 0;

	while(1)
	{
		char kcode = kbd_read();
		char kchr = kbd_tochar(kcode);

		if(kcode && kchr)
		{
			if((kchr != '\b' || index > 0) && (index < INPUT_SIZE || kchr == '\n'))
				putchar(kchr);

			if(kchr == '\n')
			{
				input_done = true;
				break;
			} else if(kchr == '\b' && index > 0)
			{
				input_buffer[--index] = '\0';
				continue;
			}

			if(kchr != '\b' && index < INPUT_SIZE)
			{
				input_buffer[index++] = kchr;
				input_buffer[index] = '\0';
			}
		}
	}
}

static bool is_command(const char* str, char* buffer)
{
	return strcmp(str,buffer) == 0;
}

static bool shell_parse()
{
	if(!index) return true; // Nothing to parse

	char* saveptr;
	char* command = strtok_r(input_buffer, ARG_DELIM, &saveptr);
	if(command == NULL) return true; // Nothing to parse

	if(is_command("echo", command))
	{
		printf("%s\n", strspn(saveptr, ARG_DELIM) + saveptr);
		return true;
	} else if(is_command("clear", command) || is_command("cls", command))
	{
		tty_clear();
		return true;
	} else if(is_command("helloworld", command) || is_command("hw", command))
	{
		const char* string = "Hello, World! I am a test statement...\n";
		int clrindex = 0;

		for(int i = 0; i < strlen(string); i++)
		{
			if(string[i] != '\n')
			{
				tty_set_colour(clrindex, ~(clrindex));
				clrindex++;
				clrindex &= 0xF;
			} else
			{
				tty_set_colour(shell_text_clr, shell_bg_clr);
			}
			putchar(string[i]);
		}

		return true;
	} else if(is_command("help", command) || is_command("?", command))
	{
		puts("Kshell version 0.3\n");
		puts("Available commands (slashes = aliases, square brackets = arguments):");
		puts("\thelp/?:          \tShows this information");
		puts("\thelloworld/hw:   \tShows an example string");
		puts("\tclear/cls:       \tClears the screen");
		puts("\techo [thistext]: \tShows [thistext]");
		puts("\texit:            \tExits the console (reboot to bring back shell)");
		puts("\tfonttest:        \tShows all charachters supported by the current font");
		puts("\twakeup:          \tWake up a test thread to show a string");
		puts("\tsleep [time]:    \tSleeps the shell for [time] milliseconds");
		return true;
	} else if(is_command("exit", command))
	{
		should_run = false;
		return true;
	} else if(is_command("fonttest", command))
	{
		for(unsigned int i = 0; i < 256; i++)
		{
			if(i == '\t' || i == '\n' || i == '\r' || i == '\b') putchar('\0');
			else putchar(i);

			if(i % 16 == 15) putchar('\n');
		}
		return true;
	} else if(is_command("wakeup", command))
	{
		if(test_wakeup != KNULL)
			sched_set_thread_state(test_wakeup, STATE_RUNNING);
		return true;
	} else if(is_command("sleep", command))
	{
		char* sleep_time = strtok_r(NULL, ARG_DELIM, &saveptr);
		long int sleep_for = 0;
		
		if(sleep_time != NULL) 
			sleep_for = atol(sleep_time);

		if(sleep_for < 0)
		{
			puts("sleep: Sleep time can't be negative!");

			// Parsing was correct, but the args weren't
			return true;
		}

		printf("Sleeping for %ld milliseconds\n", sleep_for);
		sched_sleep_millis(sleep_for);
		return true;
	}

	return false;
}

void kshell_main()
{
	refresh_thread = thread_create(
		sched_active_process(),
		(uint64_t*)refresh_task,
		PRIORITY_HIGHER,
		"refresh_thread");

	test_wakeup = thread_create(
		sched_active_process(),
		(uint64_t*)wakeup_task,
		PRIORITY_NORMAL,
		"test_wakeup");
	sched_set_thread_state(test_wakeup, STATE_SLEEPING);

	tty_set_colour(0xF, 0x0);
	printf("Welcome to K4!\n");
	tty_set_colour(0x7, 0x0);
	input_buffer = kmalloc(INPUT_SIZE+1);
	memset(input_buffer, 0x00, INPUT_SIZE+1);

	while(should_run)
	{
		shell_readline();
		if(!shell_parse())
		{
			tty_set_colour(0xC, 0x0);
			printf("%s: command not found\n", input_buffer);
			puts("Try 'help' or '?' for a list of available commands");
			tty_set_colour(shell_text_clr, shell_bg_clr);
		}
	}

	puts("kshell is exiting, nothing left to do");
	sched_set_thread_state(refresh_thread, STATE_EXITED);
	sched_set_thread_state(sched_active_thread(), STATE_EXITED);
}
