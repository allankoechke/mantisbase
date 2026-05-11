# Build from repository root: docker build -f docker/Dockerfile .
FROM node:22-bookworm AS admin
WORKDIR /src/admin
COPY admin/package.json ./
RUN npm install
COPY admin/ ./
RUN npm run build
# vite outDir → /src/public/mb-dist

FROM rust:1.85-bookworm AS rust
WORKDIR /src
RUN apt-get update && apt-get install -y --no-install-recommends \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*
COPY Cargo.toml Cargo.lock ./
COPY migrations ./migrations
COPY src ./src
RUN cargo build --release

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=rust /src/target/release/mantisbase /usr/local/bin/mantisbase
COPY --from=admin /src/public/mb-dist /opt/mantisbase/mb-dist
EXPOSE 7070
ENTRYPOINT ["/usr/local/bin/mantisbase"]
CMD ["serve", "--host", "0.0.0.0", "-p", "7070", "--admin-ui-dir", "/opt/mantisbase/mb-dist", "--data-dir", "/data"]
