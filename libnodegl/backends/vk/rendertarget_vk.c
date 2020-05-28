#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "format.h"
#include "gctx_vk.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "rendertarget_vk.h"
#include "texture_vk.h"
#include "gctx_vk.h"
#include "format_vk.h"
#include "vkutils.h"

static const VkAttachmentLoadOp load_op_map[] = {
    [NGLI_LOAD_OP_LOAD]      = VK_ATTACHMENT_LOAD_OP_LOAD,
    [NGLI_LOAD_OP_CLEAR]     = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [NGLI_LOAD_OP_DONT_CARE] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
};

static VkAttachmentLoadOp get_vk_load_op(int load_op)
{
    return load_op_map[load_op];
}

static const VkAttachmentStoreOp store_op_map[] = {
    [NGLI_STORE_OP_STORE]     = VK_ATTACHMENT_STORE_OP_STORE,
    [NGLI_STORE_OP_DONT_CARE] = VK_ATTACHMENT_STORE_OP_DONT_CARE,
};

static VkAttachmentStoreOp get_vk_store_op(int store_op)
{
    return store_op_map[store_op];
}

static VkResult vk_create_compatible_renderpass(struct gctx *s, const struct rendertarget_desc *desc, const struct rendertarget_params *params, VkRenderPass *render_pass)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s;
    struct vkcontext *vk = gctx_vk->vkcontext;

    VkAttachmentDescription descs[2 * (NGLI_MAX_COLOR_ATTACHMENTS + 1)] = {0};
    int nb_descs = 0;

    VkAttachmentReference color_refs[NGLI_MAX_COLOR_ATTACHMENTS] = {0};
    int nb_color_refs = 0;

    VkAttachmentReference resolve_refs[NGLI_MAX_COLOR_ATTACHMENTS + 1] = {0};
    int nb_resolve_refs = 0;

    VkAttachmentReference depth_stencil_ref = {0};
    VkAttachmentReference *depth_stencil_refp = NULL;
    const VkSampleCountFlags samples = ngli_vk_get_sample_count(desc->samples);

    for (int i = 0; i < desc->nb_colors; i++) {
        VkFormat format = ngli_format_get_vk_format(vk, desc->colors[i].format);

        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(vk->phy_device, format, &properties);

        const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if ((properties.optimalTilingFeatures & features) != features) {
            LOG(ERROR, "format %d does not support features 0x%d", format, properties.optimalTilingFeatures);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        const VkAttachmentLoadOp load_op   = params ? get_vk_load_op(params->colors[i].load_op)   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        const VkAttachmentStoreOp store_op = params ? get_vk_store_op(params->colors[i].store_op) : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        descs[nb_descs] = (VkAttachmentDescription) {
            .format         = format,
            .samples        = samples,
            .loadOp         = load_op,
            .storeOp        = store_op,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        color_refs[nb_color_refs] = (VkAttachmentReference) {
            .attachment = nb_descs,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        nb_descs++;
        nb_color_refs++;

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

            resolve_refs[nb_resolve_refs] = (VkAttachmentReference) {
                .attachment = nb_descs,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            nb_descs++;
            nb_resolve_refs++;
        }
    }

    if (desc->depth_stencil.format != NGLI_FORMAT_UNDEFINED) {
        VkFormat format = ngli_format_get_vk_format(vk, desc->depth_stencil.format);

        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(vk->phy_device, format, &properties);

        const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((properties.optimalTilingFeatures & features) != features) {
            LOG(ERROR, "format %d does not support features 0x%d", format, properties.optimalTilingFeatures);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }

        const VkAttachmentLoadOp load_op = params ? get_vk_load_op(params->depth_stencil.load_op) : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        const VkAttachmentStoreOp store_op = params ? get_vk_store_op(params->depth_stencil.store_op) : VK_ATTACHMENT_STORE_OP_DONT_CARE;

        descs[nb_descs] = (VkAttachmentDescription) {
            .format         = format,
            .samples        = samples,
            .loadOp         = load_op,
            .storeOp        = store_op,
            .stencilLoadOp  = load_op,
            .stencilStoreOp = store_op,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        depth_stencil_ref = (VkAttachmentReference) {
            .attachment = nb_descs,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        nb_descs++;
        depth_stencil_refp = &depth_stencil_ref;

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

            resolve_refs[nb_resolve_refs] = (VkAttachmentReference) {
                .attachment = nb_descs,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            nb_descs++;
            nb_resolve_refs++;
        }
    }

    const VkSubpassDescription subpass_description = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = nb_color_refs,
        .pColorAttachments       = color_refs,
        .pResolveAttachments     = nb_resolve_refs ? resolve_refs : NULL,
        .pDepthStencilAttachment = depth_stencil_refp,
    };

    const VkSubpassDependency dependencies[2] = {
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

    const VkRenderPassCreateInfo render_pass_create_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = nb_descs,
        .pAttachments    = descs,
        .subpassCount    = 1,
        .pSubpasses      = &subpass_description,
        .dependencyCount = NGLI_ARRAY_NB(dependencies),
        .pDependencies   = dependencies,
    };

    return vkCreateRenderPass(vk->device, &render_pass_create_info, NULL, render_pass);
}

VkResult ngli_vk_create_compatible_renderpass(struct gctx *s, const struct rendertarget_desc *desc, VkRenderPass *render_pass)
{
    return vk_create_compatible_renderpass(s, desc, NULL, render_pass);
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

static VkResult create_framebuffer_image_view(const struct rendertarget *s, const struct texture *texture, uint32_t layer, VkImageView *view)
{
    const struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    const struct vkcontext *vk = gctx_vk->vkcontext;
    const struct texture_vk *texture_vk = (struct texture_vk *)texture;

    const VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = texture_vk->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = texture_vk->format,
        .subresourceRange = {
            .aspectMask     = get_vk_image_aspect_flags(texture_vk->format),
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = layer,
            .layerCount     = 1,
        },
    };
    return vkCreateImageView(vk->device, &view_info, NULL, view);
}

struct rendertarget *ngli_rendertarget_vk_create(struct gctx *gctx)
{
    struct rendertarget_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct rendertarget *)s;
}

VkResult ngli_rendertarget_vk_init(struct rendertarget *s, const struct rendertarget_params *params)
{
    struct rendertarget_vk *s_priv = (struct rendertarget_vk *)s;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;

    s->width = params->width;
    s->height = params->height;
    s->params = *params;

    ngli_assert(params->nb_colors <= NGLI_MAX_COLOR_ATTACHMENTS);

    int samples = -1;
    struct rendertarget_desc desc = {0};
    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        const struct texture *texture = attachment->attachment;
        const struct texture_params *texture_params = &texture->params;
        desc.colors[desc.nb_colors].format = texture_params->format;
        desc.colors[desc.nb_colors].resolve = attachment->resolve_target != NULL;
        desc.nb_colors++;
        ngli_assert(samples == -1 || samples == texture_params->samples);
        samples = texture_params->samples;
    }
    const struct attachment *attachment = &params->depth_stencil;
    const struct texture *texture = attachment->attachment;
    if (texture) {
        const struct texture_params *texture_params = &texture->params;
        desc.depth_stencil.format = texture_params->format;
        desc.depth_stencil.resolve = attachment->resolve_target != NULL;
        ngli_assert(samples == -1 || samples == texture_params->samples);
        samples = texture_params->samples;
    }
    desc.samples = samples;

    VkResult res = vk_create_compatible_renderpass(s->gctx, &desc, params, &s_priv->render_pass);
    if (res != VK_SUCCESS)
        return res;

    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];

        VkImageView view;
        res = create_framebuffer_image_view(s, attachment->attachment, attachment->attachment_layer, &view);
        if (res != VK_SUCCESS)
            return res;

        s_priv->attachments[s_priv->nb_attachments] = view;
        s_priv->nb_attachments++;

        s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {
            .color = {
                .float32 = {
                    attachment->clear_value[0],
                    attachment->clear_value[1],
                    attachment->clear_value[2],
                    attachment->clear_value[3],
                },
            }
        };
        s_priv->nb_clear_values++;

        if (attachment->resolve_target) {
            VkImageView view;
            res = create_framebuffer_image_view(s, attachment->resolve_target, attachment->resolve_target_layer, &view);
            if (res != VK_SUCCESS)
                return res;

            s_priv->attachments[s_priv->nb_attachments] = view;
            s_priv->nb_attachments++;

            s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {
                .color = {
                    .float32 = {
                        attachment->clear_value[0],
                        attachment->clear_value[1],
                        attachment->clear_value[2],
                        attachment->clear_value[3],
                    },
                }
            };
            s_priv->nb_clear_values++;
        }
    }

    attachment = &params->depth_stencil;
    texture = attachment->attachment;
    if (texture) {
        VkImageView view;
        res = create_framebuffer_image_view(s, texture, attachment->attachment_layer, &view);
        if (res != VK_SUCCESS)
            return res;

        s_priv->attachments[s_priv->nb_attachments] = view;
        s_priv->nb_attachments++;

        s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {.depthStencil = {1.f, 0}};
        s_priv->nb_clear_values++;

        if (attachment->resolve_target) {
            VkImageView view;
            res = create_framebuffer_image_view(s, attachment->resolve_target, attachment->resolve_target_layer, &view);
            if (res != VK_SUCCESS)
                return res;

            s_priv->attachments[s_priv->nb_attachments] = view;
            s_priv->nb_attachments++;

            s_priv->clear_values[s_priv->nb_clear_values] = (VkClearValue) {.depthStencil = {1.f, 0}};
            s_priv->nb_clear_values++;
        }
    }

    const VkFramebufferCreateInfo framebuffer_create_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = s_priv->render_pass,
        .attachmentCount = s_priv->nb_attachments,
        .pAttachments    = s_priv->attachments,
        .width           = s->width,
        .height          = s->height,
        .layers          = 1,
    };

    res = vkCreateFramebuffer(vk->device, &framebuffer_create_info, NULL, &s_priv->framebuffer);
    if (res != VK_SUCCESS)
        return res;

    if (params->readable) {
        const struct attachment *attachment = &params->colors[0];
        const struct texture *texture = attachment->attachment;
        const struct texture_params *params = &texture->params;

        s_priv->staging_buffer_size = s->width * s->height * ngli_format_get_bytes_per_pixel(params->format);
        const VkBufferCreateInfo buffer_create_info = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = s_priv->staging_buffer_size,
            .usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        res = vkCreateBuffer(vk->device, &buffer_create_info, NULL, &s_priv->staging_buffer);
        if (res != VK_SUCCESS)
            return res;

        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(vk->device, s_priv->staging_buffer, &requirements);

        const VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const int32_t memory_type_index = ngli_vkcontext_find_memory_type(vk, requirements.memoryTypeBits, props);
        if (memory_type_index < 0)
            return VK_ERROR_UNKNOWN;

        const VkMemoryAllocateInfo memory_allocate_info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = requirements.size,
            .memoryTypeIndex = memory_type_index,
        };
        res = vkAllocateMemory(vk->device, &memory_allocate_info, NULL, &s_priv->staging_memory);
        if (res != VK_SUCCESS)
            return res;

        res = vkBindBufferMemory(vk->device, s_priv->staging_buffer, s_priv->staging_memory, 0);
        if (res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
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
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {s->width, s->height, 1},
    };

    vkCmdCopyImageToBuffer(command_buffer, src_vk->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           s_priv->staging_buffer, 1, &region);

    VkResult res = vkEndCommandBuffer(command_buffer);
    if (res != VK_SUCCESS)
        return;
    gctx_vk->cur_command_buffer_state = 0;

    const VkSubmitInfo submit_info = {
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
    if (res != VK_SUCCESS)
        return;

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

    const VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    res = vkBeginCommandBuffer(gctx_vk->cur_command_buffer, &command_buffer_begin_info);
    if (res != VK_SUCCESS)
        return;
    gctx_vk->cur_command_buffer_state = 1;
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
    vkDestroyFramebuffer(vk->device, s_priv->framebuffer, NULL);

    for (int i = 0; i < s_priv->nb_attachments; i++)
        vkDestroyImageView(vk->device, s_priv->attachments[i], NULL);

    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    vkFreeMemory(vk->device, s_priv->staging_memory, NULL);

    memset(s, 0, sizeof(*s));
}
