#include "porting/metal/MTLDepthStencilTexture.h"
#include "porting/metal/MTLGraphicsContext.h"
using namespace ngfx;

void MTLDepthStencilTexture::create(MTLGraphicsContext* ctx, uint32_t w, uint32_t h, ::MTLPixelFormat fmt) {
    this->format = fmt;
    auto device = ctx->mtlDevice.v;
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = w;
    desc.height = h;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    desc.pixelFormat = fmt;
    v = [device newTextureWithDescriptor:desc];
}

