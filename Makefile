CC := x86_64-elf-gcc
CXX := x86_64-elf-g++
LD := x86_64-elf-ld
OBJCP := x86_64-elf-objcopy

FAT_DISK_SIZE := 1024


TZ   := Europe/Berlin
DD   := $(shell TZ=$(TZ) date +%d)
MM   := $(shell TZ=$(TZ) date +%m)
YYYY := $(shell TZ=$(TZ) date +%Y)
HH   := $(shell TZ=$(TZ) date +%H)
MIN  := $(shell TZ=$(TZ) date +%M)

MJ := $(shell cat buildInfo/mj)
MN := $(shell cat buildInfo/mn)
BN := $(shell cat buildInfo/bn)

VERSION := $(DD)$(MM)$(YYYY).$(HH)$(MIN)-$(MJ).$(MN)-$(BN)

# ============================================================================
#  Build variants
#  Each variant only differs in which single DEFINES flag is appended to the
#  common set. Add/remove variants here and everything below adapts.
# ============================================================================
VARIANTS := testing production kernelpanic

# -DKEYBOARD_ VARIANTS: GERMAN // EN_US
DEFINES_testing     := -DKEYBOARD_GERMAN -DDEBUG -DTESTING
DEFINES_production  := -DKEYBOARD_GERMAN -DDEBUG -DPRODUCTION
DEFINES_kernelpanic := -DKEYBOARD_GERMAN -DDEBUG -DKERNELPANIC

# Which variant's kernel.bin gets packaged into the ISO / used by `make run`
ISO_VARIANT := testing

# DEFINES used for building the userspace programs (.lhe files). These are
# not kernel-variant specific, so a single fixed set is used.
DEFINES := -DKEYBOARD_GERMAN -DDEBUG -DPRODUCTION

kernel_c_source_files := $(shell find src/kernel -name *.c)
kernel_cpp_source_files := $(shell find src/kernel -name *.cpp)

csh_c_source_files := $(shell find src/csh -name *.c)
csh_cpp_source_files := $(shell find src/csh -name *.cpp)

x86_64_c_source_files := $(shell find src/x86_64 -name *.c)
x86_64_asm_source_files := $(shell find src/x86_64 -name *.asm)

libs_c_source_files := $(shell find src/libs/src -name *.c)
libs_cpp_source_files := $(shell find src/libs/src -name *.cpp)
libs_asm_source_files := $(shell find src/libs/src -name *.asm)

cd_program_c_source_files := $(shell find src/programs/cd -name *.c)
fat_program_c_source_files := $(shell find src/programs/fat -name *.c)

cd_program_c_lhe_files := $(patsubst src/programs/cd/%.c, targets/x86_64/iso/programs/%.lhe, $(cd_program_c_source_files))
fat_program_c_lhe_files := $(patsubst src/programs/fat/%.c, targets/x86_64/disk/programs/%.lhe, $(fat_program_c_source_files))


cd_program_cpp_source_files := $(shell find src/programs/cd -name *.cpp)
fat_program_cpp_source_files := $(shell find src/programs/fat -name *.cpp)

cd_program_cpp_lhe_files := $(patsubst src/programs/cd/%.cpp, targets/x86_64/iso/programs/%.lhe, $(cd_program_cpp_source_files))
fat_program_cpp_lhe_files := $(patsubst src/programs/fat/%.cpp, targets/x86_64/disk/programs/%.lhe, $(fat_program_cpp_source_files))

program_lhe_files := $(cd_program_c_lhe_files) $(fat_program_c_lhe_files) $(cd_program_cpp_lhe_files) $(fat_program_cpp_lhe_files)


INCLUDES := -I src/libs/include -I src/libs/include/kapi -I src/kernel/include -I src/kernel/cpp/include -I src/csh/include -I src/kernel/TESTING/include

KERNEL_SAFETY_FLAGS := -mno-red-zone -mno-mmx -mno-sse -mno-sse2

CFLAGS := $(KERNEL_SAFETY_FLAGS)
CXXFLAGS := -std=c++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics $(KERNEL_SAFETY_FLAGS)

CXXFLAGS_SSE_OK := -std=c++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics -mno-red-zone

# ----------------------------------------------------------------------------
#  Per-variant object file lists (build/<variant>/...)
# ----------------------------------------------------------------------------
define VARIANT_LISTS
kernel_c_object_files_$(1)   := $$(patsubst src/kernel/%.c, build/$(1)/kernel/%.o, $$(kernel_c_source_files))
kernel_cpp_object_files_$(1) := $$(patsubst src/kernel/%.cpp, build/$(1)/kernel/%.o, $$(kernel_cpp_source_files))

csh_c_object_files_$(1)   := $$(patsubst src/csh/%.c, build/$(1)/csh/%.o, $$(csh_c_source_files))
csh_cpp_object_files_$(1) := $$(patsubst src/csh/%.cpp, build/$(1)/csh/%.o, $$(csh_cpp_source_files))

x86_64_c_object_files_$(1)   := $$(patsubst src/x86_64/%.c, build/$(1)/x86_64/%.o, $$(x86_64_c_source_files))
x86_64_asm_object_files_$(1) := $$(patsubst src/x86_64/%.asm, build/$(1)/x86_64/%.o, $$(x86_64_asm_source_files))

libs_c_object_files_$(1)   := $$(patsubst src/libs/src/%.c, build/$(1)/libs/%.o, $$(libs_c_source_files))
libs_cpp_object_files_$(1) := $$(patsubst src/libs/src/%.cpp, build/$(1)/libs/%.o, $$(libs_cpp_source_files))
libs_asm_object_files_$(1) := $$(patsubst src/libs/src/%.asm, build/$(1)/libs/%.o, $$(libs_asm_source_files))

kernel_object_files_$(1) := $$(kernel_c_object_files_$(1)) $$(kernel_cpp_object_files_$(1))
x86_64_object_files_$(1) := $$(x86_64_c_object_files_$(1)) $$(x86_64_asm_object_files_$(1))
csh_object_files_$(1)    := $$(csh_c_object_files_$(1)) $$(csh_cpp_object_files_$(1))
libs_object_files_$(1)   := $$(libs_c_object_files_$(1)) $$(libs_cpp_object_files_$(1)) $$(libs_asm_object_files_$(1))
endef

$(foreach v,$(VARIANTS),$(eval $(call VARIANT_LISTS,$(v))))

# ============================================================================
#  Pretty output helpers (Docker-style step counter + colored labels)
# ============================================================================
BOLD    := \033[1m
DIM     := \033[2m
RED     := \033[31m
GREEN   := \033[32m
YELLOW  := \033[33m
BLUE    := \033[34m
MAGENTA := \033[35m
CYAN    := \033[36m
RESET   := \033[0m

STEP_FILE := build/.step

# Total number of compile/assemble/link-program steps across the programs
# AND all three kernel variants (used for the [n/N] counter)
TOTAL_STEPS := $(words $(cd_program_c_lhe_files) $(fat_program_c_lhe_files) $(cd_program_cpp_lhe_files) $(fat_program_cpp_lhe_files) \
	$(foreach v,$(VARIANTS),$(kernel_object_files_$(v)) $(x86_64_object_files_$(v)) $(libs_object_files_$(v)) $(csh_object_files_$(v))))

# $(call step,LABEL,TEXT) -- prints "[ n/N] LABEL  TEXT" and advances the counter
define step
	@n=$$(( $$(cat $(STEP_FILE) 2>/dev/null || echo 0) + 1 )); \
	echo $$n > $(STEP_FILE); \
	printf "$(DIM)[%2d/%2d]$(RESET) %b $(DIM)%s$(RESET)\n" $$n $(TOTAL_STEPS) "$(1)" "$(2)"
endef

# ----------------------------------------------------------------------------
#  Per-variant compile rules (build/<variant>/...)
# ----------------------------------------------------------------------------
define COMPILE_RULES

build/$(1)/kernel/%.o: src/kernel/%.c
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD) CC $(RESET) [$(1)],$$(patsubst build/$(1)/kernel/%.o, src/kernel/%.c, $$@))
	@$$(CC) $$(DEFINES_$(1)) $$(CFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/kernel/%.o, src/kernel/%.c, $$@) -o $$@

build/$(1)/kernel/%.o: src/kernel/%.cpp
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD)CXX $(RESET) [$(1)],$$(patsubst build/$(1)/kernel/%.o, src/kernel/%.cpp, $$@))
	@$$(CXX) $$(DEFINES_$(1)) $$(CXXFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/kernel/%.o, src/kernel/%.cpp, $$@) -o $$@

build/$(1)/kernel/commands.o: CXXFLAGS := $(CXXFLAGS_SSE_OK)

build/$(1)/csh/%.o: src/csh/%.c
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD) CC $(RESET) [$(1)],$$(patsubst build/$(1)/csh/%.o, src/csh/%.c, $$@))
	@$$(CC) $$(DEFINES_$(1)) $$(CFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/csh/%.o, src/csh/%.c, $$@) -o $$@

build/$(1)/csh/%.o: src/csh/%.cpp
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD)CXX $(RESET) [$(1)],$$(patsubst build/$(1)/csh/%.o, src/csh/%.cpp, $$@))
	@$$(CXX) $$(DEFINES_$(1)) $$(CXXFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/csh/%.o, src/csh/%.cpp, $$@) -o $$@

build/$(1)/x86_64/%.o: src/x86_64/%.c
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD) CC $(RESET) [$(1)],$$(patsubst build/$(1)/x86_64/%.o, src/x86_64/%.c, $$@))
	@$$(CC) $$(DEFINES_$(1)) $$(CFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/x86_64/%.o, src/x86_64/%.c, $$@) -o $$@

