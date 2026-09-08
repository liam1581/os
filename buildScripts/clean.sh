#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

clean_standard() {
  printf "${YELLOW}${BOLD}[clean]${RESET} removing build artifacts...\n"
  rm -rf build dist
  rm -rf targets/x86_64/disk/programs/*.leh targets/x86_64/iso/programs/*.leh targets/x86_64/iso/boot/kernel.bin targets/x86_64/iso/boot/kernel_*.bin
  printf "${GREEN}Clean complete${RESET}\n"
}

clean_all() {
  printf "${YELLOW}${BOLD}[clean_all]${RESET} removing ALL build artifacts...\n"
  rm -rf targets/x86_64/disk.img
  rm -rf build dist
  rm -rf targets/x86_64/disk/programs/*.leh targets/x86_64/iso/programs/*.leh targets/x86_64/iso/boot/kernel.bin targets/x86_64/iso/boot/kernel_*.bin
  printf "${GREEN}Clean complete${RESET}\n"
}

if [ "$1" = "all" ]; then
  clean_all
else
  clean_standard
fi