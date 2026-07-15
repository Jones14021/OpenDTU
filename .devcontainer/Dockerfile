# Use the official Python dev container image (Debian Bullseye)
FROM mcr.microsoft.com/devcontainers/python:1-3.11-bullseye

# Remove the broken Yarn repository list before running apt-get update
RUN rm -f /etc/apt/sources.list.d/yarn.list

# Install dependencies required by ESP32 toolchains, PlatformIO, and serial communication
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        build-essential \
        curl \
        git \
        libusb-1.0-0 \
        libpython2.7 \
        xz-utils \
        udev \
        minicom \
        picocom \
        screen \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO CLI and esptool globally
RUN pip3 install --no-cache-dir --upgrade platformio esptool

# Create a udev rule directory to prevent PIO warnings
RUN mkdir -p /etc/udev/rules.d

# Add the vscode user to the dialout group so it can access serial devices (e.g. /dev/ttyUSB0)
RUN usermod -aG dialout vscode
