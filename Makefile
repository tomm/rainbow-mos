VERSION_GITREF=$(shell git log -1 --date=format:"%Y%m%d" --format="%ad")-$(shell git rev-parse --short HEAD)
DEBUG=0
FRAMEBUF_AUDIOBUF_SPECIAL_ALLOC ?= 0x0
BIN_NAME ?= mos.bin
RAM_START ?= 0xbd000

# verbosity - comment to show all output
#V = @

# Tool directories
AGONDEV_TOOLCHAIN ?= $(shell agondev-config --prefix)
TOOLBINDIR=$(AGONDEV_TOOLCHAIN)/bin
INCLUDEDIR=$(AGONDEV_TOOLCHAIN)/include
LINKERCONFIG=linker.conf
LIBDIR=$(AGONDEV_TOOLCHAIN)/lib

# Architecture
ARCH=ez80+full
TARGET=ez80-none-elf

# Tools and flags
## Compiler
CC=$(TOOLBINDIR)/ez80-none-elf-clang
CFLAGS=-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -target $(TARGET) \
       -Oz -Wa,-march=$(ARCH) -Wextra -Wno-unused-parameter \
       -DVERSION_GITREF="\"$(VERSION_GITREF)\"" \
       -I src_fatfs -I src_umm_malloc -I src -I $(INCLUDEDIR) -fcolor-diagnostics

ifeq ($(DEBUG),1)
	CFLAGS+=-DDEBUG
endif

## Assembler
ASM=$(TOOLBINDIR)/ez80-none-elf-as
ASMFLAGS=-march=$(ARCH) -I src
## Linker
LINKER=$(TOOLBINDIR)/ez80-none-elf-ld
ifeq ($(LDHAS_ARG_PROCESSING),1)
	LINKER_ARG=-defsym=_parse_option=___arg_processing
endif
LINKER_ARG=-defsym=RAM_START=$(RAM_START) -defsym=FRAMEBUF_AUDIOBUF_SPECIAL_ALLOC=$(FRAMEBUF_AUDIOBUF_SPECIAL_ALLOC)
LINKERFLAGS=$(LINKER_ARG) -Map=$(BINDIR)/mos.map -T $(LINKERCONFIG) --oformat binary -o 
LINKERLIBFLAGS=-L$(LIBDIR) -lagon
SETPROGNAME=$(TOOLBINDIR)/agondev-setname

# project directories
SRCDIR=src
OBJDIR=obj
BINDIR=bin
# Final binary
BINARY=$(BINDIR)/$(BIN_NAME)

# Automatically get all object names from sourcefiles
OBJS= \
	$(patsubst $(SRCDIR)/%.S,   $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.S)) \
	$(patsubst $(SRCDIR)/%.c,   $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.c)) \
	$(patsubst src_fatfs/%.c,   src_fatfs/%.o, $(wildcard src_fatfs/*.c)) \
	$(patsubst src_umm_malloc/%.c,   src_umm_malloc/%.o, $(wildcard src_umm_malloc/*.c))

# Default rule
all: $(BINDIR) $(OBJDIR) $(BINARY)

# Linking all compiled objects into final binary
$(BINARY):$(OBJS)
	@echo [Linking $(BINARY)]
	$(V)$(LINKER) $(LINKERFLAGS)$@ $(OBJS) $(LINKERLIBFLAGS)

# Compile each .c file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(V)$(CC) $(ENABLED_FEATURES) $(CFLAGS) $< -c -o $@
# Preprocess .S files to .s
.S.s:
	$(V)$(CC) $(ENABLED_FEATURES) -E $< > $@
# Assemble each .s file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.s
	$(V)$(ASM) $(ASMFLAGS) $< -o $@

# Directories to create
$(BINDIR):
	@mkdir $(BINDIR)
$(OBJDIR):
	@mkdir $(OBJDIR)

clean: objclean
	@$(RM) -r $(BINDIR) $(OBJDIR) 

objclean:
	@$(RM) src_fatfs/*.o src_umm_malloc/*.o src/*.o

format:
	clang-format-16 -i src/*.c src/*.h --style=file:./clang-format.conf

fbmos:
	ENABLED_FEATURES=-DFEAT_FRAMEBUFFER \
	FRAMEBUF_AUDIOBUF_SPECIAL_ALLOC=0x102 \
	RAM_START=0xbe000 \
	BIN_NAME=mos_fb.bin \
		$(MAKE) all

mos:
	$(MAKE) all

dist:
	$(MAKE) clean
	$(MAKE) fbmos
	cp bin/mos_fb.bin .
	$(MAKE) clean
	$(MAKE) mos
	cp bin/mos.bin .
