CC ?=
LD ?=
TARGET_ARCH ?=

SYSROOT ?=
BIN_PREFIX := $(SYSROOT)/usr/bin

# Currently, there's no OS toolchain, so don't use the stdlib
CFLAGS := -ffreestanding -nostdlib

OBJS := \
	test.o \

.PHONY: all clean

%.o: %.S
	$(CC) $< -o $@ $(CFLAGS)

test.bin: $(OBJS)
	$(LD) -T arch/$(TARGET_ARCH)/linker.ld $< -o $@

all: test.bin

install: test.bin
	cp $< $(BIN_PREFIX)

clean:
	rm -f $(OBJS)
	rm test.bin
