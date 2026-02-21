FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        git \
        build-essential \
        cmake \
        mingw-w64 \
        g++-mingw-w64-x86-64 \
        binutils-mingw-w64-x86-64 \
        ccache \
    && rm -rf /var/lib/apt/lists/*
