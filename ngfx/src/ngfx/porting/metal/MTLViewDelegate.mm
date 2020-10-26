#include "porting/metal/MTLViewDelegate.h"
#include <functional>
std::function<void(void*)> appInit, appPaint, appUpdate;


@implementation MTLViewDelegate {}
- (nonnull instancetype)create:(nonnull MTKView *)mtkView {
    self = [ super init];
    appInit(mtkView);
    return self;
}

- (void)drawInMTKView:(nonnull MTKView *)mtkView {
    appUpdate(mtkView);
    appPaint(mtkView);
}
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    
}
@end
