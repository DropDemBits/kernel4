PROJECTS := libk kernel
HOSTS := i386 x86_64
COMPILERS := i686-elf-gcc x86_64-elf-gcc
INITRD_FILES := $(addprefix initrd/files/,$(shell /bin/bash scripts/list_initrd.sh))

.PHONY: all clean geniso build-libk build-kernel clean-libk clean-kernel sysroot pxeboot

all: geniso

sysroot:
	$(foreach PROJECT,$(PROJECTS),$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) SYSROOT=../sysroot install &&))

build-libk:
	@$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) SYSROOT=../sysroot all install &&) echo Done building libk

build-kernel: build-libk
	@$(foreach HOST,$(HOSTS),make -C kernel TARGET_ARCH=$(HOST) SYSROOT=../sysroot all install && ) echo Done building kernel

clean-libk:
	@$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) clean &&) echo Done cleaning libk

clean-kernel:
	@$(foreach HOST,$(HOSTS),make -C kernel TARGET_ARCH=$(HOST) clean &&) echo Done cleaning kernel

clean: clean-libk clean-kernel
	@$(foreach HOST,$(HOSTS),rm -f k4-$(HOST).iso;)
	rm -rf isodir
	rm -rf sysroot

initrd/initrd/initrd.tar: $(INITRD_FILES)
	@scripts/gen_initrd.sh

geniso: build-libk build-kernel initrd/initrd/initrd.tar
	@$(foreach HOST,$(HOSTS),/bin/bash scripts/gen_iso.sh $(HOST) &&) echo Done
