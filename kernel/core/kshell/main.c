#include <string.h>
#include <stdio.h>
#include <keycodes.h>
#include <kbd.h>
#include <tty.h>
#include <mm.h>
#include <ctype.h>
#include <sched.h>
#include <fb.h>
#include <mb2parse.h>
#include <kshell/kshell.h>

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

extern void request_refresh();

static char* input_buffer = KNULL;
static int index = 0;
static bool input_done = false;
static bool should_run = true;
static uint8_t shell_text_clr = 0x7;
static uint8_t shell_bg_clr = 0x0;
bool refresh_needed = false;

void refresh_task()
{
	while(1)
	{
		if(refresh_needed)
		{
			if(tty_background_dirty())
			{
				if(fb_info.type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
				{
					fb_fillrect(get_fb_address(), 0, 0, fb_info.width, fb_info.height, 0);
				} else
				{
					for(int i = 0; i < fb_info.width * fb_info.height; i++)
						((uint16_t*)get_fb_address())[i] = 0x0700;
				}
				tty_make_clean();
			}
			tty_reshow();
			refresh_needed = false;
		}
		sched_switch_thread();
	}
}

void request_refresh()
{
	refresh_needed = true;
}

size_t strspn(const char* str, const char* delim)
{
	size_t count = 0;

	while(*str)
	{
		const char *c;
		for(c = delim; *c; c++)
		{
			if(*str == *c)
			{
				count++;
				break;
			}
		}
		if(*c == '\0') return count;

		str++;
	}

	return count;
}

char* strtok_r(char* str, const char* delim, char** lasts)
{
	char* token;

	if(str == NULL)
		str = *lasts;

	str += strspn(str, delim);

	if(*str == '\0')
	{
		*lasts = str;
		return NULL;
	}
	token = str++;

	while(*str)
	{
		for(const char *c = delim; *c; c++)
		{
			if(*str == *c)
				goto tokend;
		}
		str++;
	}

	*lasts = str;
	goto done;

	tokend:
	*str = '\0';
	*lasts = str + 1;

	done:
	return token;
}

static void shell_readline()
{
	printf("> ");
	input_done = false;
	index = 0;
	request_refresh();

	while(1)
	{
		char kcode = kbd_read();
		char kchr = kbd_tochar(kcode);

		if(kcode)
			request_refresh();

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
	char* command = strtok_r(input_buffer, " ", &saveptr);

	if(is_command("echo", command))
	{
		printf("%s\n", strspn(saveptr, " ") + saveptr);
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
		puts("Kshell version 0.2\n");
		puts("Available commands (slashes = aliases, square brackets = arguments):");
		puts("\thelp/?:          \tShows this information");
		puts("\thelloworld/hw:   \tShows an example string");
		puts("\tclear/cls:       \tClears the screen");
		puts("\techo [thistext]: \tShows [thistext]");
		puts("\texit:            \tExits the console (requires reboot to bring back shell)");
		return true;
	} else if(is_command("exit", command))
	{
		should_run = false;
		return true;
	}

	return false;
}

void kshell_main()
{
	/*
	 * The refresh thread is on the same priority as kshell as there isn't
	 * conditional wakeups yet.
	 */
	thread_t* refresh_thread = thread_create(
		sched_active_process(),
		(uint64_t*)refresh_task,
		PRIORITY_NORMAL);

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
		request_refresh();
	}

	puts("kshell is exiting, nothing left to do");
	sched_set_thread_state(refresh_thread, STATE_EXITED);
	sched_set_thread_state(sched_active_thread(), STATE_EXITED);
}
