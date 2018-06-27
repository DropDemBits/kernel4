CC :=
ARCH_OBJ :=
ARCH_CFLAGS :=
ARCH_LFLAGS :=
ARCH_LINKER :=
LIBS :=
BUILD_OBJ :=
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
include arch/$(TARGET_ARCH)/make.config
CFLAGS += $(ARCH_CFLAGS) -D _$(TARGET_ARCH)_
CFLAGS += -L$(SYSROOT)$(PREFIX)/lib
LFLAGS += $(ARCH_LFLAGS)
LFLAGS += -L$(SYSROOT)$(PREFIX)/lib
LIBS += -lk-$(TARGET_ARCH)
endif

include core/make.config

BIN_PREFIX := bin/$(TARGET_ARCH)/
OBJS := $(addprefix $(BIN_PREFIX), $(ARCH_OBJ) $(BUILD_OBJ))


.PHONY: all clean include

bin/$(TARGET_ARCH)/%.o: arch/$(TARGET_ARCH)/%.S
	mkdir -p $(dir $@)
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

bin/$(TARGET_ARCH)/%.o: arch/$(TARGET_ARCH)/%.c
	mkdir -p $(dir $@)
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

bin/$(TARGET_ARCH)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

bin/$(TARGET_ARCH)/k4-$(TARGET_ARCH).kern: $(OBJS)
	$(CC) -T $(ARCH_LINKER) $(OBJS) $(LIBS) $(LFLAGS) -o $@

all: bin/$(TARGET_ARCH)/k4-$(TARGET_ARCH).kern

clean:
	rm -rf bin/

install:
