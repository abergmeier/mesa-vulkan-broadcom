FROM ubuntu

RUN apt-get update && \
    apt-get install -y python3 python3-pip python3-setuptools python3-wheel ninja-build \
                       g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf pkg-config && \
    apt-get autoclean && \
    apt-get clean

RUN pip3 install meson
