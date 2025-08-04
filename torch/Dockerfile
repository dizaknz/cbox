FROM ubuntu:20.04 as base
USER root
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    binutils \
    apt-utils \
    libtorch-dev \
    g++ \
    cmake \
    python3 \
    python3-pip \
    make
RUN adduser --system --group --home /opt/projects dev
USER dev
ENV PATH=/opt/projects/.local/bin:$PATH
RUN pip install conan && conan config init
WORKDIR /opt/projects
COPY --chown=dev:dev . torch/
WORKDIR /opt/projects/torch
#RUN cmake -DCMAKE_BUILD_TYPE=Release .
#RUN cmake --build .
