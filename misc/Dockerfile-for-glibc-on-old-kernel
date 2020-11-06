FROM ubuntu:18.04

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get -y install git-core gnupg flex bison gperf build-essential \
    zip curl zlib1g-dev gawk cmake

WORKDIR /root

COPY glibc-2.23.tar.xz /root

COPY glibc-2.23.patch /root

RUN tar xf glibc-2.23.tar.xz

RUN patch -s -p0 < glibc-2.23.patch

RUN mkdir build

WORKDIR /root/build

RUN ../glibc-2.23/configure --enable-kernel=2.6.32 --enable-static-nss --prefix=/root/build/out

RUN make -j4 && make install

ENV LIBRARY_PATH=/root/build/out/lib
