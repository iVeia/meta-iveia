# Docker Overview

The Xilinx Yocto build process works out of the box on specific versions of
Ubuntu Linux (and other operating systems). When using Ubuntu, building with a
version different from the one tailored to the Xilinx Yocto source often
requires adding or upgrading packages through a trial-and-error process.

For quicker setup, Ubuntu Docker containers are provided at the correct
version to match the Xilinx Yocto source requirements.

# Building a Docker Container Image

To build a Docker image, first locate the appropriate Dockerfile in
`scripts/docker` for the version of Ubuntu you plan to use. The required
version is listed in the `README.md` under Requirements.

If Docker is not already installed, follow the installation instructions on
[docker.com](https://www.docker.com).

Build the Docker container with:

```
UBUNTU_VER=VV.VV  # See README.md for the correct version.
docker build \
    -t ubuntu-yocto:$UBUNTU_VER \
    -f ./scripts/docker/Dockerfile.ubuntu-yocto-$UBUNTU_VER .
```

Once built, navigate to a BSP project that has been downloaded and set up as
described in the `README.md`. Then start an interactive shell in the Docker
container:
```
docker run --rm -it -v "$(pwd):$(pwd)" -w "$(pwd)" ubuntu-yocto:$UBUNTU_VER bash
```

The `-v` and `-w` options allow you to edit the host files in the current
working directory from the docker bash shell.

From the bash shell, you can modify and build the source as described in
[BUILD.md](BUILD.md).