build/$(1)/x86_64/%.o: src/x86_64/%.cpp
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD)CXX $(RESET) [$(1)],$$(patsubst build/$(1)/x86_64/%.o, src/x86_64/%.cpp, $$@))
	@$$(CXX) $$(DEFINES_$(1)) $$(CXXFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/x86_64/%.o, src/x86_64/%.cpp, $$@) -o $$@

build/$(1)/x86_64/%.o: src/x86_64/%.asm
	@mkdir -p $$(dir $$@)
	$$(call step,$(MAGENTA)$(BOLD)ASM $(RESET) [$(1)],$$(patsubst build/$(1)/x86_64/%.o, src/x86_64/%.asm, $$@))
	@nasm -f elf64 $$(patsubst build/$(1)/x86_64/%.o, src/x86_64/%.asm, $$@) -o $$@

build/$(1)/libs/%.o: src/libs/src/%.c
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD) CC $(RESET) [$(1)],$$(patsubst build/$(1)/libs/%.o, src/libs/src/%.c, $$@))
	@$$(CC) $$(DEFINES_$(1)) $$(CFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/libs/%.o, src/libs/src/%.c, $$@) -o $$@

build/$(1)/libs/%.o: src/libs/src/%.cpp
	@mkdir -p $$(dir $$@)
	$$(call step,$(CYAN)$(BOLD)CXX $(RESET) [$(1)],$$(patsubst build/$(1)/libs/%.o, src/libs/src/%.cpp, $$@))
	@$$(CXX) $$(DEFINES_$(1)) $$(CXXFLAGS) -c $$(INCLUDES) -ffreestanding $$(patsubst build/$(1)/libs/%.o, src/libs/src/%.cpp, $$@) -o $$@

build/$(1)/libs/%.o: src/libs/src/%.asm
	@mkdir -p $$(dir $$@)
	$$(call step,$(MAGENTA)$(BOLD)ASM $(RESET) [$(1)],$$(patsubst build/$(1)/libs/%.o, src/libs/src/%.asm, $$@))
	@nasm -f elf64 $$(patsubst build/$(1)/libs/%.o, src/libs/src/%.asm, $$@) -o $$@

# Link this variant's kernel.bin
dist/x86_64/kernel_$(1).bin: $$(kernel_object_files_$(1)) $$(x86_64_object_files_$(1)) $$(libs_object_files_$(1)) $$(csh_object_files_$(1))
	@mkdir -p dist/x86_64
	@printf "$(DIM)[  --]$(RESET) $(MAGENTA)$(BOLD)LINK$(RESET) $(DIM)dist/x86_64/kernel_$(1).bin$(RESET)\n"
	@$$(LD) -n -o dist/x86_64/kernel_$(1).bin -T targets/x86_64/linker.ld $$^

.PHONY: build-$(1)

build-$(1): stepinit dist/x86_64/kernel_$(1).bin
	@mkdir -p targets/x86_64/iso/boot
	@cp dist/x86_64/kernel_$(1).bin targets/x86_64/iso/boot/kernel_$(1).bin
	@rm -f $(STEP_FILE)
	@printf "$(GREEN)$(BOLD)Build complete$(RESET) $(DIM)→ dist/x86_64/kernel_$(1).bin$(RESET)\n"
	@printf "  $(DIM)→ targets/x86_64/iso/boot/kernel_$(1).bin$(RESET)\n"

