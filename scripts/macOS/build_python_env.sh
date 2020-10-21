#!/bin/bash
#set -x

GREEN='\033[0;32m'
NC='\033[0m' # No Color

cd cmake-build-debug/pyshell/Debug && ln -fs /Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/ .; cd -

echo -e "${GREEN}Setting up python virtual environment${NC}"
python3 -m venv nodegl-env

install -C -d nodegl-env/share
install -C -d nodegl-env/share/nodegl
install -C -d nodegl-env/lib/pkgconfig

install -C -m 644 cmake-build-debug/external/sxplayer-9.5.1/Debug/libsxplayer.dylib nodegl-env/lib/
CWD=$PWD && cd external/sxplayer-9.5.1 && \
sed -e "s#PREFIX#$CWD/nodegl-env#;s#DEP_LIBS##;s#DEP_PRIVATE_LIBS#-lavformat -lavfilter -lavcodec -lavutil  -lm -pthread#" \
    libsxplayer.pc.tpl > libsxplayer.pc
cd -
install -C -m 644 external/sxplayer-9.5.1/libsxplayer.pc nodegl-env/lib/pkgconfig

install -C -m 644 libnodegl/src/nodegl.h nodegl-env/include/
install -C -m 644 libnodegl/nodes.specs nodegl-env/share/nodegl/
install -C -m 644 cmake-build-debug/libnodegl/Debug/libnodegl.dylib nodegl-env/lib/.

CWD=$PWD cd libnodegl && \
sed -e "s#PREFIX#$CWD/nodegl-env#" \
    -e "s#DEP_LIBS##" \
    -e "s#DEP_PRIVATE_LIBS# -lm -lpthread  -L$PWD/nodegl-env/lib -L/usr/local/lib -L$VULKAN_SDK/x86_64/$VULKAN_SDK/x86_64/lib -lsxplayer -lshaderc_shared -lwayland-egl -lwayland-client -lX11 -lGL -lEGL -lvulkan#" \
    -e "s#VERSION#0.0.0#" \
    libnodegl.pc.tpl > libnodegl.pc; \
cd -
install -C -m 644 libnodegl/libnodegl.pc nodegl-env/lib/pkgconfig

echo -e "${GREEN}Installing pynodegl dependencies${NC}"
source nodegl-env/bin/activate && python3 -m pip install -r pynodegl/requirements.txt; deactivate
echo -e "${GREEN}Building pynodegl${NC}"
source nodegl-env/bin/activate && PKG_CONFIG_PATH=$PWD/nodegl-env/lib/pkgconfig LDFLAGS=-Wl,-rpath,$PWD/nodegl-env/lib python3 -m pip -v install -e pynodegl; deactivate
echo -e "${GREEN}Installing pynodegl-utils dependencies${NC}"
source nodegl-env/bin/activate && python3 -m pip install -r pynodegl-utils/requirements.txt; deactivate
echo -e "${GREEN}Building pynodegl-utils${NC}"
source nodegl-env/bin/activate && PKG_CONFIG_PATH=$PWD/nodegl-env/lib/pkgconfig LDFLAGS=-Wl,-rpath,$PWD/nodegl-env/lib python3 -m pip -v install -e pynodegl-utils; deactivate
