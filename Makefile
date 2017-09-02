PROJECTS := libk kernel
HOSTS := i386

.PHONY: all clean gen_iso

all:
	@$(foreach HOST,$(HOSTS),make -C kernel TARGET_ARCH=$(HOST))
	@$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST))

clean:
	@$(foreach HOST,$(HOSTS),make -C kernel TARGET_ARCH=$(HOST) clean)
	@$(foreach HOST,$(HOSTS),make -C libk TARGET_ARCH=$(HOST) clean)
	rm -rf isodir
	rm -f k4.iso

geniso: all
	@/bin/bash scripts/gen_iso.sh
