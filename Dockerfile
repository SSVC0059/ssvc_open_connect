# SSVC OpenConnect — сборка прошивки и тестирование
# Используется для: pio run, pio test -e native, pio test -e esp32-s3-devkitc-1-16m-test (без загрузки)
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PLATFORMIO_CORE_DIR=/.platformio

# Python, Node.js, g++, PlatformIO
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-pip \
    python3-venv \
    nodejs \
    npm \
    g++ \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# PlatformIO Core и зависимости Python (cert bundle, merge_bin)
RUN pip3 install --no-cache-dir platformio cryptography requests intelhex

WORKDIR /project

ENTRYPOINT ["pio"]
CMD ["run"]
