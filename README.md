# p2p-ring-implementation

Distributed P2P ring-based file sharing system with linear search, node management, and TCP-based communication.

## Docker workflow (Linux toolchain)

This project includes a `Dockerfile` to compile/run the code in a Linux environment.

### 1) Build the image

```bash
docker build -t p2p-ring .
```

Using the image name `p2p-ring` is recommended so the run commands stay simple.

### 2) Run each node

Start one container per node with:

```bash
docker run --rm -it -v "$PWD":/RING p2p-ring
```

The container starts an interactive shell and the project is available at `/RING`.

Inside each container:

```bash
cd /RING/src
make
```

Then run `./ring ...` as needed for node 1, node 2, etc.
