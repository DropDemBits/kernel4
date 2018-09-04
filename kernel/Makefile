CC :=
SOURCE_DIRS :=
ARCH_SRCS :=
ARCH_CFLAGS :=
ARCH_LFLAGS :=
ARCH_LINKER :=
LIBS :=
BUILD_SRCS :=
SYSROOT ?=
PREFIX ?=/usr/local
# Note: add -D__NO_OPTIMIZE__ when using -O0
# -D__K4_VISUAL_STACK__: Visualize thread stacks
CFLAGS := -c -ffreestanding -nostdlib -Wall -Iinclude \
 -I$(SYSROOT)$(PREFIX)/include \
 -Og -g \
# -D__K4_VISUAL_STACK__
#  -O2
LFLAGS := -ffreestanding -nostdlib -lgcc
HAS_LOADER := n

ifdef TARGET_ARCH

ifneq ($(or $(filter x86_64, $(TARGET_ARCH)), $(filter i386, $(TARGET_ARCH))),)
include arch/x86/make.config
else
include arch/$(TARGET_ARCH)/make.config
endif

CFLAGS += $(ARCH_CFLAGS) -D __$(TARGET_ARCH)__
CFLAGS += -L$(SYSROOT)$(PREFIX)/lib
LFLAGS += $(ARCH_LFLAGS)
LFLAGS += -L$(SYSROOT)$(PREFIX)/lib
LIBS += -lk-$(TARGET_ARCH)
endif

include core/make.config

BIN_PREFIX := bin/$(TARGET_ARCH)/

C_SRCS := $(shell find $(SOURCE_DIRS) -type f -name "*.c")
ASM_SRCS := $(shell find $(SOURCE_DIRS) -type f -name "*.S")
OBJS := $(foreach DIR,$(SOURCE_DIRS),$(subst $(DIR),$(BIN_PREFIX),$(shell find $(DIR) -type f -name "*.[cS]")))

.PHONY: all clean include

#bin/$(TARGET_ARCH)/%.o: arch/$(TARGET_ARCH)/%.S
#	mkdir -p $(dir $@)
#	$(CC) -o $@ $< $(LIBS) $(CFLAGS)
#
#bin/$(TARGET_ARCH)/%.o: arch/$(TARGET_ARCH)/%.c
#	mkdir -p $(dir $@)
#	$(CC) -o $@ $< $(LIBS) $(CFLAGS)
#
#bin/$(TARGET_ARCH)/%.o: %.c
#	mkdir -p $(dir $@)
#	$(CC) -o $@ $< $(LIBS) $(CFLAGS)
#
#bin/$(TARGET_ARCH)/k4-$(TARGET_ARCH).kern: $(OBJS)
#	$(CC) -T $(ARCH_LINKER) $(OBJS) $(LIBS) $(LFLAGS) -o $@

bin/$(TARGET_ARCH)/k4-$(TARGET_ARCH).kern:
	@$(foreach SRCFILE,$(OBJS),echo $(SRCFILE);)
	$(CC) none

all: bin/$(TARGET_ARCH)/k4-$(TARGET_ARCH).kern

clean:
	rm -rf bin/

install:
