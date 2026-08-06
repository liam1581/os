CC := x86_64-elf-gcc
CXX := x86_64-elf-g++
LD := x86_64-elf-ld

kernel_c_source_files := $(shell find src/kernel -name *.c)
kernel_c_object_files := $(patsubst src/kernel/%.c, build/kernel/%.o, $(kernel_c_source_files))

kernel_cpp_source_files := $(shell find src/kernel -name *.cpp)
kernel_cpp_object_files := $(patsubst src/kernel/%.cpp, build/kernel/%.o, $(kernel_cpp_source_files))

csh_c_source_files := $(shell find src/csh -name *.c)
csh_c_object_files := $(patsubst src/csh/%.c, build/csh/%.o, $(csh_c_source_files))

csh_cpp_source_files := $(shell find src/csh -name *.cpp)
csh_cpp_object_files := $(patsubst src/csh/%.cpp, build/csh/%.o, $(csh_cpp_source_files))

x86_64_c_source_files := $(shell find src/x86_64 -name *.c)
x86_64_c_object_files := $(patsubst src/x86_64/%.c, build/x86_64/%.o, $(x86_64_c_source_files))

x86_64_asm_source_files := $(shell find src/x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst src/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

libs_c_source_files := $(shell find src/libs/src -name *.c)
libs_c_object_files := $(patsubst src/libs/src/%.c, build/libs/%.o, $(libs_c_source_files))

libs_cpp_source_files := $(shell find src/libs/src -name *.cpp)
libs_cpp_object_files := $(patsubst src/libs/src/%.cpp, build/libs/%.o, $(libs_cpp_source_files))

libs_asm_source_files := $(shell find src/libs/src -name *.asm)
libs_asm_object_files := $(patsubst src/libs/src/%.asm, build/libs/%.o, $(libs_asm_source_files))

program_asm_source_files := $(shell find src/programs -name *.asm)
program_lse_files := $(patsubst src/programs/%.asm, targets/x86_64/iso/data/%.lhe, $(program_asm_source_files))

program_c_source_files := $(shell find src/programs -name *.c)
program_c_lse_files := $(patsubst src/programs/%.c, targets/x86_64/iso/data/%.lhe, $(program_c_source_files))

kernel_object_files := $(kernel_c_object_files) $(kernel_cpp_object_files)
x86_64_object_files := $(x86_64_c_object_files) $(x86_64_asm_object_files)
csh_object_files := $(csh_c_object_files) $(csh_cpp_object_files)
libs_object_files := $(libs_c_object_files) $(libs_cpp_object_files) $(libs_asm_object_files)

INCLUDES := -I src/libs/include -I src/libs/include/kapi -I src/kernel/include -I src/kernel/cpp/include -I src/csh/include -I src/kernel/TESTING/include
DEFINES := -DKEYBOARD_QWERTZ -DDEBUG

CXXFLAGS := -std=c++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics

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

# Total number of compile/assemble/link-program steps (used for the [n/N] counter)
TOTAL_STEPS := $(words $(kernel_object_files) $(x86_64_object_files) $(libs_object_files) $(csh_object_files) $(program_lse_files) $(program_c_lse_files))

# $(call step,LABEL,TEXT) -- prints "[ n/N] LABEL  TEXT" and advances the counter
define step
	@n=$$(( $$(cat $(STEP_FILE) 2>/dev/null || echo 0) + 1 )); \
	echo $$n > $(STEP_FILE); \
	printf "$(DIM)[%2d/%2d]$(RESET) %b $(DIM)%s$(RESET)\n" $$n $(TOTAL_STEPS) "$(1)" "$(2)"
endef

build/kernel/%.o: src/kernel/%.c
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD) CC $(RESET),$(patsubst build/kernel/%.o, src/kernel/%.c, $@))
	@$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/kernel/%.o, src/kernel/%.c, $@) -o $@

build/kernel/%.o: src/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD)CXX $(RESET),$(patsubst build/kernel/%.o, src/kernel/%.cpp, $@))
	@$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/kernel/%.o, src/kernel/%.cpp, $@) -o $@

build/csh/%.o: src/csh/%.c
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD) CC $(RESET),$(patsubst build/csh/%.o, src/csh/%.c, $@))
	@$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/csh/%.o, src/csh/%.c, $@) -o $@

build/csh/%.o: src/csh/%.cpp
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD)CXX $(RESET),$(patsubst build/csh/%.o, src/csh/%.cpp, $@))
	@$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/csh/%.o, src/csh/%.cpp, $@) -o $@

build/x86_64/%.o: src/x86_64/%.c
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD) CC $(RESET),$(patsubst build/x86_64/%.o, src/x86_64/%.c, $@))
	@$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/x86_64/%.o, src/x86_64/%.c, $@) -o $@

build/x86_64/%.o: src/x86_64/%.cpp
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD)CXX $(RESET),$(patsubst build/x86_64/%.o, src/x86_64/%.cpp, $@))
	@$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/x86_64/%.o, src/x86_64/%.cpp, $@) -o $@

