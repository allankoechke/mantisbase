# Build from repository root: docker build -f docker/Dockerfile .
FROM rust:1.85-bookworm AS builder
WORKDIR /src
RUN apt-get update && apt-get install -y --no-install-recommends \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*
COPY Cargo.toml Cargo.lock ./
COPY migrations ./migrations
COPY src ./src
COPY public ./public
RUN cargo build --release

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/target/release/mantisbase /usr/local/bin/mantisbase
COPY public /opt/mantisbase/public
EXPOSE 7070
ENTRYPOINT ["/usr/local/bin/mantisbase"]
CMD ["serve", "--host", "0.0.0.0", "-p", "7070", "--public-dir", "/opt/mantisbase/public", "--data-dir", "/data"]
