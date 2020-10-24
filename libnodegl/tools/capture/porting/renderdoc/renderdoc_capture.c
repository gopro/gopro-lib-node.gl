#include "capture.h"
#include "log.h"
#ifdef _WIN32
#include "renderdoc_app.h"
#include <Windows.h>
#else
#include "renderdoc.h"
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
static RENDERDOC_API_1_4_1 *rdoc_api = NULL;

void init_capture() {
#ifdef _WIN32
    // At init, on windows
    HMODULE mod = LoadLibraryA("renderdoc.dll");
    assert(mod);
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1, (void **)&rdoc_api);
    assert(ret == 1);
#else
    void *mod = dlopen("librenderdoc.so", RTLD_LAZY); //RTLD_NOW | RTLD_NOLOAD);
    if (!mod) fprintf(stderr, "could not load librenderdoc.so: %s", dlerror());
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1, (void **)&rdoc_api);
    assert(ret == 1);
#endif
    fprintf(stderr, "renderdoc capture path: %s\n", rdoc_api->GetCaptureFilePathTemplate());
}

void begin_capture() {
    rdoc_api->StartFrameCapture(NULL, NULL);
}

void end_capture() {
    rdoc_api->EndFrameCapture(NULL, NULL);
}