build/x86_64/%.o: src/x86_64/%.asm
	@mkdir -p $(dir $@)
	$(call step,$(MAGENTA)$(BOLD)ASM $(RESET),$(patsubst build/x86_64/%.o, src/x86_64/%.asm, $@))
	@nasm -f elf64 $(patsubst build/x86_64/%.o, src/x86_64/%.asm, $@) -o $@

build/libs/%.o: src/libs/src/%.c
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD) CC $(RESET),$(patsubst build/libs/%.o, src/libs/src/%.c, $@))
	@$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/libs/%.o, src/libs/src/%.c, $@) -o $@

build/libs/%.o: src/libs/src/%.cpp
	@mkdir -p $(dir $@)
	$(call step,$(CYAN)$(BOLD)CXX $(RESET),$(patsubst build/libs/%.o, src/libs/src/%.cpp, $@))
	@$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/libs/%.o, src/libs/src/%.cpp, $@) -o $@

build/libs/%.o: src/libs/src/%.asm
	@mkdir -p $(dir $@)
	$(call step,$(MAGENTA)$(BOLD)ASM $(RESET),$(patsubst build/libs/%.o, src/libs/src/%.asm, $@))
	@nasm -f elf64 $(patsubst build/libs/%.o, src/libs/src/%.asm, $@) -o $@

targets/x86_64/iso/data/%.lhe: src/programs/%.asm
	@mkdir -p $(dir $@)
	@mkdir -p build/programs
	$(eval STEM := $*)
	$(call step,$(YELLOW)$(BOLD)LSE $(RESET),$(STEM).asm)
	@nasm -f bin $< -o build/programs/$(STEM).bin
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	@cat build/programs/$(STEM).bin >> $@

targets/x86_64/iso/data/%.lhe: src/programs/%.c
	@mkdir -p $(dir $@)
	@mkdir -p build/programs
	$(eval STEM := $*)
	$(call step,$(YELLOW)$(BOLD)LSE $(RESET),$(STEM).c)
	@$(CC) $(DEFINES) $(INCLUDES) -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c $(patsubst targets/x86_64/iso/data/%.lhe, src/programs/%.c, $@) -o build/programs/$(STEM).o
	@$(LD) -T src/programs/program.ld -o build/programs/$(STEM).elf build/programs/$(STEM).o
	@x86_64-elf-objcopy -O binary build/programs/$(STEM).elf build/programs/$(STEM).bin
	@python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	@cat build/programs/$(STEM).bin >> $@


.PHONY: build clean run build_clean stepinit

stepinit:
	@mkdir -p build
	@rm -f $(STEP_FILE)
	@printf "$(BOLD)$(BLUE)[+] Building$(RESET) $(DIM)myos ($(TOTAL_STEPS) steps)$(RESET)\n"

build: stepinit $(kernel_object_files) $(x86_64_object_files) $(libs_object_files) $(program_c_lse_files) $(program_lse_files) $(csh_object_files)
	@mkdir -p dist/x86_64
	@printf "$(DIM)[  --]$(RESET) $(MAGENTA)$(BOLD)LINK$(RESET) $(DIM)dist/x86_64/kernel.bin$(RESET)\n"
	@$(LD) -n -o dist/x86_64/kernel.bin -T targets/x86_64/linker.ld $(kernel_object_files)  $(x86_64_object_files) $(libs_object_files) $(csh_object_files)
	@cp dist/x86_64/kernel.bin targets/x86_64/iso/boot/kernel.bin
	@printf "$(DIM)[  --]$(RESET) $(MAGENTA)$(BOLD) ISO$(RESET) $(DIM)dist/x86_64/kernel.iso$(RESET)\n"
	@grub-mkrescue /usr/lib/grub/i386-pc -o dist/x86_64/kernel.iso targets/x86_64/iso > /dev/null 2>&1
	@rm -f $(STEP_FILE)
	@printf "$(GREEN)$(BOLD)✔ Build complete$(RESET) $(DIM)→ dist/x86_64/kernel.iso$(RESET)\n"

clean:
	@printf "$(YELLOW)$(BOLD)[clean]$(RESET) removing build artifacts...\n"
	@rm -rf build dist
	@rm -rf targets/x86_64/iso/data/*.lhe targets/x86_64/iso/boot/kernel.bin
	@printf "$(GREEN)✔ Clean complete$(RESET)\n"

build_clean:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory build

run:
	@docker exec -it myos_cpp make --no-print-directory build_clean
	@if [ ! -f targets/x86_64/disk.img ]; then \
		printf "$(BLUE)$(BOLD)[disk]$(RESET) creating virtual disk...\n"; \
		dd if=/dev/zero of=targets/x86_64/disk.img bs=1M count=8096 status=none; \
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
		-drive file=dist/x86_64/kernel.iso,format=raw,if=ide,index=2,media=cdrom \
		-m 12G \
		-boot d \
		-serial tcp:127.0.0.1:1234,server &
	@sleep 1
	@/mnt/c/Program\ Files/PuTTY/putty.exe -raw 127.0.0.1 -P 1234