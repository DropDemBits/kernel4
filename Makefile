PROJECTS := libk kernel
HOSTS := i386 x86_64

.PHONY: all clean geniso

all:
	$(foreach PROJECT,$(PROJECTS),$(foreach HOST,$(HOSTS),make -C $(PROJECT) TARGET_ARCH=$(HOST);))

clean:
	@$(foreach PROJECT,$(PROJECTS),$(foreach HOST,$(HOSTS),make -C $(PROJECT) TARGET_ARCH=$(HOST) clean;))
	@$(foreach HOST,$(HOSTS),rm -f k4-$(HOST).iso;)
	rm -rf isodir

geniso: all
	@$(foreach HOST,$(HOSTS),/bin/bash scripts/gen_iso.sh $(HOST);)
