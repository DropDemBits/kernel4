PROJECTS := libk kernel
TARGETS ?=
COMPILERS := i686-k4os-gcc x86_64-k4os-gcc
INITRD_FILES := $(addprefix sysroot/,$(shell /bin/bash scripts/list_initrd.sh))
PROGRAM_PROJECTS := test

ifeq ($(strip $(TARGETS)),)
$(error "TARGETS" is not set)
endif

.PHONY: all clean geniso build-libk build-kernel build-programs clean-libk clean-kernel clean-programs sysroot pxeboot

all: geniso

sysroot:
	$(foreach PROJECT,$(PROJECTS),$(foreach TARGET,$(TARGETS),make -C libk TARGET_ARCH=$(TARGET) SYSROOT=../sysroot install &&))

build-libk:
	@$(foreach TARGET,$(TARGETS),make -C libk TARGET_ARCH=$(TARGET) SYSROOT=../sysroot all install &&) echo Done building libk

build-kernel: build-libk
	@$(foreach TARGET,$(TARGETS),scripts/build_arch_cmake.sh $(TARGET) && ) echo Done building kernel

build-programs:
	mkdir -p sysroot/usr/bin
	$(foreach PROGRAM,$(PROGRAM_PROJECTS),make -C sysroot/usr/src/$(PROGRAM) CC=x86_64-k4os-gcc LD=x86_64-k4os-ld TARGET_ARCH=x86_64 SYSROOT=../../../. all install;)

clean-libk:
	@$(foreach TARGET,$(TARGETS),make -C libk TARGET_ARCH=$(TARGET) clean &&) echo Done cleaning libk

clean-kernel:
	@$(foreach TARGET,$(TARGETS),cmake -E chdir build/$(TARGET) make clean &&) echo Done cleaning kernel
	@#$(foreach TARGET,$(TARGETS),make -C kernel TARGET_ARCH=$(TARGET) clean &&) echo Done cleaning kernel

clean-programs:
	$(foreach PROGRAM,$(PROGRAM_PROJECTS),make -C sysroot/usr/src/$(PROGRAM) CC=x86_64-k4os-gcc LD=x86_64-k4os-ld TARGET_ARCH=x86_64 clean;)
	rm -rf sysroot/usr/bin

clean: clean-libk clean-kernel clean-programs
	@$(foreach TARGET,$(TARGETS),rm -f k4-$(TARGET).iso;)
	rm -rf isodir
	rm -rf sysroot/usr/lib
	rm -rf pxedest

initrd/initrd/initrd.tar: $(INITRD_FILES)
	# Make the empty directories first
	mkdir -p sysroot/dev
	@scripts/gen_initrd.sh

geniso: build-libk build-kernel build-programs initrd/initrd/initrd.tar
	@$(foreach TARGET,$(TARGETS),/bin/bash scripts/gen_iso.sh $(TARGET) &&) echo Done

pxeboot: build-libk build-kernel initrd/initrd/initrd.tar
	mkdir -p pxedest
	@#$(foreach TARGET,$(TARGETS),cp kernel/bin/$(TARGET)/k4-$(TARGET).kern pxedest/ &&) echo Done
	@$(foreach TARGET,$(TARGETS),cp build/$(TARGET)/kernel/k4-$(TARGET).kern pxedest/ &&) echo Done
	cp initrd/initrd/initrd.tar pxedest/
	sudo cp pxedest/* /var/lib/tftpboot/
