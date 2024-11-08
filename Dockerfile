# 使用Ubuntu作為基礎映像
FROM ubuntu:20.04

# 設置非交互模式，避免手動選擇地理區域等問題
ENV DEBIAN_FRONTEND=noninteractive

# 更新包管理器並安裝C/C++編譯器以及您指定的工具和庫
RUN apt-get update && apt-get -y upgrade && \
    apt-get install -y \
    git \
    net-tools \
    vim \
    sudo \
    tcsh \
    gcc \
    g++ \
    unzip \
    python3 \
    python3-pip \
    curl \
    htop \
    libpthread-stubs0-dev \
    libomp-dev \
    ocl-icd-libopencl1 \
    opencl-headers \
    clinfo \
    build-essential \
    cmake \
    tzdata \  
    && rm -rf /var/lib/apt/lists/*

# 安裝OpenMP庫
RUN apt-get install -y libomp-dev

# 安裝OpenCL庫
RUN apt-get install -y ocl-icd-libopencl1 opencl-headers clinfo

# 設置工作目錄
WORKDIR /workspace

# 讓容器保持開啟的方式
CMD ["tail", "-f", "/dev/null"]
