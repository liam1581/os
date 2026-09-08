#!/bin/bash

GCC_VERSION=$1

export PREFIX="/usr/local"
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

cd $PREFIX/src/gcc-${GCC_VERSION}/gcc

# Wire in the no-red-zone multilib (config/i386/t-x86_64-elf) for the
# x86_64-*-elf* target. This used to be done via config.gcc.patch, a
# unified diff -- but that diff's context lines (which included
# dbxelf.h in the target's tm_file) no longer match current GCC's
# config.gcc (dbxelf.h was dropped from that target upstream at some
# point after GCC 13), so `patch` would fail or mismatch on newer GCC
# versions. Anchoring on the "x86_64-*-elf*)" target-triple case label
# itself instead -- that's a much more stable point of reference than
# the exact wording of the tm_file line next to it.
if grep -q 'i386/t-x86_64-elf' config.gcc; then
	echo "config.gcc already wires in i386/t-x86_64-elf, skipping"
elif grep -q '^x86_64-\*-elf\*)$' config.gcc; then
	sed -i '/^x86_64-\*-elf\*)$/a\	tmake_file="${tmake_file} i386/t-x86_64-elf" # include the new multilib configuration' config.gcc
else
	echo "ERROR: could not find the 'x86_64-*-elf*)' target stanza in config.gcc." >&2
	echo "GCC ${GCC_VERSION}'s config.gcc has likely been restructured; update" >&2
	echo "build-gcc.sh's insertion logic (see buildenv/gcc/t-x86_64-elf) before" >&2
	echo "continuing, or the resulting toolchain will silently be missing the" >&2
	echo "-mno-red-zone libgcc multilib this kernel's build depends on." >&2
	exit 1
fi

cd $PREFIX/src
mkdir build-gcc
cd build-gcc
../gcc-${GCC_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make -j `nproc` all-gcc
make -j `nproc` all-target-libgcc
make install-gcc
make install-target-libgcc

cd $PREFIX/src
rm -rf build-gcc.sh build-gcc gcc-${GCC_VERSION}