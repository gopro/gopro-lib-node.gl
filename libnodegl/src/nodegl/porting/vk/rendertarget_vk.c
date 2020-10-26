#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "nodegl/format.h"
#include "nodegl/porting/vk/gctx_vk.h"
#include "nodegl/log.h"
#include "nodegl/memory.h"
#include "nodegl/nodes.h"
#include "nodegl/porting/vk/rendertarget_vk.h"
#include "nodegl/porting/vk/texture_vk.h"
#include "nodegl/porting/vk/gctx_vk.h"
#include "nodegl/porting/vk/format_vk.h"

int ngli_vk_create_renderpass(struct gctx *s, const struct rendertarget_desc *desc, VkRenderPass *render_pass, int conservative)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s;
    struct vkcontext *vk = gctx_vk->vkcontext;

    int nb_descs = 0;
    VkAttachmentDescription descs[2 * (NGLI_MAX_COLOR_ATTACHMENTS + 1)] = {0};

    int nb_color_refs = 0;
    VkAttachmentReference color_refs[NGLI_MAX_COLOR_ATTACHMENTS] = {0};

    int nb_resolve_refs = 0;
    VkAttachmentReference resolve_refs[NGLI_MAX_COLOR_ATTACHMENTS+1] = {0};

    VkAttachmentReference depth_stencil_ref = {0};
    VkAttachmentReference *depth_stencil_refp = NULL;

    for (int i = 0; i < desc->nb_colors; i++) {
        VkFormat format = VK_FORMAT_UNDEFINED;
        int ret = ngli_format_get_vk_format(vk, desc->colors[i].format, &format);
        if (ret < 0)
            return ret;

        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(vk->phy_device, format, &properties);

        const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if ((properties.optimalTilingFeatures & features) != features) {
            LOG(ERROR, "format %d does not support features 0x%d", format, properties.optimalTilingFeatures);
            return NGL_ERROR_EXTERNAL;
        }

        descs[nb_descs] = (VkAttachmentDescription) {
            .format         = format,
            .samples        = ngli_vk_get_sample_count(desc->colors[i].samples),
            .loadOp         = conservative ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        color_refs[nb_color_refs].attachment = nb_descs;
        color_refs[nb_color_refs].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        nb_color_refs++;
        nb_descs++;

        if (desc->colors[i].resolve) {
            descs[nb_descs] = (VkAttachmentDescription) {
                .format         = format,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            resolve_refs[nb_resolve_refs].attachment = nb_descs;
            resolve_refs[nb_resolve_refs].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            nb_descs++;
            nb_resolve_refs++;
        }
    }

    if (desc->depth_stencil.format != NGLI_FORMAT_UNDEFINED) {
        VkFormat format = VK_FORMAT_UNDEFINED;
        int ret = ngli_format_get_vk_format(vk, desc->depth_stencil.format, &format);
        if (ret < 0)
            return ret;

        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(vk->phy_device, format, &properties);

        const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((properties.optimalTilingFeatures & features) != features) {
            LOG(ERROR, "format %d does not support features 0x%d", format, properties.optimalTilingFeatures);
            return NGL_ERROR_EXTERNAL;
        }

        descs[nb_descs] = (VkAttachmentDescription) {
            .format         = format,
            .samples        = ngli_vk_get_sample_count(desc->depth_stencil.samples),
            .loadOp         = conservative ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        depth_stencil_ref.attachment = nb_descs;
        depth_stencil_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_stencil_refp = &depth_stencil_ref;
        nb_descs++;

        if (desc->depth_stencil.resolve) {
            descs[nb_descs] = (VkAttachmentDescription) {
                .format         = format,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            };

            resolve_refs[nb_resolve_refs].attachment = nb_descs;
            resolve_refs[nb_resolve_refs].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            nb_descs++;
            nb_resolve_refs++;
        }
    }

    VkSubpassDescription subpass_description = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = nb_color_refs,
        .pColorAttachments       = color_refs,
        .pResolveAttachments     = nb_resolve_refs ? resolve_refs : NULL,
        .pDepthStencilAttachment = depth_stencil_refp,
    };

    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        }, {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        }
    };

    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = nb_descs,
        .pAttachments = descs,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = NGLI_ARRAY_NB(dependencies),
        .pDependencies = dependencies,
    };

    VkResult res = vkCreateRenderPass(vk->device, &render_pass_create_info, NULL, render_pass);
    if (res != VK_SUCCESS)
        return -1;

    return 0;
}

