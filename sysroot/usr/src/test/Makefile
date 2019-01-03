CC ?=
LD ?=
TARGET_ARCH ?=

SYSROOT ?=
BIN_PREFIX := $(SYSROOT)/usr/bin

# Don't use the stdlib
CFLAGS := -nostdlib

OBJS := \
	test.o \

.PHONY: all clean

%.o: %.S
	$(CC) $< -o $@ $(CFLAGS)

test.bin: $(OBJS)
	$(LD) $< -o $@

all: test.bin

install: test.bin
	cp $< $(BIN_PREFIX)

clean:
	rm -f $(OBJS)
	rm test.bin
