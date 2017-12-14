PROJECTS := libk kernel
HOSTS := i386 x86_64
COMPILERS := i686-elf-gcc x86_64-elf-gcc

.PHONY: all clean geniso build-libk build-kernel clean-libk clean-kernel sysroot

sysroot:
	$(foreach PROJECT,$(PROJECTS),$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) SYSROOT=../sysroot install &&))

build-libk:
	$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) SYSROOT=../sysroot all install &&) echo Done

build-kernel: build-libk
	$(foreach HOST,$(HOSTS),make -C kernel TARGET_ARCH=$(HOST) SYSROOT=../sysroot all install && ) echo Done

all: build-libk build-kernel


clean-libk:
	$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) clean &&) echo Done

clean-kernel:
	@$(foreach HOST,$(HOSTS),make -C kernel TARGET_ARCH=$(HOST) clean &&) echo Done

clean: clean-libk clean-kernel
	@$(foreach HOST,$(HOSTS),rm -f k4-$(HOST).iso;)
	rm -rf isodir
	rm -rf sysroot

geniso: build-libk build-kernel
	@$(foreach HOST,$(HOSTS),/bin/bash scripts/gen_iso.sh $(HOST) &&) echo Done