VkSampleCountFlagBits ngli_vk_get_sample_count(int samples)
{
    switch(samples) {
    case 0:
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64:
        return VK_SAMPLE_COUNT_64_BIT;
    default:
        ngli_assert(0);
    }
}

static VkImageAspectFlags get_vk_image_aspect_flags(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

struct rendertarget *ngli_rendertarget_vk_create(struct gctx *gctx)
{
    struct rendertarget_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct rendertarget *)s;
}

int ngli_rendertarget_vk_init(struct rendertarget *s, const struct rendertarget_params *params)
{
    struct rendertarget_vk *s_priv = (struct rendertarget_vk *)s;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;

    s->width = params->width;
    s->height = params->height;
    s->params = *params;

    ngli_assert(params->nb_colors <= NGLI_MAX_COLOR_ATTACHMENTS);

    struct rendertarget_desc desc = {0};
    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        const struct texture *texture = attachment->attachment;
        const struct texture_params *texture_params = &texture->params;
        desc.colors[desc.nb_colors].format = texture_params->format;
        desc.colors[desc.nb_colors].samples = texture_params->samples;
        desc.colors[desc.nb_colors].resolve = attachment->resolve_target != NULL;
        desc.nb_colors++;
    }

    const struct attachment *attachment = &params->depth_stencil;
    const struct texture *texture = attachment->attachment;
    if (texture) {
        const struct texture_params *texture_params = &texture->params;
        desc.depth_stencil.format = texture_params->format;
        desc.depth_stencil.samples = texture_params->samples;
        desc.depth_stencil.resolve = attachment->resolve_target != NULL;
    }

    int ret = ngli_vk_create_renderpass(s->gctx, &desc, &s_priv->render_pass, 0);
    if (ret < 0)
        return ret;

    ret = ngli_vk_create_renderpass(s->gctx, &desc, &s_priv->conservative_render_pass, 1);
    if (ret < 0)
        return ret;

    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        const struct texture_vk *texture_vk = (struct texture_vk *)attachment->attachment;
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture_vk->image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texture_vk->format,
            .subresourceRange.aspectMask = get_vk_image_aspect_flags(texture_vk->format),
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = attachment->attachment_layer,
            .subresourceRange.layerCount = 1,
        };

        VkResult vkret = vkCreateImageView(vk->device, &view_info, NULL, &s_priv->attachments[s_priv->nb_attachments]);
        if (vkret != VK_SUCCESS)
            return -1;
        s_priv->nb_attachments++;

        if (attachment->resolve_target) {
            const struct texture_vk *texture_vk = (struct texture_vk *)attachment->resolve_target;
            VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = texture_vk->image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = texture_vk->format,
                .subresourceRange.aspectMask = get_vk_image_aspect_flags(texture_vk->format),
                .subresourceRange.baseMipLevel = 0,
                .subresourceRange.levelCount = 1,
                .subresourceRange.baseArrayLayer = attachment->resolve_target_layer,
                .subresourceRange.layerCount = 1,
            };

            VkResult vkret = vkCreateImageView(vk->device, &view_info, NULL, &s_priv->attachments[s_priv->nb_attachments]);
            if (vkret != VK_SUCCESS)
                return -1;
            s_priv->nb_attachments++;
        }
    }

    attachment = &params->depth_stencil;
    texture = attachment->attachment;
    if (texture) {
        struct texture_vk *texture_vk = (struct texture_vk *)texture;
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture_vk->image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texture_vk->format,
            .subresourceRange.aspectMask = get_vk_image_aspect_flags(texture_vk->format),
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };

        VkResult vkret = vkCreateImageView(vk->device, &view_info, NULL, &s_priv->attachments[s_priv->nb_attachments]);
        if (vkret != VK_SUCCESS)
            return -1;
        s_priv->nb_attachments++;

        if (attachment->resolve_target) {
            struct texture_vk *resolve_target_vk = (struct texture_vk *)attachment->resolve_target;
            VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = resolve_target_vk->image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = resolve_target_vk->format,
                .subresourceRange.aspectMask = get_vk_image_aspect_flags(resolve_target_vk->format),
                .subresourceRange.baseMipLevel = 0,
                .subresourceRange.levelCount = 1,
                .subresourceRange.baseArrayLayer = 0,
                .subresourceRange.layerCount = 1,
            };
            VkResult vkret = vkCreateImageView(vk->device, &view_info, NULL, &s_priv->attachments[s_priv->nb_attachments]);
            if (vkret != VK_SUCCESS)
                return -1;
            s_priv->nb_attachments++;
        }
    }

    VkFramebufferCreateInfo framebuffer_create_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = s_priv->render_pass,
        .attachmentCount = s_priv->nb_attachments,
        .pAttachments    = s_priv->attachments,
        .width           = s->width,
        .height          = s->height,
        .layers          = 1,
    };

    VkResult res = vkCreateFramebuffer(vk->device, &framebuffer_create_info, NULL, &s_priv->framebuffer);
    if (res != VK_SUCCESS)
        return -1;

    if (params->readable) {
        const struct attachment *attachment = &params->colors[0];
        const struct texture *texture = attachment->attachment;
        const struct texture_params *params = &texture->params;

        s_priv->staging_buffer_size = s->width * s->height * ngli_format_get_bytes_per_pixel(params->format);
        VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = s_priv->staging_buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VkResult res = vkCreateBuffer(vk->device, &buffer_create_info, NULL, &s_priv->staging_buffer);
        if (res != VK_SUCCESS)
            return -1;

        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(vk->device, s_priv->staging_buffer, &requirements);

        VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        int32_t memory_type_index = ngli_vkcontext_find_memory_type(vk, requirements.memoryTypeBits, props);
        if (memory_type_index < 0)
            return NGL_ERROR_EXTERNAL;

        VkMemoryAllocateInfo memory_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = requirements.size,
            .memoryTypeIndex = memory_type_index,
        };
        res = vkAllocateMemory(vk->device, &memory_allocate_info, NULL, &s_priv->staging_memory);
        if (res != VK_SUCCESS)
            return NGL_ERROR_MEMORY;

        res = vkBindBufferMemory(vk->device, s_priv->staging_buffer, s_priv->staging_memory, 0);
        if (res != VK_SUCCESS)
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

void ngli_rendertarget_vk_resolve(struct rendertarget *s)
{
}

void ngli_rendertarget_vk_read_pixels(struct rendertarget *s, uint8_t *data)
{
    struct rendertarget_vk *s_priv = (struct rendertarget_vk *)s;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct rendertarget_params *params = &s->params;

    if (!params->readable) {
        LOG(ERROR, "rendertarget is not readable");
        return;
    }

    struct attachment *color = &params->colors[0];
    struct texture *src = color->resolve_target ? color->resolve_target : color->attachment;
    struct texture_vk *src_vk = (struct texture_vk *)src;

    int ret = ngli_texture_vk_transition_layout(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    if (ret < 0)
        return;

    VkCommandBuffer command_buffer = gctx_vk->cur_command_buffer;

    const VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageOffset = {0, 0, 0},
        .imageExtent = {s->width, s->height, 1},
    };

    vkCmdCopyImageToBuffer(command_buffer, src_vk->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           s_priv->staging_buffer, 1, &region);

    VkResult res = vkEndCommandBuffer(command_buffer);
    if (res != VK_SUCCESS)
        return;
    gctx_vk->cur_command_buffer_state = 0;

    VkSubmitInfo submit_info = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = ngli_darray_count(&gctx_vk->wait_semaphores),
        .pWaitSemaphores      = ngli_darray_data(&gctx_vk->wait_semaphores),
        .pWaitDstStageMask    = ngli_darray_data(&gctx_vk->wait_stages),
        .commandBufferCount   = 1,
        .pCommandBuffers      = &command_buffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores    = NULL,
    };
    ngli_darray_clear(&gctx_vk->wait_semaphores);
    ngli_darray_clear(&gctx_vk->wait_stages);

    res = vkQueueSubmit(vk->graphic_queue, 1, &submit_info, gctx_vk->fences[gctx_vk->frame_index]);
    if (res != VK_SUCCESS) {
        return;
    }

    res = vkWaitForFences(vk->device, 1, &gctx_vk->fences[gctx_vk->frame_index], VK_TRUE, UINT64_MAX);
    if (res != VK_SUCCESS)
        return;

    res = vkResetFences(vk->device, 1, &gctx_vk->fences[gctx_vk->frame_index]);
    if (res != VK_SUCCESS)
        return;

    uint8_t *mapped_data = NULL;
    res = vkMapMemory(vk->device, s_priv->staging_memory, 0, VK_WHOLE_SIZE, 0, (void**)&mapped_data);
    if (res != VK_SUCCESS)
        return;
    memcpy(data, mapped_data, s_priv->staging_buffer_size);
    vkUnmapMemory(vk->device, s_priv->staging_memory);

    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    res = vkBeginCommandBuffer(gctx_vk->cur_command_buffer, &command_buffer_begin_info);
    if (res != VK_SUCCESS)
        return;;
    gctx_vk->cur_command_buffer_state = 1;
}

