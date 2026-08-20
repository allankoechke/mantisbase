FROM debian:stable-slim

ARG MB_VERSION=0.4.0
ARG TARGETARCH
# Optional: pin the release archive checksum so a tampered or substituted
# artifact cannot be silently baked into the image.
#   docker build --build-arg MB_SHA256=<sha256 of the release zip> .
ARG MB_SHA256=""

RUN apt-get update && apt-get install -y --no-install-recommends \
    wget \
    unzip \
    ca-certificates \
    libpq5 \
    libuuid1 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /mb

RUN ARCH=$([ "$TARGETARCH" = "arm64" ] && echo "linux-aarch64" || echo "linux-x86_64") \
    && wget -O /tmp/mantisbase.zip https://github.com/allankoechke/mantisbase/releases/download/v${MB_VERSION}/mantisbase_v${MB_VERSION}_${ARCH}.zip \
    && if [ -n "$MB_SHA256" ]; then \
         echo "${MB_SHA256}  /tmp/mantisbase.zip" | sha256sum -c -; \
       fi \
    && unzip /tmp/mantisbase.zip -d /mb \
    && chmod +x /mb/mantisbase \
    && rm /tmp/mantisbase.zip

RUN groupadd --system mantisbase && useradd --system --gid mantisbase --no-create-home mantisbase \
    && chown -R mantisbase:mantisbase /mb

EXPOSE 8080

USER mantisbase

ENTRYPOINT ["/mb/mantisbase"]
# Unprivileged port: the image no longer runs as root, so it cannot bind 80.
CMD ["serve", "-p", "8080"]