endef

$(foreach v,$(VARIANTS),$(eval $(call COMPILE_RULES,$(v))))

# ----------------------------------------------------------------------------
#  Program (.lhe) rules — shared across all kernel variants
# ----------------------------------------------------------------------------
targets/x86_64/iso/programs/%.lhe: src/programs/cd/%.c
	@mkdir -p $(dir $@)
	@mkdir -p build/programs/cd
	$(eval STEM := $*)
	$(call step,$(YELLOW)$(BOLD)LHE $(RESET),$(STEM).c)
	@$(CC) $(DEFINES) $(CFLAGS) $(INCLUDES) -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c $(patsubst targets/x86_64/iso/programs/%.lhe, src/programs/cd/%.c, $@) -o build/programs/cd/$(STEM).o
	@$(LD) -T src/programs/program.ld -o build/programs/cd/$(STEM).elf build/programs/cd/$(STEM).o
	@$(OBJCP) -O binary build/programs/cd/$(STEM).elf build/programs/cd/$(STEM).bin
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	@cat build/programs/cd/$(STEM).bin >> $@

targets/x86_64/disk/programs/%.lhe: src/programs/fat/%.c
	@mkdir -p $(dir $@)
	@mkdir -p build/programs/fat
	$(eval STEM := $*)
	$(call step,$(YELLOW)$(BOLD)LHE $(RESET),$(STEM).c)
	@$(CC) $(DEFINES) $(CFLAGS) $(INCLUDES) -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c $(patsubst targets/x86_64/disk/programs/%.lhe, src/programs/fat/%.c, $@) -o build/programs/fat/$(STEM).o
	@$(LD) -T src/programs/program.ld -o build/programs/fat/$(STEM).elf build/programs/fat/$(STEM).o
	@$(OBJCP) -O binary build/programs/fat/$(STEM).elf build/programs/fat/$(STEM).bin
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	@cat build/programs/fat/$(STEM).bin >> $@

targets/x86_64/iso/programs/%.lhe: src/programs/cd/%.cpp
	@mkdir -p $(dir $@)
	@mkdir -p build/programs/cd
	$(eval STEM := $*)
	$(call step,$(YELLOW)$(BOLD)LHE $(RESET),$(STEM).cpp)
	@$(CXX) $(DEFINES) $(CXXFLAGS) $(INCLUDES) -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c $(patsubst targets/x86_64/iso/programs/%.lhe, src/programs/cd/%.cpp, $@) -o build/programs/cd/$(STEM).o
	@$(LD) -T src/programs/program.ld -o build/programs/cd/$(STEM).elf build/programs/cd/$(STEM).o
	@$(OBJCP) -O binary build/programs/cd/$(STEM).elf build/programs/cd/$(STEM).bin
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	@cat build/programs/cd/$(STEM).bin >> $@

targets/x86_64/disk/programs/%.lhe: src/programs/fat/%.cpp
	@mkdir -p $(dir $@)
	@mkdir -p build/programs/fat
	$(eval STEM := $*)
	$(call step,$(YELLOW)$(BOLD)LHE $(RESET),$(STEM).cpp)
	@$(CXX) $(DEFINES) $(CXXFLAGS) $(INCLUDES) -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c $(patsubst targets/x86_64/disk/programs/%.lhe, src/programs/fat/%.cpp, $@) -o build/programs/fat/$(STEM).o
	@$(LD) -T src/programs/program.ld -o build/programs/fat/$(STEM).elf build/programs/fat/$(STEM).o
	@$(OBJCP) -O binary build/programs/fat/$(STEM).elf build/programs/fat/$(STEM).bin
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	@cat build/programs/fat/$(STEM).bin >> $@

.PHONY: build clean clean_all run build_clean stepinit

stepinit:
	@mkdir -p vault
	@mkdir -p build
	@rm -f $(STEP_FILE)
	@printf "$(BOLD)$(BLUE)[+] Building$(RESET) $(DIM)myos version $(VERSION)($(TOTAL_STEPS) steps, $(words $(VARIANTS)) variants)$(RESET)\n"

