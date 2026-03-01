FROM ubuntu:22.04

# 安装必要的工具和依赖
RUN apt-get update && apt-get install -y \
    git \
    g++ \
    cmake \
    make \
    build-essential \
    libboost-all-dev \
    protobuf-compiler \
    libprotobuf-dev \
    libevent-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# 重新编译proto文件
RUN protoc -I=/app/protocol --cpp_out=/app/protocol --experimental_allow_proto3_optional /app/protocol/protocol.proto

RUN rm -rf build && mkdir build && cd build && cmake .. && make

EXPOSE 8888

CMD ["/app/build/ncxs /app/ncxs.toml"]