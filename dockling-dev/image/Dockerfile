FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
    # Basic tools
    ca-certificates \
    bash-completion \
    curl \
    git \
    wget \
    xz-utils \
    make \
    pkg-config \
    xxd \
    ssh \
    gh \
    # Python
    python3 \
    python3-pip \
    python3-dev \
    python3-venv \
    python3-click \
    python3-yaml \
    python3-requests \
    # g++ & clang
    g++-14 \
    clang-19 \
    clang++-19 \
    clang-format-19 \
    clang-tidy-19 \
    # Linker used by gh runners
    mold \
    # Build tools
    cmake \
    ninja-build \
    # LLVM
    llvm-19 \
    llvm-19-dev \
    llvm-19-runtime \
    # Docs deps
    doxygen \
    graphviz \
    libgraphviz-dev \
    # Coverage deps
    lcov \
    # Libraries
    libzstd-dev \
    zlib1g-dev \
    && \
    # Make g++-14 default
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN echo 'source /usr/share/bash-completion/bash_completion' >> /etc/bash.bashrc
COPY toolbox-completion.bash /etc/bash_completion.d/toolbox

SHELL ["/bin/bash", "-c"]
WORKDIR /duckling

ENV LLVM_DIR=/usr/lib/llvm-19
ENV PATH=/duckling/dockling-dev/bin:/usr/lib/llvm-19/bin:$PATH

ARG UID=1000
ARG GID=1000
ARG DEFAULT_USER=ubuntu

RUN groupmod -g ${GID} ubuntu && \
    usermod -u ${UID} -g ${GID} ubuntu

USER ${DEFAULT_USER}

CMD ["/bin/bash"]
