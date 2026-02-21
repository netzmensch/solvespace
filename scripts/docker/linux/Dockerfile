FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        git \
        build-essential \
        cmake \
        zlib1g-dev \
        libpng-dev \
        libcairo2-dev \
        libfreetype6-dev \
        libjson-c-dev \
        libfontconfig1-dev \
        libpangomm-1.4-dev \
        libgl-dev \
        libglu-dev \
        libspnav-dev \
        libgtkmm-3.0-dev \
        qt6-base-dev \
        ccache \
    && rm -rf /var/lib/apt/lists/*
