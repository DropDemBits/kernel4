TARGET_ARCH?=
CC?=
AR?=
SYSROOT?=
PREFIX?=/usr/local

ARCH_INCLUDE_DIR?=
ARCH_CFLAGS?=
ARCH_CPPFLAGS?=

BUILD_OBJ=
CFLAGS=-Wall -Wextra
CPPFLAGS= -I$(SYSROOT)$(PREFIX)/include

include ./arch/$(TARGET_ARCH)/make.config

CFLAGS += $(ARCH_CFLAGS)
CPPFLAGS += $(ARCH_CPPFLAGS) -I$(ARCH_INCLUDE_DIR)
LIBK_CFLAGS=$(CFLAGS) -ffreestanding -fbuiltin
LIBK_CPPFLAGS=$(CPPFLAGS) -Iinclude

BUILD_OBJ:= $(BUILD_OBJ) \
stdio/printf.o \
stdio/sprintf.o \
stdio/vprintf.o \
stdio/vsprintf.o \
stdio/putchar.o \
stdio/puts.o \
stdlib/abort.o \
stdlib/exit.o \
stdlib/itoa.o \
stdlib/lltoa.o \
stdlib/ulltoa.o \
stdlib/atoi.o \
stdlib/atol.o \
stdlib/strtoul.o \
string/memcmp.o \
string/memcpy.o \
string/memmove.o \
string/memset.o \
string/strcmp.o \
string/strncmp.o \
string/strcpy.o \
string/strncpy.o \
string/stricmp.o \
string/strnicmp.o \
string/strlen.o \
string/strtok_r.o \
string/strspn.o \
string/strupr.o \
string/strstr.o \
string/strcat.o \
string/strncat.o \
ctype/ctype.o \

BIN_PREFIX := bin/$(TARGET_ARCH)/
OBJ := $(addprefix $(BIN_PREFIX), $(BUILD_OBJ))

LIBK_OBJ:=$(OBJ:.o=.libk.o)

BINARIES=$(BIN_PREFIX)libk.a #$(BIN_PREFIX)libc-$(TARGET_ARCH).a

all: $(BINARIES)

$(BIN_PREFIX)%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< -std=gnu11 $(CFLAGS) $(CPPFLAGS)
$(BIN_PREFIX)%.o: %.S
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(BIN_PREFIX)%.libk.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)
$(BIN_PREFIX)%.libk.o: %.S
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

$(BIN_PREFIX)libc.a: $(OBJ)
	$(AR) rcs $@ $(OBJ)
$(BIN_PREFIX)libk.a: $(LIBK_OBJ)
	$(AR) rcs $@ $(LIBK_OBJ)
clean:
	rm -rf bin/ #*.o */*.o */*/*.o

install-binaries: $(BINARIES)
	mkdir -p $(SYSROOT)$(PREFIX)/lib/$(TARGET_ARCH)/
	cp $(BINARIES) $(SYSROOT)$(PREFIX)/lib/$(TARGET_ARCH)/
install-headers:
	mkdir -p $(SYSROOT)$(PREFIX)/include
	cp -rp ./include $(SYSROOT)$(PREFIX)

install: install-headers install-binaries
