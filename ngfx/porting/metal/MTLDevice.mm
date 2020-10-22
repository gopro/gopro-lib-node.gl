#include "porting/metal/MTLDevice.h"
#include "DebugUtil.h"
using namespace ngfx;

id<MTLDevice> _mtlDevice;

void MTLDevice::create() {
    v = MTLCreateSystemDefaultDevice();
    NSCAssert(v, @"Failed to create metal device");
    _mtlDevice = v;
}
