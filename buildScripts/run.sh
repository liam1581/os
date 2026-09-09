#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

docker exec -it myos_cpp ./build.sh build_clean

if [ ! -f targets/x86_64/disk.img ]; then
  printf "${BLUE}${BOLD}[disk]${RESET} creating virtual disk...\n"
  dd if=/dev/zero of=targets/x86_64/disk.img bs=1M count=$FAT_DISK_SIZE status=none
  mkfs.fat -F 32 targets/x86_64/disk.img > /dev/null
fi

if [ -d targets/x86_64/disk ] && [ "$(ls -A targets/x86_64/disk 2>/dev/null)" ]; then
  printf "${BLUE}${BOLD}[disk]${RESET} copying files to image...\n"
  mkdir -p /tmp/os-disk-mount
  sudo mount -o loop targets/x86_64/disk.img /tmp/os-disk-mount
  sudo cp -r targets/x86_64/disk/. /tmp/os-disk-mount/
  sudo umount /tmp/os-disk-mount
fi

printf "${GREEN}${BOLD}[run]${RESET} starting QEMU...\n"
qemu-system-x86_64 \
  -drive file=targets/x86_64/disk.img,format=raw,if=ide,index=0,media=disk \
  -drive file="dist/x86_64/kernel_${VERSION}.iso",format=raw,if=ide,index=2,media=cdrom \
  -m 16G \
  -boot d \
  -vga std \
  -d int,cpu_reset -D qemu-crash.log \
  -debugcon stdio \
  -serial tcp:127.0.0.1:1234,server &

sleep 1
putty -raw 127.0.0.1 -P 1234