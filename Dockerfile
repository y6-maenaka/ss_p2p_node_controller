FROM almalinux

RUN yum update -y \
&& yum install -y \
git \
g++ \
cmake \
openssl-devel \
boost-devel \
&& yum clean all

RUN mkdir -p ~/develop/ss_p2p_node_controller/
RUN git clone https://github.com/y6-maenaka/ss_p2p_node_controller.git ~/develop/ss_p2p_node_controller/
RUN mkdir -p ~/develop/ss_p2p_node_controller/build
WORKDIR root/develop/ss_p2p_node_controller/build

RUN cmake -D_BUILD_DEBUG=True ..
RUN cmake --build .


