#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

compile_variant() {
  local v="$1"
  local var_defines
  var_defines=$(get_defines_for_variant "$v")
  local obj_list=()

  # 1. Kernel C/CPP
  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/kernel/}"
    local obj="build/${v}/kernel/${rel%.c}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD} CC ${RESET} [${v}]" "$src"
      $CC $var_defines $CFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/kernel" ".c")

  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/kernel/}"
    local obj="build/${v}/kernel/${rel%.cpp}.o"
    mkdir -p "$(dirname "$obj")"
    
    local cxx_flags_to_use="$CXXFLAGS"
    if [ "$rel" = "commands.cpp" ]; then
      cxx_flags_to_use="$CXXFLAGS_SSE_OK"
    fi

    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD}CXX ${RESET} [${v}]" "$src"
      $CXX $var_defines $cxx_flags_to_use -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/kernel" ".cpp")

  # 2. Shell C/CPP
  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/csh/}"
    local obj="build/${v}/csh/${rel%.c}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD} CC ${RESET} [${v}]" "$src"
      $CC $var_defines $CFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/csh" ".c")

  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/csh/}"
    local obj="build/${v}/csh/${rel%.cpp}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD}CXX ${RESET} [${v}]" "$src"
      $CXX $var_defines $CXXFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/csh" ".cpp")

  # 3. x86_64 C/CPP/ASM
  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/x86_64/}"
    local obj="build/${v}/x86_64/${rel%.c}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD} CC ${RESET} [${v}]" "$src"
      $CC $var_defines $CFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/x86_64" ".c")

  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/x86_64/}"
    local obj="build/${v}/x86_64/${rel%.cpp}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD}CXX ${RESET} [${v}]" "$src"
      $CXX $var_defines $CXXFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/x86_64" ".cpp")

  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/x86_64/}"
    local obj="build/${v}/x86_64/${rel%.asm}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${MAGENTA}${BOLD}ASM ${RESET} [${v}]" "$src"
      nasm -f elf64 "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/x86_64" ".asm")

  # 4. Libs C/CPP/ASM
  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/libs/src/}"
    local obj="build/${v}/libs/${rel%.c}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD} CC ${RESET} [${v}]" "$src"
      $CC $var_defines $CFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/libs/src" ".c")

  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/libs/src/}"
    local obj="build/${v}/libs/${rel%.cpp}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${CYAN}${BOLD}CXX ${RESET} [${v}]" "$src"
      $CXX $var_defines $CXXFLAGS -c $INCLUDES -ffreestanding "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/libs/src" ".cpp")

  while IFS= read -r src; do
    [ -z "$src" ] && continue
    local rel="${src#src/libs/src/}"
    local obj="build/${v}/libs/${rel%.asm}.o"
    mkdir -p "$(dirname "$obj")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
      step "${MAGENTA}${BOLD}ASM ${RESET} [${v}]" "$src"
      nasm -f elf64 "$src" -o "$obj"
    fi
    obj_list+=("$obj")
  done < <(find_files "src/libs/src" ".asm")

  mkdir -p dist/x86_64
  printf "${DIM}[  --]${RESET} ${MAGENTA}${BOLD}LINK${RESET} ${DIM}dist/x86_64/kernel_${v}.bin${RESET}\n"
  $LD -n -o "dist/x86_64/kernel_${v}.bin" -T targets/x86_64/linker.ld "${obj_list[@]}"
}

