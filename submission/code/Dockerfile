# --- Stage 1: The Build Environment ---
FROM gcc:13 AS builder

RUN apt-get update && apt-get install -y cmake

WORKDIR /app
COPY . .

# ADDED: -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DENABLE_NATIVE_OPTIMIZATION=OFF \
          -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" .. && \
    make -j$(nproc)

# --- Stage 2: The Runtime Environment ---
FROM debian:bookworm-slim

WORKDIR /app

COPY --from=builder /app/build/out .
COPY --from=builder /app/build/dataset/ ./dataset/

RUN mkdir -p results

ENTRYPOINT ["./out"]
