FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
    # Podstawowe narzędzia
    ca-certificates \
    curl \
    git \
    wget \
    xz-utils \
    make \
    pkg-config \
    xxd \
    ssh \
    # Python i narzędzia (z pakietami Pythona z apt)
    python3 \
    python3-pip \
    python3-dev \
    python3-venv \
    python3-click \
    python3-yaml \
    python3-requests \
    # Kompilator g++ i clang
    g++-14 \
    clang-19 \
    clang++-19 \
    clang-format-19 \
    clang-tidy-19 \
    # Narzędzia budowania
    cmake \
    ninja-build \
    # Zależności LLVM
    llvm-19 \
    llvm-19-dev \
    llvm-19-runtime \
    # Zależności do generowania dokumentacji i diagramów
    doxygen \
    graphviz \
    libgraphviz-dev \
    # Narzędzia do raportów pokrycia kodu
    lcov \
    # Zależności biblioteczne
    libzstd-dev \
    zlib1g-dev \
    && \
    # Ustaw g++-14 jako domyślny kompilator
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    # Wyczyść cache apt, aby zmniejszyć rozmiar obrazu
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

SHELL ["/bin/bash", "-c"]
WORKDIR /duckling

# Zmienne środowiskowe dla LLVM
ENV LLVM_DIR=/usr/lib/llvm-19
ENV PATH=/usr/lib/llvm-19/bin:$PATH

# Domyślna wartość; nadpisana w `run.sh`
USER 1000:1000

# Domyślna komenda
CMD ["/bin/bash"]