static void begin_render_pass(struct rendertarget_vk *rt_vk, struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;

    if (rt_vk) {
        struct rendertarget_params *params = &rt_vk->parent.params;
        for (int i = 0; i < params->nb_colors; i++)
            ngli_texture_vk_transition_layout(params->colors[i].attachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        if (params->depth_stencil.attachment)
            ngli_texture_vk_transition_layout(params->depth_stencil.attachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        for (int i = 0; i < params->nb_colors; i++) {
            struct texture_vk *resolve_target_vk = (struct texture_vk *)params->colors[i].resolve_target;
            if (resolve_target_vk)
                resolve_target_vk->image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        struct texture_vk *resolve_target_vk = (struct texture_vk *)params->depth_stencil.resolve_target;
        if (resolve_target_vk)
            resolve_target_vk->image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }


    VkCommandBuffer cmd_buf = s_priv->cur_command_buffer;
    VkRenderPassBeginInfo render_pass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = rt_vk->render_pass,
        .framebuffer = rt_vk->framebuffer,
        .renderArea = {
            .extent.width = rt_vk->parent.width,
            .extent.height = rt_vk->parent.height,
        },
        .clearValueCount = 0,
        .pClearValues = NULL,
    };
    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

}

static void end_render_pass(struct rendertarget_vk *rt_vk, struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;

    VkCommandBuffer cmd_buf = s_priv->cur_command_buffer;
    vkCmdEndRenderPass(cmd_buf);
}

void ngli_rendertarget_vk_begin_pass(struct rendertarget *s) {
    begin_render_pass((struct rendertarget_vk *)s, s->gctx);
}
void ngli_rendertarget_vk_end_pass(struct rendertarget *s) {
    end_render_pass((struct rendertarget_vk *)s, s->gctx);
}

void ngli_rendertarget_vk_freep(struct rendertarget **sp)
{
    if (!*sp)
        return;

    struct rendertarget *s = *sp;
    struct rendertarget_vk *s_priv = (struct rendertarget_vk *)s;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;

    struct vkcontext *vk = gctx_vk->vkcontext;
    vkDestroyRenderPass(vk->device, s_priv->render_pass, NULL);
    vkDestroyRenderPass(vk->device, s_priv->conservative_render_pass, NULL);
    vkDestroyFramebuffer(vk->device, s_priv->framebuffer, NULL);

    for (int i = 0; i < s_priv->nb_attachments; i++)
        vkDestroyImageView(vk->device, s_priv->attachments[i], NULL);

    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);

    memset(s, 0, sizeof(*s));
}
