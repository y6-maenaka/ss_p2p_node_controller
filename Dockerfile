# SS P2P Node Controller Development Environment
# Ubuntu-based container with C++20, CMake, and all required dependencies

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Tokyo

# Install system dependencies
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    make \
    ninja-build \
    pkg-config \
    # C++20 compiler
    g++-11 \
    gcc-11 \
    # Development tools
    git \
    curl \
    wget \
    vim \
    nano \
    gdb \
    valgrind \
    # Required libraries
    libssl-dev \
    libboost-all-dev \
    # Testing framework
    libgtest-dev \
    # Networking utilities
    net-tools \
    iputils-ping \
    netcat \
    # Cleanup
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set default compiler to g++-11 for C++20 support
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

# Install Google Test
RUN cd /usr/src/gtest \
    && cmake CMakeLists.txt \
    && make \
    && cp lib/*.a /usr/lib/ \
    && mkdir -p /usr/local/lib \
    && cp lib/*.a /usr/local/lib/

# Create working directory
WORKDIR /workspace/ss_p2p_node_controller

# Copy source code
COPY . .

# Create necessary directories
RUN mkdir -p build log

# Set environment variables
ENV CMAKE_BUILD_TYPE=Debug
ENV SS_P2P_LOG_DIR=/workspace/ss_p2p_node_controller/log

# Configure CMake project
RUN cd build && \
    cmake -DCMAKE_BUILD_TYPE=Debug \
          -DBUILD_TESTS=ON \
          -DBUILD_EXAMPLES=ON \
          -DCMAKE_CXX_COMPILER=g++-11 \
          ..

# Build the library
RUN cd build && cmake --build . -j$(nproc)

# Expose commonly used ports for P2P communication
EXPOSE 8080 8081 8082 8083 8084

# Default command
CMD ["/bin/bash"]