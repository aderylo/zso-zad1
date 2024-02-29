# Flags are inspired by
# https://github.com/quark-mcu/qm-bootloader/blob/master/base.mk
# https://github.com/quark-mcu/qmsi/blob/master/base.mk


BINUTILS_PREFIX ?= x86_64-linux-gnu-
BINUTILS_VERSION ?= -10

CC = ${BINUTILS_PREFIX}gcc${BINUTILS_VERSION}
NM = ${BINUTILS_PREFIX}nm${BINUTILS_VERSION}
OBJDUMP = ${BINUTILS_PREFIX}objdump
OBJCOPY =  ${BINUTILS_PREFIX}objcopy
LD := $(CC)

### Flags

CFLAGS += -Iinclude/
#CFLAGS += -Wall -Wextra -Werror
CFLAGS +=  -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -march=lakemont -mtune=lakemont -m32 -miamcu #-msoft-float
CFLAGS += -nostdlib -fno-builtin -mno-default # -nostdinc -- needed for <float.h>
CFLAGS += -static

SIMPLE_PIC = -fno-plt -fpic -fno-jump-tables
NOTHING = -fno-pic -fno-pie -fno-plt -fno-jump-tables
#CFLAGS +=  -fno-plt
#CFLAGS +=  -fno-shrink-wrap
#-static-pie, -pie


LDFLAGS += $(CFLAGS)
#LDFLAGS += -static

LATE_LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--emit-relocs
#LDFLAGS += -nolibc
LDFLAGS += -Wl,--build-id=none

#LDLIBS += -lc -lnosys -lsoftfp -lgcc
#LDFLAGS += -static -pie
LDFLAGS += -Wl,--no-dynamic-linker
#LDFLAGS += -Wl,-z,text,-z,norelro


PICOLIBC := picolibc/iamcu
#LATE_LDFLAGS += -T quarkish.ld
LATE_LDFLAGS += -T $(PICOLIBC)/lib/picolibc.ld

#BUILD ?= debug
BUILD ?= release

ifeq ($(BUILD), debug)
CFLAGS += -O0 -g -DDEBUG
LDFLAGS += -Wl,--strip-debug
else ifeq ($(BUILD), release)
CFLAGS += -Os -fomit-frame-pointer
LDFLAGS += -Wl,--strip-debug
else ifeq ($(BUILD), lto)
CFLAGS += -Os -fomit-frame-pointer -flto
LDFLAGS += -flto
else
$(error Supported BUILD values are 'release', 'debug' and 'lto')
endif


OBJCOPY_FLAGS += -O binary --gap-fill 0x90 # That's NOP

OBJDUMP_FLAGS += --disassemble --disassembler-options=i386
OBJDUMP_FLAGS += --disassembler-color=terminal
OBJDUMP_FLAGS += --visualize-jumps=extended-color -w
ifdef WITH_SYMBOLS
OBJDUMP_FLAGS += -T -r -t -h
endif

### Don't treat the .elf as intermediate
.PRECIOUS: %.elf %.part %.flash %.o
.PHONY: %-run run-% %-objdump clean


TARGET ?= all


raw_targets := examples/a.elf
stdlib_targets := examples/b.elf

all: $(stdlib_targets) $(raw_targets)

# This syntax updates the var only for matching targets (and dependents)
$(stdlib_targets): LIBRARIES += -lc
$(stdlib_targets): CFLAGS += -I$(PICOLIBC)/include  -L$(PICOLIBC)/lib
#$(stdlib_targets): LDFLAGS += -specs=picolibc.specs

# This syntax adds dependencies for entries matching the pattern
# $(var:pat=repl) is a replacement syntax
$(stdlib_targets:elf=part) $(raw_targets:elf=part): %.part: examples/_io_impl.o
$(stdlib_targets:elf=part): %.part: picolibc/crt0.o picolibc/picolibc_syscalls.o

#$(stdlib_targets:.elf=-objdump): OBJDUMP_FLAGS += --source

# Syntax for changing depf of only one file
examples/b.o: CFLAGS += -Wno-format # picolibc's doing a weird trick


%.elf: %.part
	$(LD) $(LDFLAGS) $(LATE_LDFLAGS) $(filter %.o %.c %.part, $^)  -o $@

# Note: this is not to be always assumed!
%.part: %.o
	$(LD) $(LDFLAGS) -Wl,-\( $(filter %.o, $^) $(LIBRARIES) -Wl,-\) -o $@ -r

%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c Makefile include/*
	$(CC) $(CFLAGS) -c $< -o $@

%.flash: %.elf
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@

%.strip: %.elf
	# Not present in my objcopy?
	#$(OBJCOPY) --strip-all --enable-deterministic-archives  --strip-section-headers $< $@
	llvm-objcopy --strip-all -D --strip-sections $< $@

%-objdump: %.elf
	$(OBJDUMP) $(OBJDUMP_FLAGS) $<

%-run: %.elf
	-i386 ./$< < $(<:elf=input)

# Helpful for IDEs to click
run-a: examples/a-run
run-b: examples/b-run

source_dirs = examples/ picolibc/
clean:
	-rm -f  $(foreach dir, $(source_dirs), $(addprefix $(dir), *.elf *.part *.strip *.o *.flash))


.PHONY: build-picolibc
build-picolibc:
	! [ -d picolibc/_repo -a -f picolibc/_repo/meson.build ] && git submodule update --init --recursive --single-branch picolibc/_repo || true
	rm -rf picolibc/meson-build picolibc/iamcu
	#mkdir -p picolibc_build
	meson setup $([ -e picolibc_build/ ] && echo --wipe) \
		-Dincludedir=include \
		-Dlibdir=lib \
		-Dprefix=$(abspath picolibc/iamcu/) \
		-Dmultilib=false \
		-Dsemihost=false \
		-Dpicocrt=false \
		-Dformat-default=float \
		-Dthread-local-storage=false \
		-Dnewlib-multithread=false  -Dnewlib-retargetable-locking=false \
		-Dnewlib-iconv-encodings=utf_8,win_1250,iso_8859_1,iso_8859_2 \
		-Dspecsdir=none \
		--cross-file picolibc/picolibc-quarkish-linux-none.txt \
		picolibc/meson-build picolibc/_repo

	cd picolibc/meson-build && ninja && ninja install
