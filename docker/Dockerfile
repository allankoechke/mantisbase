FROM debian:trixie-slim

ARG MB_VERSION=0.3.4

RUN apt-get update && apt-get install -y --no-install-recommends \
    wget \
    unzip \
    ca-certificates \
    libpq5 \
    libzstd1 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /mb

RUN wget -O /tmp/mantisbase.zip https://github.com/allankoechke/mantisbase/releases/download/v${MB_VERSION}/mantisbase_v${MB_VERSION}_linux-x86_64.zip \
    && unzip /tmp/mantisbase.zip -d /mb \
    && chmod +x /mb/mantisbase \
    && rm /tmp/mantisbase.zip

EXPOSE 80

ENTRYPOINT ["/mb/mantisbase"]
CMD ["--dev", "serve", "-p", "80"]
