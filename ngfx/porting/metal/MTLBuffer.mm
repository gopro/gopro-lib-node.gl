#include "porting/metal/MTLBuffer.h"
#include "porting/metal/MTLGraphicsContext.h"
#include "DebugUtil.h"
using namespace ngfx;

void MTLBuffer::create(MTLGraphicsContext* ctx, const void* data, uint32_t size, MTLResourceOptions resourceOptions) {
    if (!data) v = [ctx->mtlDevice.v newBufferWithLength:size options:resourceOptions];
    else v = [ctx->mtlDevice.v newBufferWithBytes:data length:size options:resourceOptions];
}

Buffer* Buffer::create(GraphicsContext* ctx, const void* data, uint32_t size, BufferUsageFlags usageFlags) {
    MTLBuffer* buffer = new MTLBuffer();
    buffer->create(mtl(ctx), data, size, MTLResourceStorageModeShared);
    return buffer;
}

void* MTLBuffer::map() { return v.contents; }
void MTLBuffer::unmap() {}
void MTLBuffer::upload(const void* data, uint32_t size, uint32_t offset) {
    memcpy((uint8_t*)(v.contents) + offset, data, size);
}
void MTLBuffer::download(void* data, uint32_t size, uint32_t offset) {
    memcpy(data, (uint8_t*)(v.contents) + offset, size);
}
