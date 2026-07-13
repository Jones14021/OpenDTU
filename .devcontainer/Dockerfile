# Use the official Python dev container image (Debian Bullseye)
FROM mcr.microsoft.com/devcontainers/python:1-3.11-bullseye

# Install dependencies required by ESP32 toolchains and PlatformIO
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        build-essential \
        curl \
        git \
        libusb-1.0-0 \
        libpython2.7 \
        xz-utils \
        udev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO CLI globally
RUN pip3 install --no-cache-dir --upgrade platformio

# Create a udev rule directory to prevent PIO warnings, though USB 
# pass-through will be handled by Docker runArgs in devcontainer.json
RUN mkdir -p /etc/udev/rules.d
