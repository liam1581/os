CC := x86_64-elf-gcc
CXX := x86_64-elf-g++
LD := x86_64-elf-ld

kernel_c_source_files := $(shell find src/kernel -name *.c)
kernel_c_object_files := $(patsubst src/kernel/%.c, build/kernel/%.o, $(kernel_c_source_files))

kernel_cpp_source_files := $(shell find src/kernel/cpp -name *.cpp)
kernel_cpp_object_files := $(patsubst src/kernel/cpp/%.cpp, build/kernel/cpp/%.o, $(kernel_cpp_source_files))

csh_source_files := $(shell find src/csh -name *.c)
csh_object_files := $(patsubst src/csh/%.c, build/csh/%.o, $(csh_source_files))

x86_64_c_source_files := $(shell find src/x86_64 -name *.c)
x86_64_c_object_files := $(patsubst src/x86_64/%.c, build/x86_64/%.o, $(x86_64_c_source_files))

x86_64_asm_source_files := $(shell find src/x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst src/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

libs_c_source_files := $(shell find src/libs/source -name *.c)
libs_c_object_files := $(patsubst src/libs/source/%.c, build/libs/%.o, $(libs_c_source_files))

libs_cpp_source_files := $(shell find src/libs/source -name *.cpp)
libs_cpp_object_files := $(patsubst src/libs/source/%.cpp, build/libs/%.o, $(libs_cpp_source_files))

libs_asm_source_files := $(shell find src/libs/source -name *.asm)
libs_asm_object_files := $(patsubst src/libs/source/%.asm, build/libs/%.o, $(libs_asm_source_files))

program_asm_source_files := $(shell find src/programs -name *.asm)
program_lse_files := $(patsubst src/programs/%.asm, targets/x86_64/iso/data/%.lhe, $(program_asm_source_files))

program_c_source_files := $(shell find src/programs -name *.c)
program_c_lse_files := $(patsubst src/programs/%.c, targets/x86_64/iso/data/%.lhe, $(program_c_source_files))

kernel_object_files := $(kernel_c_object_files) $(kernel_cpp_object_files)
x86_64_object_files := $(x86_64_c_object_files) $(x86_64_asm_object_files)
libs_object_files := $(libs_c_object_files) $(libs_cpp_object_files) $(libs_asm_object_files)

INCLUDES := -I src/libs/include -I src/libs/include/kapi -I src/kernel/include -I src/kernel/cpp/include -I src/csh/include
DEFINES := -DKEYBOARD_QWERTZ -DDEBUG

# C++ is freestanding here too (no libstdc++ is built for the x86_64-elf
# cross toolchain), so exceptions/RTTI/threadsafe-statics/__cxa_atexit are
# all switched off - they'd otherwise pull in runtime support that doesn't
# exist. See src/libs/source/cpp_support.cpp for the small amount of C++
# ABI glue (operator new/delete, __cxa_pure_virtual, global constructors)
# that's provided by hand instead.
CXXFLAGS := -std=c++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics

build/kernel/%.o: src/kernel/%.c
	mkdir -p $(dir $@)
	$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/kernel/%.o, src/kernel/%.c, $@) -o $@

build/kernel/%.o: src/kernel/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/kernel/%.o, src/kernel/%.cpp, $@) -o $@

build/csh/%.o: src/csh/%.c
	mkdir -p $(dir $@)
	$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/csh/%.o, src/csh/%.c, $@) -o $@

build/x86_64/%.o: src/x86_64/%.c
	mkdir -p $(dir $@)
	$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/x86_64/%.o, src/x86_64/%.c, $@) -o $@

build/x86_64/%.o: src/x86_64/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/x86_64/%.o, src/x86_64/%.cpp, $@) -o $@

build/x86_64/%.o: src/x86_64/%.asm
	mkdir -p $(dir $@)
	nasm -f elf64 $(patsubst build/x86_64/%.o, src/x86_64/%.asm, $@) -o $@

