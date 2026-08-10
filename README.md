For  building the Docker image:
```
docker build buildenv -t myos-buildenv_cpp
```
For running the Docker container:
```
docker run -d --rm --name myos_cpp -v "$(pwd)":/root/env myos-buildenv_cpp tail -f /dev/null
```
For going inside the Docker container:
```
docker exec -it myos_cpp bash
```

Please also view the CHANGELOG file for information about what changed between versions

Please also view and edit the Makefile:277, it uses a Windows Path for PuTTY since this was originally coded in wsl