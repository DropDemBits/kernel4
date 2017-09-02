CC :=
ARCH_OBJ :=
ARCH_CFLAGS :=
ARCH_LINKER :=
BUILD_OBJ :=
CFLAGS := -c -ffreestanding -nostdlib -Wall
LFLAGS := -ffreestanding -nostdlib -lgcc

ifdef TARGET_ARCH
    include arch/$(TARGET_ARCH)/make.config
endif

#include core/make.config

.PHONY: all clean

%.o: %.c
	mkdir -p bin/$(dir $@)
	$(CC) -o $@ $< $(CFLAGS)

%.o: %.S
	mkdir -p bin/$(dir $@)
	$(CC) -o $@ $< $(CFLAGS)

k4-$(TARGET_ARCH).kern: $(ARCH_OBJ) $(BUILD_OBJ)
	$(CC) -T $(ARCH_LINKER) $(LFLAGS) $(ARCH_OBJ) $(BUILD_OBJ) -o $@

all: k4.kern

clean:
	rm -f $(BUILD_OBJ) $(ARCH_OBJ) k4-$(TARGET_ARCH).kern