# Default build: produces all three kernel.bin files, copies each into
# targets/x86_64/iso/boot/ (as kernel_<variant>.bin) so grub.cfg can offer a
# boot menu entry per variant, then packages dist/x86_64/kernel.iso.
build: stepinit $(program_lhe_files) $(foreach v,$(VARIANTS),dist/x86_64/kernel_$(v).bin)
	@mkdir -p targets/x86_64/iso/boot
	@$(foreach v,$(VARIANTS),cp dist/x86_64/kernel_$(v).bin targets/x86_64/iso/boot/kernel_$(v).bin;)
	@cp dist/x86_64/kernel_$(ISO_VARIANT).bin targets/x86_64/iso/boot/kernel.bin
	@printf "$(DIM)[  --]$(RESET) $(MAGENTA)$(BOLD) ISO$(RESET) $(DIM)dist/x86_64/kernel_$(VERSION).iso$(RESET)\n"
	@grub-mkrescue /usr/lib/grub/i386-pc --modules="normal multiboot2 all_video video video_bochs gfxterm" -o dist/x86_64/kernel_$(VERSION).iso targets/x86_64/iso > /dev/null 2>&1
	@cp dist/x86_64/kernel_$(VERSION).iso vault/
	@next=$$(($(BN) + 1)); echo $$next > buildInfo/bn;
	@rm -f $(STEP_FILE)
	@printf "$(GREEN)$(BOLD)Build complete$(RESET)\n"
	@$(foreach v,$(VARIANTS),printf "  $(DIM)→ dist/x86_64/kernel_$(v).bin$(RESET)\n";)
	@$(foreach v,$(VARIANTS),printf "  $(DIM)→ targets/x86_64/iso/boot/kernel_$(v).bin$(RESET)\n";)
	@printf "  $(DIM)→ dist/x86_64/kernel_$(VERSION).iso$(RESET) (default boot: $(ISO_VARIANT))\n"

clean:
	@printf "$(YELLOW)$(BOLD)[clean]$(RESET) removing build artifacts...\n"
	@rm -rf build dist
	@rm -rf targets/x86_64/iso/data/*.lhe targets/x86_64/iso/boot/kernel.bin targets/x86_64/iso/boot/kernel_*.bin
	@printf "$(GREEN)Clean complete$(RESET)\n"

clean_all:
	@printf "$(YELLOW)$(BOLD)[clean_all]$(RESET) removing ALL build artifacts...\n"
	@rm -rf targets/x86_64/disk.img
	@rm -rf build dist
	@rm -rf targets/x86_64/iso/programs/*.lhe targets/x86_64/iso/boot/kernel.bin targets/x86_64/iso/boot/kernel_*.bin
	@printf "$(GREEN)Clean complete$(RESET)\n"

build_clean:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory build

run:
	@docker exec -it myos_cpp make --no-print-directory build_clean
	@if [ ! -f targets/x86_64/disk.img ]; then \
		printf "$(BLUE)$(BOLD)[disk]$(RESET) creating virtual disk...\n"; \
		dd if=/dev/zero of=targets/x86_64/disk.img bs=1M count=$(FAT_DISK_SIZE) status=none; \
		mkfs.fat -F 32 targets/x86_64/disk.img > /dev/null; \
	fi
	@if [ -d targets/x86_64/disk ] && [ "$$(ls -A targets/x86_64/disk 2>/dev/null)" ]; then \
		printf "$(BLUE)$(BOLD)[disk]$(RESET) copying files to image...\n"; \
		mkdir -p /tmp/os-disk-mount; \
		sudo mount -o loop targets/x86_64/disk.img /tmp/os-disk-mount; \
		sudo cp -r targets/x86_64/disk/. /tmp/os-disk-mount/; \
		sudo umount /tmp/os-disk-mount; \
	fi
	@printf "$(GREEN)$(BOLD)[run]$(RESET) starting QEMU...\n"
	@qemu-system-x86_64 \
		-drive file=targets/x86_64/disk.img,format=raw,if=ide,index=0,media=disk \
		-drive file=dist/x86_64/kernel_$(VERSION).iso,format=raw,if=ide,index=2,media=cdrom \
		-m 16G \
		-boot d \
		-vga std \
		-serial tcp:127.0.0.1:1234,server &
	@sleep 1
	@/mnt/c/Program\ Files/PuTTY/putty.exe -raw 127.0.0.1 -P 1234