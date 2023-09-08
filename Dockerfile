# Stage 1: Build the application
FROM ubuntu:20.04 as build

# Set environment variables to non-interactive (this prevents some prompts)
ENV DEBIAN_FRONTEND=noninteractive

# Install g++, make, boost libraries, and nlohmann-json
RUN apt-get update -q && \
    apt-get install -yq g++ make libboost-all-dev nlohmann-json3-dev

WORKDIR /build
COPY . .

# Compile the C++ code
RUN g++ server.cpp -lboost_system -pthread -o server

# Stage 2: Create the final image
FROM ubuntu:20.04

WORKDIR /app
COPY --from=build /build/server /app/

# Run the application
CMD ["./server"]