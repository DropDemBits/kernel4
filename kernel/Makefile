ARCH_OBJ :=
ARCH_CFLAGS :=
ARCH_LINKER :=
BUILD_OBJ :=
CFLAGS := -c -ffreestanding -nostdlib -Wall
LFLAGS := -ffreestanding -nostdlib -lgcc

ifdef TARGET_ARCH
    include arch/$(TARGET_ARCH)/make.config
endif

include core/make.config

%.o: %.c
	i686-elf-gcc -o $@ $< $(CFLAGS)

%.o: %.S
	i686-elf-gcc -o $@ $< $(CFLAGS)

k4.kern: $(ARCH_OBJ) $(BUILD_OBJ)
	i686-elf-gcc -T $(ARCH_LINKER) $(LFLAGS) $(ARCH_OBJ) $(BUILD_OBJ) -o $@

all: k4.kern

clean:
	rm $(BUILD_OBJ) $(ARCH_OBJ) k4.kern
