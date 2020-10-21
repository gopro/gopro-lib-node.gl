#!/bin/bash
#set -x
python3 -m venv nodegl-env

install -C -d nodegl-env/share
install -C -d nodegl-env/share/nodegl
install -C -d nodegl-env/lib/pkgconfig

install -C -m 644 cmake-build-debug/external/sxplayer-9.5.1/libsxplayer.so nodegl-env/lib/
CWD=$PWD && cd external/sxplayer-9.5.1 && \
sed -e "s#PREFIX#$CWD/nodegl-env#;s#DEP_LIBS##;s#DEP_PRIVATE_LIBS#-lavformat -lavfilter -lavcodec -lavutil  -lm -pthread#" \
    libsxplayer.pc.tpl > libsxplayer.pc
cd -
install -C -m 644 external/sxplayer-9.5.1/libsxplayer.pc nodegl-env/lib/pkgconfig

install -C -m 644 libnodegl/nodegl.h nodegl-env/include/
install -C -m 644 libnodegl/nodes.specs nodegl-env/share/nodegl/
install -C -m 644 cmake-build-debug/libnodegl/libnodegl.so nodegl-env/lib/.

CWD=$PWD cd libnodegl && \
sed -e "s#PREFIX#$CWD/nodegl-env#" \
    -e "s#DEP_LIBS##" \
    -e "s#DEP_PRIVATE_LIBS# -lm -lpthread  -L$PWD/nodegl-env/lib -L/usr/local/lib -L$VULKAN_SDK/x86_64/$VULKAN_SDK/x86_64/lib -lsxplayer -lshaderc_shared -lwayland-egl -lwayland-client -lX11 -lGL -lEGL -lvulkan#" \
    -e "s#VERSION#0.0.0#" \
    libnodegl.pc.tpl > libnodegl.pc; \
cd -
install -C -m 644 libnodegl/libnodegl.pc nodegl-env/lib/pkgconfig

source nodegl-env/bin/activate && python3 -m pip install -r pynodegl/requirements.txt; deactivate
source nodegl-env/bin/activate && PKG_CONFIG_PATH=$PWD/nodegl-env/lib/pkgconfig LDFLAGS=-Wl,-rpath,$PWD/nodegl-env/lib python3 -m pip -v install -e pynodegl; deactivate
source nodegl-env/bin/activate && python3 -m pip install -r pynodegl-utils/requirements.txt; deactivate
source nodegl-env/bin/activate && PKG_CONFIG_PATH=$PWD/nodegl-env/lib/pkgconfig LDFLAGS=-Wl,-rpath,$PWD/nodegl-env/lib python3 -m pip -v install -e pynodegl-utils; deactivate
