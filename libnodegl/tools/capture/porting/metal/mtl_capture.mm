#include "capture.h"
#include "FileUtil.h"
#include <Metal/Metal.h>
#include <string>
using namespace std;

struct mtl_capture_t {
    void init() {}
    void begin_capture() {
        NSError *error;
        MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
        MTLCaptureDescriptor *captureDescriptor = [[MTLCaptureDescriptor alloc] init];
        extern id<MTLDevice> _mtlDevice;
        captureDescriptor.captureObject = _mtlDevice;
        captureDescriptor.destination = MTLCaptureDestinationDeveloperTools;
        if (![captureManager startCaptureWithDescriptor: captureDescriptor error:&error]) {
            NSLog(@"Failed to start capture, error %@", error);
        }
    }
    void end_capture() {
        MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
        [captureManager stopCapture];
    }
};
static mtl_capture_t mtl_capture;

void init_capture() { mtl_capture.init(); }
void begin_capture() { mtl_capture.begin_capture(); }
void end_capture() { mtl_capture.end_capture(); }

