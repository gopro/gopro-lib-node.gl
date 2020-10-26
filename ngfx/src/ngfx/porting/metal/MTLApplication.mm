#include "MTLApplication.h"
#include "DebugUtil.h"
#include "porting/metal/MTLGraphicsContext.h"
#include "porting/metal/MTLCommandBuffer.h"
#include "porting/metal/MTLSurface.h"
#include <functional>
using namespace ngfx;
using namespace std::placeholders;

extern std::function<void(void*)> appInit, appPaint, appUpdate;

MTLApplication::MTLApplication(const std::string& appName,
        int width, int height, bool enableDepthStencil, bool offscreen)
    : BaseApplication(appName, width, height, enableDepthStencil, offscreen) {}

void MTLApplication::init() {
    if (offscreen) BaseApplication::init();
    else {
        appInit = [&](void* view) {
            graphicsContext.reset(GraphicsContext::create(appName.c_str(), enableDepthStencil, true));
            MTLSurface surface;
            auto& mtkView = surface.mtkView;
            mtkView = (MTKView*)view;
            if (w != -1 && h != -1) {
                [mtkView setFrame: NSMakeRect(0, 0, w, h)];
            }
            CGSize mtkViewSize = [mtkView drawableSize];
            surface.w = mtkViewSize.width;
            surface.h = mtkViewSize.height;
            graphicsContext->setSurface(&surface);
            graphics.reset(Graphics::create(graphicsContext.get()));
            onInit();
        };
        appPaint = [&](void* view) {
            auto ctx = mtl(graphicsContext.get());
            ctx->mtkView = (MTKView*) view;
            paint();
        };
        appUpdate = [&](void*) { onUpdate(); };
    }
}

void MTLApplication::run() {
    init();
    const char* argv[] = {""};
    NSApplicationMain(1, argv);
}


void MTLApplication::paint() {
    auto ctx = mtl(graphicsContext.get());
    MTLCommandBuffer commandBuffer;
    commandBuffer.v = [ctx->mtlCommandQueue commandBuffer];
    onRecordCommandBuffer(&commandBuffer);
    if (!offscreen) {
        [commandBuffer.v presentDrawable:ctx->mtkView.currentDrawable];
    }
    [commandBuffer.v commit];
    if (offscreen) graphics->waitIdle(&commandBuffer);
}
