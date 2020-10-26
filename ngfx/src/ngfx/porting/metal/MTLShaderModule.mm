#include "porting/metal/MTLShaderModule.h"
#include "porting/metal/MTLDevice.h"
#include "graphics/Config.h"
#include "DebugUtil.h"
#include "File.h"
using namespace ngfx;
using namespace std;

void MTLShaderModule::initFromFile(id<MTLDevice> device, const std::string &filename) {
    File file;
#ifdef USE_PRECOMPILED_SHADERS
    file.read(filename + ".metallib");
    initFromByteCode(device, file.data.get(), file.size);
#else
    ERR("TODO: Support runtime shader compilation");
#endif
}

void MTLShaderModule::initFromByteCode(id<MTLDevice> device, void* data, uint32_t size) {
    NSError* error;
    dispatch_data_t dispatch_data = dispatch_data_create(data, size, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    mtlLibrary = [device newLibraryWithData:dispatch_data error:&error];
    NSCAssert(mtlLibrary, @"Failed to load metal library");
    mtlFunction = [mtlLibrary newFunctionWithName:@"main0"];
}

template <typename T>
static std::unique_ptr<T> createShaderModule(Device* device, const std::string& filename) {
    auto mtlShaderModule = make_unique<T>();
    mtlShaderModule->initFromFile(mtl(device)->v, filename);
    mtlShaderModule->initBindings(filename+".metal.map");
    return mtlShaderModule;
}

unique_ptr<VertexShaderModule> VertexShaderModule::create(Device* device, const std::string& filename) {
    return createShaderModule<MTLVertexShaderModule>(device, filename);
}

unique_ptr<FragmentShaderModule> FragmentShaderModule::create(Device* device, const std::string& filename) {
    return createShaderModule<MTLFragmentShaderModule>(device, filename);
}

unique_ptr<ComputeShaderModule> ComputeShaderModule::create(Device* device, const std::string& filename) {
    return createShaderModule<MTLComputeShaderModule>(device, filename);
}
