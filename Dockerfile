FROM ubuntu:22.04 AS base
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ="Asia/Singapore"
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
# General utilities
RUN apt update && apt -y upgrade && apt -y install git nano vim wget curl zip unzip \
    iproute2 cmake make ninja-build build-essential init gdb adb
RUN systemctl mask console-getty.service && \
    systemctl mask system-getty.slice && \
    systemctl mask getty@tty1.service && \
    systemctl mask getty@tty2.service && \
    systemctl mask getty@tty3.service && \
    systemctl mask getty@tty4.service && \
    systemctl mask getty@tty5.service && \
    systemctl mask getty@tty6.service && \
    systemctl mask unattended-upgrades.service

# Add UHD dependencies
FROM base AS uhd
RUN apt install -y libuhd-dev uhd-host
RUN /usr/bin/uhd_images_downloader
# Add srsRAN project
FROM uhd AS srsran5g
RUN apt -y install gcc g++ pkg-config libfftw3-dev libmbedtls-dev libsctp-dev libyaml-cpp-dev libgtest-dev
RUN git clone https://gitlab.com/ocudu/ocudu /root/ocudu
WORKDIR /root/ocudu
RUN git checkout release_26_04 && cmake -B build -G Ninja && ninja -C build

# Add open5gs project
FROM base AS open5gs
RUN wget -qO- https://www.mongodb.org/static/pgp/server-8.0.asc | tee /etc/apt/trusted.gpg.d/server-8.0.asc && \
    echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/8.0 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-8.0.list && \
    apt update && apt install -y mongodb-mongosh
RUN apt -y install python3-pip python3-setuptools python3-wheel flex bison \
    libsctp-dev libgnutls28-dev libgcrypt-dev libssl-dev libmongoc-dev libbson-dev libyaml-dev libnghttp2-dev \
    libmicrohttpd-dev libcurl4-gnutls-dev libnghttp2-dev libtins-dev libtalloc-dev meson && \
    apt install -y --no-install-recommends libidn-dev
RUN git clone https://github.com/open5gs/open5gs /root/open5gs
WORKDIR /root/open5gs
RUN meson build --prefix=`pwd`/install && ninja -C build
RUN curl -fsSL https://deb.nodesource.com/setup_20.x -o nodesource_setup.sh && \
    bash nodesource_setup.sh && \
    apt install nodejs -y && \
    rm nodesource_setup.sh
RUN cd webui && npm install

# Add sni5gect project
FROM uhd AS sni5gect
RUN apt install -y build-essential libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev libzmq3-dev libliquid-dev libyaml-cpp-dev
# # Install cuda
# RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb && \
#     dpkg -i cuda-keyring_1.1-1_all.deb && \
#     apt update && apt -y install cuda-toolkit-12-8 && rm cuda-keyring_1.1-1_all.deb
# ENV PATH=/usr/local/cuda-12.8/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN git clone https://github.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation.git /root/sni5gect
WORKDIR /root/sni5gect
RUN git clone https://gitlab.com/wireshark/wireshark /opt/wireshark/ && \
    cd /opt/wireshark/ && \
    git checkout v4.6.4 && \
    echo yes | ./tools/debian-setup.sh && \
    cmake -B build -G Ninja && ninja -C build
# build
RUN git pull && cmake -B build -G Ninja && ninja -C build
