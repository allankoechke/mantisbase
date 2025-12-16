FROM ubuntu:latest

ARG MANTIS_VERSION=0.3.0
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    unzip \
    wget \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /mb

RUN wget -O /tmp/mantisbase.zip https://github.com/allankoechke/mantisbase/releases/download/v${MANTIS_VERSION}/mantisbase_v${MANTIS_VERSION}_linux.zip \
  && unzip /tmp/mantisbase.zip -d /mb \
  && chmod +x /mb/mantisbase \
  && rm /tmp/mantisbase.zip

EXPOSE 80

CMD ["/mb/mantisbase", "--dev", "serve", "-p", "80"]