build/libs/%.o: src/libs/source/%.c
	mkdir -p $(dir $@)
	$(CC) $(DEFINES) -c $(INCLUDES) -ffreestanding $(patsubst build/libs/%.o, src/libs/source/%.c, $@) -o $@

build/libs/%.o: src/libs/source/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(DEFINES) $(CXXFLAGS) -c $(INCLUDES) -ffreestanding $(patsubst build/libs/%.o, src/libs/source/%.cpp, $@) -o $@

build/libs/%.o: src/libs/source/%.asm
	mkdir -p $(dir $@)
	nasm -f elf64 $(patsubst build/libs/%.o, src/libs/source/%.asm, $@) -o $@

targets/x86_64/iso/data/%.lhe: src/programs/%.asm
	mkdir -p $(dir $@)
	mkdir -p build/programs
	$(eval STEM := $*)
	nasm -f bin $< -o build/programs/$(STEM).bin
	python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	cat build/programs/$(STEM).bin >> $@

targets/x86_64/iso/data/%.lhe: src/programs/%.c
	mkdir -p $(dir $@)
	mkdir -p build/programs
	$(eval STEM := $*)
	$(CC) $(DEFINES) $(INCLUDES) -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c $(patsubst targets/x86_64/iso/data/%.lhe, src/programs/%.c, $@) -o build/programs/$(STEM).o
	$(LD) -T src/programs/program.ld -o build/programs/$(STEM).elf build/programs/$(STEM).o
	x86_64-elf-objcopy -O binary build/programs/$(STEM).elf build/programs/$(STEM).bin
	python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF,0x4C,0x53,0x4F,0x53,0x46,0x48,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0xFF]))" > $@
	cat build/programs/$(STEM).bin >> $@


.PHONY: build clean run build_clean

build: $(kernel_object_files) $(csh_object_files) $(x86_64_object_files) $(libs_object_files) $(program_lse_files) $(program_c_lse_files)
	mkdir -p dist/x86_64
	$(LD) -n -o dist/x86_64/kernel.bin -T targets/x86_64/linker.ld $(kernel_object_files) $(csh_object_files) $(x86_64_object_files) $(libs_object_files)
	cp dist/x86_64/kernel.bin targets/x86_64/iso/boot/kernel.bin
	grub-mkrescue /usr/lib/grub/i386-pc -o dist/x86_64/kernel.iso targets/x86_64/iso

clean:
	rm -rf build dist
	rm -rf targets/x86_64/iso/data/*.lhe targets/x86_64/iso/boot/kernel.bin

build_clean:
	$(MAKE) clean
	$(MAKE) build

run:
	docker exec -it myos make build_clean
	@if [ ! -f targets/x86_64/disk.img ]; then \
		echo "Creating virtual disk..."; \
		dd if=/dev/zero of=targets/x86_64/disk.img bs=1M count=8096; \
		mkfs.fat -F 32 targets/x86_64/disk.img; \
	fi
	@if [ -d targets/x86_64/disk ] && [ "$$(ls -A targets/x86_64/disk 2>/dev/null)" ]; then \
		echo "Copying disk files to image..."; \
		mkdir -p /tmp/os-disk-mount; \
		sudo mount -o loop targets/x86_64/disk.img /tmp/os-disk-mount; \
		sudo cp -r targets/x86_64/disk/. /tmp/os-disk-mount/; \
		sudo umount /tmp/os-disk-mount; \
	fi
	qemu-system-x86_64 \
		-drive file=targets/x86_64/disk.img,format=raw,if=ide,index=0,media=disk \
		-drive file=dist/x86_64/kernel.iso,format=raw,if=ide,index=2,media=cdrom \
		-m 12G \
		-boot d \
		-serial tcp:127.0.0.1:1234,server &
	sleep 1
	/mnt/c/Program\ Files/PuTTY/putty.exe -raw 127.0.0.1 -P 1234