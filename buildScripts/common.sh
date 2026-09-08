#!/usr/bin/env bash
set -e

# Variable Definitions
CC="x86_64-elf-gcc"
CXX="x86_64-elf-g++"
LD="x86_64-elf-ld"
OBJCP="x86_64-elf-objcopy"

FAT_DISK_SIZE=1024

TZ_VAL="Europe/Berlin"
DD=$(TZ=$TZ_VAL date +%d)
MM=$(TZ=$TZ_VAL date +%m)
YYYY=$(TZ=$TZ_VAL date +%Y)
HH=$(TZ=$TZ_VAL date +%H)
MIN=$(TZ=$TZ_VAL date +%M)

MJ=$(cat buildInfo/mj 2>/dev/null || echo "0")
MN=$(cat buildInfo/mn 2>/dev/null || echo "0")
BN=$(cat buildInfo/bn 2>/dev/null || echo "0")

VERSION="${DD}${MM}${YYYY}.${HH}${MIN}-${MJ}.${MN}-${BN}"

VARIANTS=("testing" "production" "kernelpanic")

DEFINES_testing="-DKEYBOARD_GERMAN -DDEBUG_QEMU -DTESTING"
DEFINES_production="-DKEYBOARD_GERMAN -DDEBUG_QEMU -DPRODUCTION"
DEFINES_kernelpanic="-DKEYBOARD_GERMAN -DDEBUG_QEMU -DKERNELPANIC"

ISO_VARIANT="testing"
DEFINES="-DKEYBOARD_GERMAN -DDEBUG_QEMU -DPRODUCTION"

INCLUDES="-I src/libs/include -I src/libs/include/kapi -I src/kernel/include -I src/kernel/cpp/include -I src/csh/include -I src/kernel/TESTING/include"
KERNEL_SAFETY_FLAGS="-mno-red-zone -mno-mmx -mno-sse -mno-sse2"

CFLAGS="${KERNEL_SAFETY_FLAGS}"
CXXFLAGS="-std=c++26 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics ${KERNEL_SAFETY_FLAGS}"
CXXFLAGS_SSE_OK="-std=c++26 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mgeneral-regs-only"

# Pretty Output Colors
BOLD="\033[1m"
DIM="\033[2m"
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
MAGENTA="\033[35m"
CYAN="\033[36m"
RESET="\033[0m"

STEP_FILE="build/.step"

# Helper Functions
get_defines_for_variant() {
  local variant="$1"
  case "$variant" in
    testing)     echo "$DEFINES_testing" ;;
    production)  echo "$DEFINES_production" ;;
    kernelpanic) echo "$DEFINES_kernelpanic" ;;
  esac
}

find_files() {
  local dir="$1"
  local ext="$2"
  if [ -d "$dir" ]; then
    find "$dir" -name "*$ext" 2>/dev/null || true
  fi
}

calculate_total_steps() {
  local count=0
  
  count=$((count + $(find_files "src/programs/cd" ".c" | wc -l)))
  count=$((count + $(find_files "src/programs/fat" ".c" | wc -l)))
  count=$((count + $(find_files "src/programs/cd" ".cpp" | wc -l)))
  count=$((count + $(find_files "src/programs/fat" ".cpp" | wc -l)))

  local k_c=$(find_files "src/kernel" ".c" | wc -l)
  local k_cpp=$(find_files "src/kernel" ".cpp" | wc -l)
  local csh_c=$(find_files "src/csh" ".c" | wc -l)
  local csh_cpp=$(find_files "src/csh" ".cpp" | wc -l)
  local x_c=$(find_files "src/x86_64" ".c" | wc -l)
  local x_asm=$(find_files "src/x86_64" ".asm" | wc -l)
  local l_c=$(find_files "src/libs/src" ".c" | wc -l)
  local l_cpp=$(find_files "src/libs/src" ".cpp" | wc -l)
  local l_asm=$(find_files "src/libs/src" ".asm" | wc -l)

  local single_variant_steps=$((k_c + k_cpp + csh_c + csh_cpp + x_c + x_asm + l_c + l_cpp + l_asm))
  local total_variant_steps=$((single_variant_steps * ${#VARIANTS[@]}))

  echo $((count + total_variant_steps))
}

step() {
  local label="$1"
  local text="$2"
  local n=1
  if [ -f "$STEP_FILE" ]; then
    n=$(($(cat "$STEP_FILE" 2>/dev/null || echo 0) + 1))
  fi
  echo "$n" > "$STEP_FILE"
  printf "${DIM}[%2d/%2d]${RESET} %b ${DIM}%s${RESET}\n" "$n" "$TOTAL_STEPS" "$label" "$text"
}

stepinit() {
  mkdir -p vault build
  rm -f "$STEP_FILE"
  TOTAL_STEPS=$(calculate_total_steps)
  printf "${BOLD}${BLUE}[+] Building${RESET} ${DIM}myos version ${VERSION}(${TOTAL_STEPS} steps, ${#VARIANTS[@]} variants)${RESET}\n"
}