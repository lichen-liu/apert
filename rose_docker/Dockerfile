# Environment for ROSE compiler development.
# Modified from:
# https://github.com/ouankou/rose-tools/blob/main/Dockerfile.1804-release-noinstall
# https://github.com/ouankou/rose-tools/blob/main/Dockerfile.1804-release

# Pull base image.
FROM ubuntu:18.04

# Adding user rose
RUN groupadd -g 9999 rose && \
    useradd -r -u 9999 -g rose -m -d /home/rose rose

# Install packages.
RUN apt-get update && \
    apt-get install -y \
        apt-utils \
        dialog \
        software-properties-common && \
    apt-get install -y openjdk-8-jdk && \
    apt-get install -y \
        autoconf \
        automake \
        autotools-dev \
        bc \
        binutils \
        bison \
        build-essential \
        cmake \
        curl \
        flex \
        gcc-multilib \
        gdb \
        gfortran \
        ghostscript \
        git \
        graphviz \
        iputils-ping \
        libboost-all-dev \
        libomp-dev \
        libtool \
        vim \
        wget && \
    rm -rf /var/lib/apt/lists/* && \
    rm -rf /var/cache/*

USER rose

# Setup environment
ENV ROSE_SRC /home/rose/rose_src
ENV ROSE_PATH /home/rose/rose_install
ENV ROSE_BUILD /home/rose/rose_build
ENV PATH $ROSE_PATH/bin:$PATH
ENV LD_LIBRARY_PATH $ROSE_PATH/lib:$LD_LIBRARY_PATH
ENV LIBRARY_PATH $ROSE_PATH/lib:$LIBRARY_PATH

# Define commonly used JAVA_HOME variable
ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64
ENV LD_LIBRARY_PATH $JAVA_HOME/jre/lib/amd64/server:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

RUN mkdir -p $ROSE_PATH && \
    mkdir -p $ROSE_BUILD && \
    git clone https://github.com/rose-compiler/rose.git $ROSE_SRC && \
    cd $ROSE_SRC && \
    ./build && \
    cd $ROSE_BUILD && \
    $ROSE_SRC/configure --prefix=$ROSE_PATH --with-boost=/usr --with-boost-libdir=/usr/lib/x86_64-linux-gnu/ --enable-languages=c,c++,fortran --disable-projects-directory --disable-tests-directory --disable-tutorial-directory --with-gomp_omp_runtime_library=/usr/lib/gcc/x86_64-linux-gnu/7
# Compile ROSE
RUN cd $ROSE_BUILD && \
    make core -j2
# Compile ROSE
RUN cd $ROSE_BUILD && \
    make install-core && \
    make install-tools

# If the files on the host need to be modified, the host user has to be passed into Docker for correct file permissions.

# Define working directory.
WORKDIR /home/rose

# Define default command.
CMD ["bash"]