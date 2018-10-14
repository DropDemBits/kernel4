CC :=
SOURCE_DIRS :=
ARCH_SRCS :=
ARCH_CFLAGS :=
ARCH_LFLAGS :=
ARCH_LINKER :=
LIBS :=
BUILD_SRCS :=
SYSROOT ?=
PREFIX ?=/usr/local
# Note: add -D__NO_OPTIMIZE__ when using -O0
# -D__K4_VISUAL_STACK__: Visualize thread stacks
CFLAGS := -c -ffreestanding -nostdlib -Wall -Wextra -Iinclude \
 -I$(SYSROOT)$(PREFIX)/include \
 -Og -g \
# -D__K4_VISUAL_STACK__
#  -O2
LFLAGS := -ffreestanding -nostdlib -lgcc
HAS_LOADER := n

ifdef TARGET_ARCH

ifneq ($(or $(filter x86_64, $(TARGET_ARCH)), $(filter i386, $(TARGET_ARCH))),)
include arch/x86/make.config
else
include arch/$(TARGET_ARCH)/make.config
endif

CFLAGS += $(ARCH_CFLAGS) -D __$(TARGET_ARCH)__
LFLAGS += $(ARCH_LFLAGS)
LFLAGS += -L$(SYSROOT)$(PREFIX)/lib/$(TARGET_ARCH)
LIBS += -lk
endif

include core/make.config

BIN_PREFIX := bin/$(TARGET_ARCH)/

OBJS := $(addprefix $(BIN_PREFIX),$(addsuffix .obj,$(ARCH_SRCS) $(BUILD_SRCS)))
BIN_DIRS := $(addprefix $(BIN_PREFIX),$(shell find $(SOURCE_DIRS) -type d -not -path "*include*"))

.PHONY: all clean include make_bindirs

make_bindirs:
	@$(foreach dir,$(BIN_DIRS),mkdir -p $(dir);)

$(BIN_PREFIX)%.obj: %
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

k4-$(TARGET_ARCH).kern: make_bindirs $(OBJS)
	$(CC) -T $(ARCH_LINKER) $(OBJS) $(LIBS) $(LFLAGS) -o bin/$(TARGET_ARCH)/$@

all: k4-$(TARGET_ARCH).kern

clean:
	rm -rf bin/$(TARGET_ARCH)

install:
