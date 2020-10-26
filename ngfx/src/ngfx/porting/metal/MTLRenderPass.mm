#include "porting/metal/MTLRenderPass.h"
#include "porting/metal/MTLGraphicsContext.h"
using namespace ngfx;
using namespace std;

MTLRenderPassDescriptor* MTLRenderPass::getDescriptor(MTLGraphicsContext* mtlCtx, MTLFramebuffer* mtlFramebuffer,
       glm::vec4 clearColor, float clearDepth, uint32_t clearStencil) {
    MTLRenderPassDescriptor* mtlRenderPassDescriptor;
    vector<MTLRenderPassColorAttachmentDescriptor*> colorAttachments;
    if (mtlFramebuffer->colorAttachments.empty()) {
        auto view = mtlCtx->mtkView;
        mtlRenderPassDescriptor = view.currentRenderPassDescriptor;
        colorAttachments.push_back(mtlRenderPassDescriptor.colorAttachments[0]);
    } else {
        mtlRenderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        for (uint32_t j = 0; j<mtlFramebuffer->colorAttachments.size(); j++) {
            auto colorAttachment = mtlRenderPassDescriptor.colorAttachments[j];
            auto& fbColorAttachment = mtlFramebuffer->colorAttachments[j];
            colorAttachment.texture = fbColorAttachment.texture;
            colorAttachment.resolveTexture = fbColorAttachment.resolveTexture;
            colorAttachment.slice = fbColorAttachment.slice;
            colorAttachment.level = fbColorAttachment.level;
            colorAttachment.resolveSlice = fbColorAttachment.resolveSlice;
            colorAttachment.resolveLevel = fbColorAttachment.resolveLevel;
            colorAttachments.push_back(colorAttachment);
        }
    }
    for (auto& colorAttachment : colorAttachments) {
        colorAttachment.clearColor = { clearColor[0], clearColor[1], clearColor[2], clearColor[3] };
        colorAttachment.loadAction = MTLLoadActionClear;
        if (colorAttachment.resolveTexture)
            colorAttachment.storeAction = MTLStoreActionStoreAndMultisampleResolve;
        else colorAttachment.storeAction = MTLStoreActionStore;
    }
    auto depthAttachment = mtlRenderPassDescriptor.depthAttachment;
    if (mtlFramebuffer->depthAttachment) {
        depthAttachment.clearDepth = clearDepth;
        depthAttachment.loadAction = MTLLoadActionClear;
        depthAttachment.resolveTexture = mtlFramebuffer->depthAttachment.resolveTexture;
        depthAttachment.texture = mtlFramebuffer->depthAttachment.texture;
        if (depthAttachment.resolveTexture) depthAttachment.storeAction = MTLStoreActionMultisampleResolve;
        else depthAttachment.storeAction = MTLStoreActionDontCare;
    }
    auto stencilAttachment = mtlRenderPassDescriptor.stencilAttachment;
    if (mtlFramebuffer->stencilAttachment) {
        stencilAttachment.texture = mtlFramebuffer->stencilAttachment.texture;
    }
    if (mtlRenderPassDescriptor.stencilAttachment) mtlRenderPassDescriptor.stencilAttachment.clearStencil = clearStencil;
    return mtlRenderPassDescriptor;
}
