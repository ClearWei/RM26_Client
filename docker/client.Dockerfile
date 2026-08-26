ARG RM26_CLIENT_BASE_IMAGE=ubuntu:24.04
FROM ${RM26_CLIENT_BASE_IMAGE}

ARG DEBIAN_FRONTEND=noninteractive

LABEL org.opencontainers.image.title="RM26 Custom Client" \
      org.opencontainers.image.version="1.0.0" \
      org.opencontainers.image.source="https://github.com/ClearWei/RM26_Client"

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    ca-certificates \
    ccache \
    cmake \
    fontconfig \
    git \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-pulseaudio \
    fonts-noto-color-emoji \
    fonts-noto-cjk \
    fonts-wqy-zenhei \
    libabsl-dev \
    libasound2t64 \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libdbus-1-3 \
    libfontconfig1 \
    libglib2.0-0 \
    libpaho-mqtt-dev \
    libpulse0 \
    libprotobuf-dev \
    libswscale-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-shm0 \
    libxcb-sync1 \
    libxcb-util1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xkb1 \
    libxrender1 \
    ninja-build \
    pkg-config \
    protobuf-compiler \
    qml6-module-qtqml \
    qml6-module-qtqml-models \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-templates \
    qml6-module-qtquick-window \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-multimedia-dev \
    qt6-serialport-dev \
    qt6-svg-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/rmclient

COPY docker/fonts-local.conf /etc/fonts/local.conf
COPY . .

# 编译缓存和 CMake 构建目录都交给 BuildKit 持久化。源码变化会让 Docker 层失效，
# 但 Ninja 仍可复用构建目录，只重新编译发生变化的单元。声音资源单独同步，避免没有
# C++ 变化时跳过 POST_BUILD 后留下旧文件。
RUN --mount=type=cache,id=rm26-client-ccache,target=/root/.ccache,sharing=locked \
    --mount=type=cache,id=rm26-client-cmake-build,target=/root/rmclient-build,sharing=locked \
    chmod +x /opt/rmclient/docker/client-entrypoint.sh && \
    fc-cache -f && \
    cmake -S /opt/rmclient -B /root/rmclient-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache && \
    CCACHE_DIR=/root/.ccache cmake --build /root/rmclient-build --parallel && \
    cmake -E copy_directory /opt/rmclient/resources/sounds /root/rmclient-build/resources/sounds && \
    rm -rf /opt/rmclient/build && \
    cp -a /root/rmclient-build /opt/rmclient/build

ENTRYPOINT ["/opt/rmclient/docker/client-entrypoint.sh"]
