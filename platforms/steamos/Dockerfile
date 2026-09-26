FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk/arm64:3.0.20250408.124536 AS sdk

USER root
RUN curl -fL https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-aarch64.tar.gz -o /tmp/cmake.tar.gz \
    && mkdir -p /opt \
    && tar -xzf /tmp/cmake.tar.gz -C /opt \
    && rm /tmp/cmake.tar.gz
ENV PATH="/opt/cmake-3.30.5-linux-aarch64/bin:${PATH}"

RUN git clone --depth 1 --branch v2024.3 https://github.com/google/shaderc.git /tmp/shaderc \
    && cd /tmp/shaderc && python3 utils/git-sync-deps \
    && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
        -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON \
        -DSPIRV_SKIP_TESTS=ON \
    && cmake --build build --target glslc_exe --parallel 4 \
    && install -m 755 build/glslc/glslc /usr/local/bin/glslc

FROM sdk AS build
WORKDIR /workspace

COPY CMakeLists.txt LICENSE ./
COPY cmake/ cmake/
COPY xrlib/ xrlib/
COPY xrapp/ xrapp/
COPY demo-01_checkxr/ demo-01_checkxr/
COPY demo-02_displayxr/ demo-02_displayxr/
COPY demo-05_inputxr/ demo-05_inputxr/
COPY platforms/steamos/ platforms/steamos/

ARG DEMO=all
ARG BUILD_ENTRY=root
RUN set -eu; \
    case "$DEMO" in \
        all) demos="demo-01_checkxr demo-02_displayxr demo-05_inputxr" ;; \
        demo-01_checkxr|demo-02_displayxr|demo-05_inputxr) demos="$DEMO" ;; \
        *) echo "Unsupported demo: $DEMO" >&2; exit 1 ;; \
    esac; \
    for demo in $demos; do \
        case "$BUILD_ENTRY" in \
            root) source=.; set -- "-DSTEAMOS_PROJECT=$demo" ;; \
            standalone) source="$demo"; set -- ;; \
            *) echo "Unsupported build entry: $BUILD_ENTRY" >&2; exit 1 ;; \
        esac; \
        build="$demo/build/steamos-arm64"; \
        cmake -S "$source" -B "$build" -DSTEAMOS_ARM64=ON \
            "$@" -DCMAKE_BUILD_TYPE=Release; \
        cmake --build "$build" --parallel 4; \
        cmake --install "$build" --component SteamOS --prefix "/deploy/$build/deploy"; \
    done

FROM scratch AS packages
COPY --from=build /deploy/ /
