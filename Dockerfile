FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        make \
        gcc \
        g++ \
        libc6-dev \
        pkg-config && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /RING
CMD ["/bin/bash"]

