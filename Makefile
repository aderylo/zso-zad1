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
CFLAGS += -march=lakemont -mtune=lakemont -m32 -miamcu -msoft-float
CFLAGS += -nostdlib -fno-builtin -mno-default # -nostdinc -- needed for <float.h>
#CFLAGS += -static
# Disable GOT32X relocation.
CFLAGS += -Wa,-mrelax-relocations=no
CFLAGS += -Wl,--no-relax

NO_PLT = -fno-plt
SIMPLE_PIC = -fno-plt -fpic -fno-jump-tables
NOTHING = -fno-pic -fno-pie -fno-plt -fno-jump-tables
#-static-pie, -pie


LDFLAGS += $(CFLAGS)

# Maybe we won't use this for rebuilding!
LATE_LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--emit-relocs
LDFLAGS += -Wl,--build-id=none

# This notation should work for a while...
LD_VERSION := $(shell $(LD) --version -v 2>/dev/null | sed -n -E -e '/GNU ld/ s/[^0-9]//g p'\
                                                     | head -c3)
# --no-warn-rwx-segments is from 2.39
# $(intcmp) is only from Make 4.4...
ifeq ($(shell [ ${LD_VERSION} -le 239 ] && echo less), )
LDFLAGS += -Wl,--no-warn-rwx-segments
endif

LDFLAGS_FOR_PLT += -Wl,--dynamic-linker=
LDFLAGS_FOR_PLT += -Wl,-znorelro,-ztextoff

LDFLAGS_FOR_STATIC += -Wl,-znorelro,-ztext


PICOLIBC := picolibc/iamcu
#LATE_LDFLAGS += -T quarkish.ld
LATE_LDFLAGS += -T $(PICOLIBC)/lib/picolibc.ld

BUILD ?= debug
#BUILD ?= release

ifeq ($(BUILD), debug)
CFLAGS += -O0
#CFLAGS += -g -DDEBUG
CFLAGS += -g0 -DDEBUG
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


raw_targets := $(addprefix examples/, a.elf min.elf min_got.elf min_got_harder.elf min_plt.elf)
stdlib_targets := $(addprefix examples/, b.elf min_libc.elf min_libc_plt.elf min_libc_constructors.elf tricky.elf iotbench.elf question.elf)
all_targets := $(raw_targets) $(stdlib_targets)

plt_targets := examples/min_plt.elf examples/min_libc_plt.elf
#plt_targets += examples/b.elf examples/min_libc.elf examples/tricky.elf
static_targets := $(filter-out $(plt_targets), $(all_targets))

all: force
	# Manually run this makefile or each target.
	# (dependencies may be compiled with various flags)
	for target in ${all_targets}; do \
		$(MAKE) -C . -B $$target; \
	done

# This syntax updates the var only for matching targets (and dependents)
$(stdlib_targets): LIBRARIES += -lc -lm -lgcc
$(stdlib_targets): CFLAGS += -I$(PICOLIBC)/include  -L$(PICOLIBC)/lib

ifeq (${BINUTILS_PREFIX}, x86_64-linux-gnu-)
# If not a proper cross-compilation.
# The default libgcc is either not present or compiled without softfloat (gcc-10-multilib).
$(stdlib_targets): CFLAGS += -Lpicolibc/iamcu-but-i386
endif
#$(stdlib_targets): LDFLAGS += -specs=picolibc.specs

$(static_targets): CFLAGS += -static
$(static_targets): LDFLAGS += $(LDFLAGS_FOR_STATIC)
$(plt_targets): LDFLAGS += $(LDFLAGS_FOR_PLT)

# This syntax adds dependencies for entries matching the pattern
# $(var:pat=repl) is a replacement syntax
$(stdlib_targets:elf=part) $(raw_targets:elf=part): %.part: examples/_io_impl.o
$(stdlib_targets:elf=part): %.part: picolibc/crt0.o picolibc/picolibc_syscalls.o

examples/tricky.part: examples/tricky_part1.o
examples/iotbench.part: $(addprefix examples/IoTBench/, $(addsuffix .o, main list_search_sort conv matrix))

# Syntax for changing dep/vars of only one file
examples/b.o: CFLAGS += -Wno-format # picolibc's doing a weird trick

# Try mix-and-match various flags for various examples
examples/a.part: CFLAGS += $(NOTHING)
#examples/a.part: CFLAGS += $(NO_PLT)
examples/b.part: CFLAGS += $(NO_PLT)

#picolibc/crt0.o: CFLAGS += -fno-pic -fno-plt
examples/mi%.part: CFLAGS += -Os

#examples/tricky.elf: CFLAGS += -DFULL_TEST_VERSION

%.elf: %.part
	@echo Linking with ld version ${LD_VERSION} through gcc
	$(LD) $(LDFLAGS) $(LATE_LDFLAGS) $(filter %.o %.c %.part, $^)  -o $@

# Note: this is not to be always assumed!
%.part: %.o
	$(LD) $(LDFLAGS)  -Wl,-\( $(filter %.o, $^) $(LIBRARIES) -Wl,-\) -o $@ -r -Wl,-e,_start -Wl,--gc-sections

%.o: %.S Makefile force
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c Makefile include/* force
	$(CC) $(CFLAGS) -c $< -o $@

%.flash: %.elf
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@

force: ;
.PHONY: force


%.strip: %.elf
	# Not present in my objcopy?
	#$(OBJCOPY) --strip-all --enable-deterministic-archives  --strip-section-headers $< $@
	llvm-objcopy --strip-all -D --strip-sections $< $@

%-objdump: %.elf
	$(OBJDUMP) $(OBJDUMP_FLAGS) $<

%-run: %.elf
	-i386 ./$< <$(if $(wildcard $(<:elf=input)), $(<:elf=input), /dev/null)

# Helpful for IDEs to click
examples/a.elf:
examples/b.elf:
examples/min_got_harder.elf:
examples/tricky.elf:
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
	@echo Patching the linker script to make Linux always allocate the whole simulated SRAM for us.
	sed -i picolibc/iamcu/lib/picolibc.ld -e '/\.stack (NOLOAD)/ a . += __heap_size - (DEFINED(__heap_size_min) ? __heap_size_min : 0);'


# May come useful to test relinking. Usage: make relink WHAT=my_reloc_file
.PHONY: relink
relink: Makefile
	$(LD) $(LDFLAGS) $(LATE_LDFLAGS) $(LDFLAGS_FOR_STATIC) -static ${WHAT}  -o ${WHAT}.relf

check.pex: check.py checklib
	# -D checklib
	pex -o $@ -v --sh-boot --exe $< -P checklib "pyelftools==0.29"

.PHONY: prepare-zso-image
prepare-zso-image:
	sudo apt install -f llvm-16 # for llvm-objcopy
