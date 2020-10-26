#include "porting/metal/MTLPipelineUtil.h"
using namespace ngfx;

void MTLPipelineUtil::parseDescriptors(
        ShaderModule::DescriptorInfos& descriptorInfos,
        std::vector<uint32_t>& descriptorBindings) {
    uint32_t numBuffers = 0, numTextures = 0;
    for (uint32_t j = 0; j<descriptorInfos.size(); j++) {
        auto& descriptorInfo = descriptorInfos[j];
        if (descriptorInfo.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            descriptorInfo.type == DESCRIPTOR_TYPE_STORAGE_BUFFER)
            descriptorBindings.push_back(numBuffers++);
        else if (descriptorInfo.type == DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            descriptorBindings.push_back(numTextures++);
    }
}
