# P2P Ring Implementation for File Sharing

Distributed P2P ring-based file sharing system with linear search, node management, and TCP-based communication, written in C.


## Key features
- Ring topology (each node stores its successor; an n-node network forms a logical ring)
- Linear search (lookup) across the ring with configurable hop limit
- Zero-copy transfer primitives: sendfile() for sending and mmap() for receiving
- Single executable (ring) — each process acts both as client and server
- Concurrent server: one detached thread per incoming request


## Project Structure
- `Dockerfile` — container image to build and run the project in a Linux environment (Ubuntu 24.04)
- `src/` — C sources and Makefile
  - `ring_cln.c` — client-side API and logic
  - `ring_srv.c` — server-side request handling and thread logic
  - `common.c` — helper utilities (sockets, thread creation, etc.)
  - `include/` — public headers used by the code

---


## Build
### 1) Build the Docker image:

```bash
docker build -t p2p-ring .
```

## Run
### 1) Start a container 
Mount the repository into `/RING` so you can compile/run inside the image:

```bash
docker run --rm -it -v "$PWD":/RING p2p-ring
# inside container:
cd /RING/src
make
```

### 2) Start nodes
Each node is a separate process (different containers):

```bash
# create a directory to share inside the container
mkdir -p /tmp/dir1
# start the first node (creates a new ring)
./ring /tmp/dir1

# in a second container add a node joining the first using the first node's IP and port
mkdir -p /tmp/dir2
./ring /tmp/dir2 <FIRST_NODE_IP> <FIRST_NODE_PORT>
```

#### Docker example. Start 2 containers:

```bash
# build image (on host)
docker build -t p2p-ring .

# terminal A: run first node
docker run --rm -it -v "$PWD":/RING p2p-ring

# inside container: 
cd /RING/src
make
mkdir /tmp/dir1 
./ring /tmp/dir1

# terminal B: run second node, joining first (replace IP/PORT shown in terminal A)
docker run --rm -it -v "$PWD":/RING p2p-ring

# inside container:
cd /RING/src 
make
mkdir /tmp/dir2 
./ring /tmp/dir2 <IP_A> <PORT_A>
```

## Usage
- Start the `ring` executable with arguments:
  - `./ring <shared_dir>` — create a new ring with a single node
  - `./ring <shared_dir> <contact_host> <contact_port>` — join an existing ring by contacting the given node

Once running the interactive menu lets you perform operations such as:
- I — show local node info
- P — get PID from a remote node
- S — show successor
- R — remote successor (ask another node for its successor)
- U — successor of a node's successor
- D — download a file directly from a remote node
- L — lookup a filename across the ring with a hop limit
- G — lookup + download (convenience)