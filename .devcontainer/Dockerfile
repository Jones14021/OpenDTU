# Use the official Python dev container image (Debian Bullseye)
FROM mcr.microsoft.com/devcontainers/python:1-3.11-bullseye

ARG WEBSOCAT_VERSION=1.13.0

# Remove the broken Yarn repository list before running apt-get update
RUN rm -f /etc/apt/sources.list.d/yarn.list

# Install dependencies required by ESP32 toolchains, PlatformIO, and serial communication
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        build-essential \
        bubblewrap \
        ca-certificates \
        curl \
        git \
        libusb-1.0-0 \
        libpython2.7 \
        xz-utils \
        udev \
        minicom \
        picocom \
        screen \
        socat \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install websocat from release binaries because bullseye apt does not provide it.
RUN set -eux; \
        case "${TARGETARCH:-amd64}" in \
            amd64) websocat_asset='websocat.x86_64-unknown-linux-musl' ;; \
            arm64) websocat_asset='websocat.aarch64-unknown-linux-musl' ;; \
            *) echo "Unsupported TARGETARCH for websocat: ${TARGETARCH}"; exit 1 ;; \
        esac; \
        curl -fL "https://github.com/vi/websocat/releases/download/v${WEBSOCAT_VERSION}/${websocat_asset}" \
            -o /usr/local/bin/websocat; \
        chmod +x /usr/local/bin/websocat; \
        /usr/local/bin/websocat --version

# Install PlatformIO CLI and esptool globally
RUN pip3 install --no-cache-dir --upgrade platformio esptool

# Keep PlatformIO core data in the workspace mount so rebuilds do not re-download toolchains/packages.
ENV PLATFORMIO_CORE_DIR=/workspaces/.platformio
RUN mkdir -p /workspaces/.platformio \
    && chown -R vscode:vscode /workspaces/.platformio

# Create a udev rule directory to prevent PIO warnings
RUN mkdir -p /etc/udev/rules.d

# Install a terminal launcher that activates the serial device group in each new terminal.
COPY terminal-with-serial-group.sh /usr/local/bin/terminal-with-serial-group
RUN chmod +x /usr/local/bin/terminal-with-serial-group

# Add the vscode user to common serial group.
# A startup hook in devcontainer.json dynamically maps host-specific serial GIDs.
RUN usermod -aG dialout vscode
