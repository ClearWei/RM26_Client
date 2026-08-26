FROM python:3.11-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends ffmpeg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/sim

COPY sim /workspace/sim

RUN python -m pip install --no-cache-dir -e /workspace/sim
