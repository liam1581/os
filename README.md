please run the docker.sh file before everything else!

then you can do build.sh run to build_clean and run the OS

build.sh options:
- clean (cleans the build files and leh files)
- clean all (cleans ALL generated files, including the fat32 img)
- build (builds the os)
- build-VERSION (either testing, production or kernelpanic to just build one specific version of the OS)
- build_clean (runs clean and then build)
- run (runs build_clean and then qemu + putty)

Please also view the CHANGELOG file for information about what changed between versions