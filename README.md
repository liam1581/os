For  building the Docker image:
```
docker build buildenv -t myos-buildenv
```
For running the Docker container:
```
docker run -d --rm --name myos -v "$(pwd)":/root/env myos-buildenv tail -f /dev/null
```
For going inside the Docker container:
```
docker exec -it myos bash
```