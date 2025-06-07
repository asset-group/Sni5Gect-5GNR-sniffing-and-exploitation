FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ="Asia/Singapore"
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
# Install dependencies
RUN apt update && apt -y upgrade && apt -y install git nano vim wget curl iproute2 \
    cmake make ninja-build build-essential
# Install UHD and download images
RUN apt install -y libuhd-dev uhd-host && /usr/bin/uhd_images_downloader
# Install dependencies for 5G NR attacks
RUN apt -y install sudo init python3-pip python3-dev software-properties-common kmod bc gzip zstd flex bison \
        pkg-config swig graphviz libglib2.0-dev libgcrypt20-dev libnl-genl-3-200 libgraphviz-dev liblz4-dev \
        libsnappy-dev libgnutls28-dev libxml2-dev libnghttp2-dev libkrb5-dev libnl-3-dev libspandsp-dev \
        libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libnl-genl-3-dev libnl-route-3-dev \
        libnl-nf-3-dev libcap-dev libbrotli-dev libsmi2-dev liblua5.2-dev libc-ares-dev libsbc-dev \
        libspeexdsp-dev libfreetype6-dev libxss1 libtbb-dev libnss3-dev libudev-dev libpulse-dev \
        libpcre2-dev libasound2-dev libgl1-mesa-dev libssh-dev libmaxminddb-dev libopus-dev \
        libusb-1.0-0 psmisc sshpass tcpdump libfmt-dev libsodium-dev xxd libc-bin libmbim-glib-dev \
        libgoogle-glog-dev libzstd-dev libevent-dev libunwind-dev libdouble-conversion-dev libgflags-dev \
        qtbase5-dev libqt5multimedia5 libqt5svg5 qttools5-dev qtmultimedia5-dev g++ software-properties-common \
        kmod libglib2.0-dev libsnappy1v5 libsmi2ldbl liblua5.2-0 libc-ares2 libnl-route-3-200 libnl-genl-3-200 \
        libfreetype6 graphviz libtbb12 libxss1 libnss3 libspandsp2 libsbc1 libbrotli1 libnghttp2-14 libasound2 \
        psmisc sshpass libpulse0 libasound2 libpcre2-dev libmaxminddb0 libopus0 libspeex1 bc tcpdump libgflags2.2 \
        libzstd1 libunwind8 libcap2 libspeexdsp1 libxtst6 libatk-bridge2.0-0 libusb-1.0-0 meson
# Install llvm 15
RUN wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && ./llvm.sh 15 && rm llvm.sh
RUN wget https://security.debian.org/debian-security/pool/updates/main/o/openssl/libssl1.1_1.1.1n-0+deb10u6_amd64.deb && \
        dpkg -i libssl1.1_1.1.1n-0+deb10u6_amd64.deb && \
        rm libssl1.1_1.1.1n-0+deb10u6_amd64.deb
# Clone and build wdissector
RUN git clone https://github.com/asset-group/5ghoul-5g-nr-attacks /root/wdissector
WORKDIR /root/wdissector
RUN ./scripts/apply_patches.sh 3rd-party
RUN cd 3rd-party/ && git clone https://github.com/zeromq/libzmq.git --depth=1 && cd libzmq && \
    ./autogen.sh && ./configure && make -j && make install && ldconfig
RUN cd 3rd-party/ && git clone https://github.com/zeromq/czmq.git --depth=1 && cd czmq && \
        ./autogen.sh && ./configure && make -j && make install && ldconfig
RUN cd 3rd-party/ && git clone https://github.com/json-c/json-c.git --depth=1 && cd json-c && \
    mkdir -p build && cd build && cmake ../ && make -j && make install && ldconfig
RUN curl https://raw.githubusercontent.com/jckarter/tbb/refs/heads/master/include/tbb/tbb_stddef.h -o /usr/include/tbb/tbb_stddef.h
RUN sed -i 's/\blong[[:space:]]\+gettid()/__pid_t gettid()/g' /root/wdissector/src/MiscUtils.hpp && \
    sed -i 's/\bif\s*(\s*SWIG_EXIST\s*)/if(NOT SWIG_EXIST)/g' /root/wdissector/CMakeLists.txt
RUN cd /root/wdissector && ./build.sh all
# Prevent docker container take over tty
RUN systemctl mask console-getty.service && \
    systemctl mask system-getty.slice && \
    systemctl mask getty@tty1.service && \
    systemctl mask getty@tty2.service && \
    systemctl mask getty@tty3.service && \
    systemctl mask getty@tty4.service && \
    systemctl mask getty@tty5.service && \
    systemctl mask getty@tty6.service && \
    systemctl mask unattended-upgrades.service
