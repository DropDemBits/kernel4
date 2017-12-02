CC :=
ARCH_OBJ :=
ARCH_CFLAGS :=
ARCH_LFLAGS :=
ARCH_LINKER :=
BUILD_OBJ :=
CFLAGS := -c -ffreestanding -nostdlib -Wall -Iinclude/common -O2
LFLAGS := -ffreestanding -nostdlib -lgcc
HAS_LOADER := n

ifdef TARGET_ARCH
include arch/$(TARGET_ARCH)/make.config
CFLAGS += $(ARCH_CFLAGS) -D _$(TARGET_ARCH)_ -Iinclude/$(TARGET_ARCH)
LFLAGS += $(ARCH_LFLAGS)
endif

include core/make.config

BIN_PREFIX := bin/$(TARGET_ARCH)/
OBJS := $(addprefix $(BIN_PREFIX), $(ARCH_OBJ) $(BUILD_OBJ))


.PHONY: all clean

bin/$(TARGET_ARCH)/%.o: arch/$(TARGET_ARCH)/%.S
	mkdir -p $(dir $@)
	$(CC) -o $@ $< $(CFLAGS)

bin/$(TARGET_ARCH)/%.o: arch/$(TARGET_ARCH)/%.c
	mkdir -p $(dir $@)
	$(CC) -o $@ $< $(CFLAGS)

bin/$(TARGET_ARCH)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -o $@ $< $(CFLAGS)

bin/$(TARGET_ARCH)/k4-$(TARGET_ARCH).kern: $(OBJS)
	$(CC) -T $(ARCH_LINKER) $(LFLAGS) $(OBJS) -o $@

all: k4.kern

clean:
	rm -rf bin/