compile_programs() {
  local dirs=("cd:iso" "fat:disk")
  
  for entry in "${dirs[@]}"; do
    IFS=":" read -r p_dir p_target <<< "$entry"
    
    while IFS= read -r src; do
      [ -z "$src" ] && continue
      local filename=$(basename "$src")
      local stem="${filename%.c}"
      local target_leh="targets/x86_64/${p_target}/programs/${stem}.leh"
      
      mkdir -p "targets/x86_64/${p_target}/programs" "build/programs/${p_dir}"
      step "${YELLOW}${BOLD}LHE ${RESET}" "${stem}.c"

      $CC $DEFINES $CFLAGS $INCLUDES -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c "$src" -o "build/programs/${p_dir}/${stem}.o"
      $LD -T src/programs/program.ld -o "build/programs/${p_dir}/${stem}.elf" "build/programs/${p_dir}/${stem}.o"
      $OBJCP -O binary "build/programs/${p_dir}/${stem}.elf" "build/programs/${p_dir}/${stem}.bin"
      python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF, 0x4C, 0x45, 0x48, 0x00, 0x00, 0x00, 0xFF]))" > "$target_leh"
      cat "build/programs/${p_dir}/${stem}.bin" >> "$target_leh"
    done < <(find_files "src/programs/${p_dir}" ".c")

    while IFS= read -r src; do
      [ -z "$src" ] && continue
      local filename=$(basename "$src")
      local stem="${filename%.cpp}"
      local target_leh="targets/x86_64/${p_target}/programs/${stem}.leh"
      
      mkdir -p "targets/x86_64/${p_target}/programs" "build/programs/${p_dir}"
      step "${YELLOW}${BOLD}LHE ${RESET}" "${stem}.cpp"

      $CXX $DEFINES $CXXFLAGS $INCLUDES -ffreestanding -nostdlib -fno-pie -fno-pic -fcf-protection=none -c "$src" -o "build/programs/${p_dir}/${stem}.o"
      $LD -T src/programs/program.ld -o "build/programs/${p_dir}/${stem}.elf" "build/programs/${p_dir}/${stem}.o"
      $OBJCP -O binary "build/programs/${p_dir}/${stem}.elf" "build/programs/${p_dir}/${stem}.bin"
      python3 -c "import sys; sys.stdout.buffer.write(bytes([0xFF, 0x4C, 0x45, 0x48, 0x00, 0x00, 0x00, 0xFF]))" > "$target_leh"
      cat "build/programs/${p_dir}/${stem}.bin" >> "$target_leh"
    done < <(find_files "src/programs/${p_dir}" ".cpp")
  done
}

build_variant_target() {
  local v="$1"
  stepinit
  compile_variant "$v"
  mkdir -p targets/x86_64/iso/boot
  cp "dist/x86_64/kernel_${v}.bin" "targets/x86_64/iso/boot/kernel_${v}.bin"
  rm -f "$STEP_FILE"
  printf "${GREEN}${BOLD}Build complete${RESET} ${DIM}→ dist/x86_64/kernel_${v}.bin${RESET}\n"
  printf "  ${DIM}→ targets/x86_64/iso/boot/kernel_${v}.bin${RESET}\n"
}

build_all() {
  stepinit
  compile_programs
  for v in "${VARIANTS[@]}"; do
    compile_variant "$v"
  done

  mkdir -p targets/x86_64/iso/boot
  for v in "${VARIANTS[@]}"; do
    cp "dist/x86_64/kernel_${v}.bin" "targets/x86_64/iso/boot/kernel_${v}.bin"
  done
  cp "dist/x86_64/kernel_${ISO_VARIANT}.bin" targets/x86_64/iso/boot/kernel.bin

  printf "${DIM}[  --]${RESET} ${MAGENTA}${BOLD} ISO${RESET} ${DIM}dist/x86_64/kernel_${VERSION}.iso${RESET}\n"
  echo "${VERSION}" > targets/x86_64/iso/data/ver.txt
  grub-mkrescue /usr/lib/grub/i386-pc --modules="normal multiboot2 all_video video video_bochs gfxterm" -o "dist/x86_64/kernel_${VERSION}.iso" targets/x86_64/iso > /dev/null 2>&1
  
  cp "dist/x86_64/kernel_${VERSION}.iso" vault/
  
  local next=$((BN + 1))
  echo "$next" > buildInfo/bn

  rm -f "$STEP_FILE"
  printf "${GREEN}${BOLD}Build complete${RESET}\n"
  for v in "${VARIANTS[@]}"; do
    printf "  ${DIM}→ dist/x86_64/kernel_${v}.bin${RESET}\n"
  done
  for v in "${VARIANTS[@]}"; do
    printf "  ${DIM}→ targets/x86_64/iso/boot/kernel_${v}.bin${RESET}\n"
  done
  printf "  ${DIM}→ dist/x86_64/kernel_${VERSION}.iso${RESET} (default boot: ${ISO_VARIANT})\n"
}

# Sub-command dispatcher for build actions
TARGET="$1"
case "$TARGET" in
  stepinit)
    stepinit
    ;;
  testing|production|kernelpanic)
    build_variant_target "$TARGET"
    ;;
  *)
    build_all
    ;;
esac