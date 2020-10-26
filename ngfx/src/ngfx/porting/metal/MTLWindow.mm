#include "porting/metal/MTLWindow.h"
#include "DebugUtil.h"
using namespace ngfx;

bool MTLWindow::shouldClose() {
    LOG_TRACE("Not supported");
    return false;
}
void MTLWindow::pollEvents() {
    LOG_TRACE("Not supported");
}

Window* Window::create(GraphicsContext* graphicsContext, const char* title, std::function<void(Window* thiz)> onWindowCreated,
                       int w, int h) {
    LOG_TRACE("Not supported");
    return nullptr;
}
