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
    ssh \
    gh \
    less \
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
    clangd-19 \
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
    # Additional
    jq \
    build-essential \
    libssl-dev \
    libffi-dev \
    libicu74 \
    valgrind \
    libsqlite3-dev \
    libgfortran5 \
    software-properties-common \
    rsync \
    nlohmann-json3-dev \
    nano \
    gdb \
    && \
    # Make g++-14 default
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# install rust
ENV RUSTUP_HOME=/var/rustup
ENV RUSTUP_TOOLCHAIN=stable
ENV RUSTUP_AUTO_INSTALL=1
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
RUN chmod o+rx /root
RUN chmod -R o+r /root/.cargo

# install Claude Code
ENV CLAUDE_CODE_DIR=/opt/claude-code
RUN mkdir -p ${CLAUDE_CODE_DIR} && \
    HOME=${CLAUDE_CODE_DIR} bash -c 'curl -fsSL https://claude.ai/install.sh | bash' && \
    ln -s ${CLAUDE_CODE_DIR}/.local/bin/claude /usr/local/bin/claude && \
    chmod -R a+rX ${CLAUDE_CODE_DIR}

RUN echo 'source /usr/share/bash-completion/bash_completion' >> /etc/bash.bashrc
COPY bash_completion/* /etc/bash_completion.d/
COPY bin/* /bin/

# RUN echo 'PS1="\[\e[1;32m\]\u@\h\[\e[0m\]:\[\e[1;34m\]\w\[\e[0m\]\$ "' >> /etc/bash.bashrc
# RUN echo 'PS1="\[\e[1;92m\]\$(git branch --show-current 2>/dev/null)\[\e[0m\] \[\e[1;34m\]\w\[\e[0m\] \$ "' >> /etc/bash.bashrc
RUN echo 'PS1="\[\e[1;92m\]\$(sed '\''s/ref: refs\/heads\///'\'' /duckling/.git/HEAD 2>/dev/null)\[\e[0m\] \[\e[1;34m\]\w\[\e[0m\] \$ "' >> /etc/bash.bashrc
RUN echo 'alias grep="grep --color=auto"' >> /etc/bash.bashrc
RUN echo 'alias ls="ls --color=auto"' >> /etc/bash.bashrc

SHELL ["/bin/bash", "-c"]
WORKDIR /duckling

ENV LLVM_DIR=/usr/lib/llvm-19
ENV PATH=/home/ubuntu/.local/bin:/root/.cargo/bin:/usr/lib/llvm-19/bin:$PATH
ENV HOME=/home/ubuntu

ARG UID=1000
ARG GID
ARG DEFAULT_USER=ubuntu

# `groupmod` can fail with exit code 4 when the provided GID is already in use in the container.
# Our only concern is that the GID of created files match the GID of the user outside the container,
# so we can use the already existing group with this GID instead of ubuntu group.
# This is the case for macOS users whose GID is usually 20 (`staff`).
RUN echo gid ${GID} uid ${UID} expr ${GID:-${UID}}       && \
    ( groupmod -g ${GID:-${UID}} ubuntu || [ $? == 4 ] ) && \
    usermod -u ${UID} -g ${GID:-${UID}} ubuntu

RUN if [ "$DEFAULT_USER" = "root" ]; then \
        rm -rf /root/.ssh && ln -s /home/ubuntu/.ssh /root/.ssh; \
    fi

USER ${DEFAULT_USER}

CMD ["/bin/bash"]