# Build sni5gect
RUN apt install -y build-essential libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev libzmq3-dev libliquid-dev unzip openssh-client
ARG GITHUB_TOKEN
RUN git clone https://$GITHUB_TOKEN@github.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation.git /root/sni5gect
WORKDIR /root/sni5gect
RUN cmake -B build -G Ninja && ninja -C build
# Build open5gs
RUN apt -y install gnupg curl wget iproute2 python3-pip python3-setuptools python3-wheel flex bison \
    libsctp-dev libgnutls28-dev libgcrypt-dev libssl-dev libmongoc-dev libbson-dev libyaml-dev libnghttp2-dev \
    libmicrohttpd-dev libcurl4-gnutls-dev libnghttp2-dev libtins-dev libtalloc-dev meson && \
    apt install -y --no-install-recommends libidn-dev
RUN wget -qO- https://www.mongodb.org/static/pgp/server-8.0.asc | tee /etc/apt/trusted.gpg.d/server-8.0.asc && \
    echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/8.0 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-8.0.list && \
    apt update && apt install -y mongodb-mongosh
RUN git clone https://github.com/open5gs/open5gs /root/open5gs && cd /root/open5gs && meson build --prefix=`pwd`/install && ninja -C build && \
    cp build/configs/sample.yaml /root/open5gs/open5gs.yaml && \
    sed -i 's/mcc: 999/mcc: 001/g' /root/open5gs/open5gs.yaml && \
    sed -i 's/mnc: 70/mnc: 01/g' /root/open5gs/open5gs.yaml && \
    sed -i 's|logger:|logger:\n  file:\n    path: /root/sni5gect/logs/open5gs.log|g' /root/open5gs/open5gs.yaml

# Build srsran
RUN apt -y install gcc g++ pkg-config libfftw3-dev libmbedtls-dev libsctp-dev libyaml-cpp-dev libgtest-dev
RUN git clone https://github.com/srsRAN/srsRAN_Project.git /root/srsran && cd /root/srsran && \
    git checkout release_24_10_1 && cmake -B build -G Ninja && ninja -C build
RUN cp /root/srsran/configs/gnb_rf_b200_tdd_n78_20mhz.yml /root/srsran/srsran.conf && \
    sed -i 's/addr: 127\.0\.1\.100/addr: 127.0.0.5/' /root/srsran/srsran.conf && \
    sed -i 's/tac: 7/tac: 1/g' /root/srsran/srsran.conf && \
    sed -i 's/dl_arfcn: 632628/dl_arfcn: 628500/' /root/srsran/srsran.conf && \
    sed -i 's|filename: /tmp/gnb.log|filename: /root/sni5gect/logs/gnb.log|' /root/srsran/srsran.conf && \
    sed -i 's/all_level: warning/all_level: debug/' /root/srsran/srsran.conf && \
    sed -i 's/mac_enable: false/mac_enable: true/' /root/srsran/srsran.conf && \
    sed -i 's|mac_filename: /tmp/gnb_mac.pcap|mac_filename: /root/sni5gect/logs/gnb_mac.pcap|' /root/srsran/srsran.conf

# Configurations required for artifacts
RUN apt install -y adb openssh-server wireshark tshark && \
    sed -i 's/^#Port 22/Port 65330/' /etc/ssh/sshd_config && \
    systemctl enable ssh && mkdir -p /root/.ssh && \
    chmod 700 /root/.ssh
COPY credentials/authorized_keys /root/.ssh/authorized_keys
RUN chmod 0600 /root/.ssh/authorized_keys
COPY utils/dlt_user_config /root/.config/wireshark
RUN wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh && \
    bash Miniconda3-latest-Linux-x86_64.sh -p /root/.miniconda -b -u && \
    /root/.miniconda/bin/conda init bash && \
    rm Miniconda3-latest-Linux-x86_64.sh && \
    . "/root/.miniconda/etc/profile.d/conda.sh" && conda activate base && \
    pip install pandas libtmux loguru seaborn jupyter ipython ipykernel pyserial pyusb crcmod pycrate && \
    git clone https://github.com/P1sec/QCSuper /root/qcsuper && \
    cd /root/qcsuper && pip3 install --upgrade https://github.com/P1sec/pycrate/archive/master.zip && \
    apt install -y cutecom
# Use X11VNC for remote access
RUN apt install -y xvfb x11vnc fluxbox init
COPY utils/xvfb.service /etc/systemd/system/xvfb.service
COPY utils/x11vnc.service /etc/systemd/system/x11vnc.service
COPY utils/fluxbox.service /etc/systemd/system/fluxbox.service
RUN systemctl enable xvfb.service && \
    systemctl enable fluxbox.service && \
    systemctl enable x11vnc.service
RUN git pull && mkdir -p /root/sni5gect/logs