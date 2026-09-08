#!/bin/bash

docker build buildenv -t myos-buildenv_cpp
docker run -d --rm --name myos_cpp -v "$(pwd)":/root/env myos-buildenv_cpp tail -f /dev/null