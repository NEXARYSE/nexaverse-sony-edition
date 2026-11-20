/* PortheonFSR.dll - PORTHEON 5.0's FSR 1 for every UFC (the user: "an AMD FSR option in every UFC's settings").
   A Vulkan layer, VK_LAYER_PORTHEON_fsr, that PORTHEON enables for the game it starts (VK_LAYER_PATH +
   VK_INSTANCE_LAYERS, PORTHEON_FSR_SCALE, PORTHEON_FSR_SHARP). Two ways, chosen per swapchain:
   - upscale: the game is told its window is PORTHEON_FSR_SCALE of its real size (vkGetPhysicalDeviceSurfaceCapabilities:
     the current extent), so it draws a smaller frame; when it then asks for a swapchain of that size, the layer makes the
     real swapchain at the window's size, gives the game images of the size it asked for, and at each present enlarges
     the game's frame with EASU and sharpens it with RCAS into the real image;
   - sharpen only: a game that keeps asking for its window's full size (or scale 1) gets RCAS over its own frame.
   Any failure leaves the frame as the game drew it. FSR 1's EASU and RCAS: fsr_easu.frag, fsr_rcas.frag (MIT, AMD
   FidelityFX FSR 1; licenses\FSR1.txt). Licence of this file: GPL-2.0-or-later. Built from PortheonFXAA.dll's layer. */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "fsr_shaders.inc"

/* ---------------------------------------------------------------- the Vulkan subset */
typedef int32_t VkResult; typedef uint32_t VkFlags, VkBool32, VkFormat; typedef uint64_t VkDeviceSize;
typedef struct VkInstance_T *VkInstance; typedef struct VkPhysicalDevice_T *VkPhysicalDevice; typedef struct VkDevice_T *VkDevice;
typedef struct VkQueue_T *VkQueue; typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef uint64_t VkSemaphore, VkFence, VkImage, VkImageView, VkSampler, VkDeviceMemory, VkRenderPass, VkFramebuffer, VkPipeline,
    VkPipelineLayout, VkDescriptorSetLayout, VkDescriptorPool, VkDescriptorSet, VkShaderModule, VkCommandPool, VkSwapchainKHR,
    VkSurfaceKHR, VkPipelineCache;
typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (__stdcall *PFN_vkGetInstanceProcAddr)(VkInstance, const char *);
typedef PFN_vkVoidFunction (__stdcall *PFN_vkGetDeviceProcAddr)(VkDevice, const char *);
typedef VkResult (__stdcall *PFN_vkSetDeviceLoaderData)(VkDevice, void *);
#define VKAPI __stdcall
enum { VK_SUCCESS = 0, VK_ERROR_INITIALIZATION_FAILED = -3 };
typedef struct { uint32_t width, height; } VkExtent2D;
typedef struct { uint32_t width, height, depth; } VkExtent3D;
typedef struct { int32_t x, y; } VkOffset2D;
typedef struct { int32_t x, y, z; } VkOffset3D;
typedef struct { VkOffset2D offset; VkExtent2D extent; } VkRect2D;
typedef struct { float x, y, width, height, minDepth, maxDepth; } VkViewport;
typedef struct { uint32_t sType; const void *pNext; } VkBase;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; const void *pApplicationInfo; uint32_t enabledLayerCount;
                 const char *const *ppEnabledLayerNames; uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames; } VkInstanceCreateInfo;
typedef struct VkLayerInstanceLink_ { struct VkLayerInstanceLink_ *pNext; PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr; void *pfnNextGetPhysicalDeviceProcAddr; } VkLayerInstanceLink;
typedef struct { uint32_t sType; const void *pNext; uint32_t function; union { VkLayerInstanceLink *pLayerInfo; void *other; } u; } VkLayerInstanceCreateInfo;
typedef struct VkLayerDeviceLink_ { struct VkLayerDeviceLink_ *pNext; PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr; PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr; } VkLayerDeviceLink;
typedef struct { uint32_t sType; const void *pNext; uint32_t function; union { VkLayerDeviceLink *pLayerInfo; PFN_vkSetDeviceLoaderData pfnSetDeviceLoaderData; } u; } VkLayerDeviceCreateInfo;
typedef struct { uint32_t sType; void *pNext; uint32_t loaderLayerInterfaceVersion; PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr;
                 PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr; void *pfnGetPhysicalDeviceProcAddr; } VkNegotiateLayerInterface;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; VkSurfaceKHR surface; uint32_t minImageCount; VkFormat imageFormat; uint32_t imageColorSpace;
                 VkExtent2D imageExtent; uint32_t imageArrayLayers; VkFlags imageUsage; uint32_t imageSharingMode; uint32_t queueFamilyIndexCount;
                 const uint32_t *pQueueFamilyIndices; uint32_t preTransform, compositeAlpha, presentMode; VkBool32 clipped; VkSwapchainKHR oldSwapchain; } VkSwapchainCreateInfoKHR;
typedef struct { uint32_t minImageCount, maxImageCount; VkExtent2D currentExtent, minImageExtent, maxImageExtent; uint32_t maxImageArrayLayers;
                 VkFlags supportedTransforms, currentTransform, supportedCompositeAlpha, supportedUsageFlags; } VkSurfaceCapabilitiesKHR;
typedef struct { uint32_t sType; void *pNext; VkSurfaceCapabilitiesKHR surfaceCapabilities; } VkSurfaceCapabilities2KHR;
typedef struct { uint32_t sType; const void *pNext; VkSurfaceKHR surface; } VkPhysicalDeviceSurfaceInfo2KHR;
typedef struct { uint32_t sType; const void *pNext; uint32_t waitSemaphoreCount; const VkSemaphore *pWaitSemaphores; uint32_t swapchainCount;
                 const VkSwapchainKHR *pSwapchains; const uint32_t *pImageIndices; VkResult *pResults; } VkPresentInfoKHR;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t imageType; VkFormat format; VkExtent3D extent; uint32_t mipLevels, arrayLayers, samples, tiling;
                 VkFlags usage; uint32_t sharingMode, queueFamilyIndexCount; const uint32_t *pQueueFamilyIndices; uint32_t initialLayout; } VkImageCreateInfo;
typedef struct { VkDeviceSize size, alignment; uint32_t memoryTypeBits; } VkMemoryRequirements;
typedef struct { uint32_t sType; const void *pNext; VkDeviceSize allocationSize; uint32_t memoryTypeIndex; } VkMemoryAllocateInfo;
typedef struct { VkFlags propertyFlags; uint32_t heapIndex; } VkMemoryType;
typedef struct { VkDeviceSize size; VkFlags flags; } VkMemoryHeap;
typedef struct { uint32_t memoryTypeCount; VkMemoryType memoryTypes[32]; uint32_t memoryHeapCount; VkMemoryHeap memoryHeaps[16]; } VkPhysicalDeviceMemoryProperties;
typedef struct { VkFlags queueFlags; uint32_t queueCount, timestampValidBits; VkExtent3D minImageTransferGranularity; } VkQueueFamilyProperties;
typedef struct { VkFlags aspectMask; uint32_t baseMipLevel, levelCount, baseArrayLayer, layerCount; } VkImageSubresourceRange;
typedef struct { VkFlags aspectMask; uint32_t mipLevel, baseArrayLayer, layerCount; } VkImageSubresourceLayers;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; VkImage image; uint32_t viewType; VkFormat format; uint32_t components[4]; VkImageSubresourceRange subresourceRange; } VkImageViewCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t magFilter, minFilter, mipmapMode, addressModeU, addressModeV, addressModeW; float mipLodBias;
                 VkBool32 anisotropyEnable; float maxAnisotropy; VkBool32 compareEnable; uint32_t compareOp; float minLod, maxLod; uint32_t borderColor; VkBool32 unnormalizedCoordinates; } VkSamplerCreateInfo;
typedef struct { VkFlags flags; VkFormat format; uint32_t samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalLayout; } VkAttachmentDescription;
typedef struct { uint32_t attachment, layout; } VkAttachmentReference;
typedef struct { VkFlags flags; uint32_t pipelineBindPoint, inputAttachmentCount; const VkAttachmentReference *pInputAttachments; uint32_t colorAttachmentCount;
                 const VkAttachmentReference *pColorAttachments, *pResolveAttachments, *pDepthStencilAttachment; uint32_t preserveAttachmentCount; const uint32_t *pPreserveAttachments; } VkSubpassDescription;
typedef struct { uint32_t srcSubpass, dstSubpass; VkFlags srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, dependencyFlags; } VkSubpassDependency;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t attachmentCount; const VkAttachmentDescription *pAttachments; uint32_t subpassCount;
                 const VkSubpassDescription *pSubpasses; uint32_t dependencyCount; const VkSubpassDependency *pDependencies; } VkRenderPassCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; VkRenderPass renderPass; uint32_t attachmentCount; const VkImageView *pAttachments; uint32_t width, height, layers; } VkFramebufferCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; size_t codeSize; const uint32_t *pCode; } VkShaderModuleCreateInfo;
typedef struct { uint32_t binding, descriptorType, descriptorCount; VkFlags stageFlags; const VkSampler *pImmutableSamplers; } VkDescriptorSetLayoutBinding;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t bindingCount; const VkDescriptorSetLayoutBinding *pBindings; } VkDescriptorSetLayoutCreateInfo;
typedef struct { VkFlags stageFlags; uint32_t offset, size; } VkPushConstantRange;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t setLayoutCount; const VkDescriptorSetLayout *pSetLayouts; uint32_t pushConstantRangeCount; const VkPushConstantRange *pPushConstantRanges; } VkPipelineLayoutCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t stage; VkShaderModule module; const char *pName; const void *pSpecializationInfo; } VkPipelineShaderStageCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t vertexBindingDescriptionCount; const void *pVertexBindingDescriptions; uint32_t vertexAttributeDescriptionCount; const void *pVertexAttributeDescriptions; } VkPipelineVertexInputStateCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t topology; VkBool32 primitiveRestartEnable; } VkPipelineInputAssemblyStateCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t viewportCount; const VkViewport *pViewports; uint32_t scissorCount; const VkRect2D *pScissors; } VkPipelineViewportStateCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; VkBool32 depthClampEnable, rasterizerDiscardEnable; uint32_t polygonMode; VkFlags cullMode; uint32_t frontFace;
                 VkBool32 depthBiasEnable; float depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor, lineWidth; } VkPipelineRasterizationStateCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t rasterizationSamples; VkBool32 sampleShadingEnable; float minSampleShading; const uint32_t *pSampleMask;
                 VkBool32 alphaToCoverageEnable, alphaToOneEnable; } VkPipelineMultisampleStateCreateInfo;
typedef struct { VkBool32 blendEnable; uint32_t srcColorBlendFactor, dstColorBlendFactor, colorBlendOp, srcAlphaBlendFactor, dstAlphaBlendFactor, alphaBlendOp; VkFlags colorWriteMask; } VkPipelineColorBlendAttachmentState;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; VkBool32 logicOpEnable; uint32_t logicOp, attachmentCount; const VkPipelineColorBlendAttachmentState *pAttachments; float blendConstants[4]; } VkPipelineColorBlendStateCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t dynamicStateCount; const uint32_t *pDynamicStates; } VkPipelineDynamicStateCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t stageCount; const VkPipelineShaderStageCreateInfo *pStages;
                 const VkPipelineVertexInputStateCreateInfo *pVertexInputState; const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyState; const void *pTessellationState;
                 const VkPipelineViewportStateCreateInfo *pViewportState; const VkPipelineRasterizationStateCreateInfo *pRasterizationState; const VkPipelineMultisampleStateCreateInfo *pMultisampleState;
                 const void *pDepthStencilState; const VkPipelineColorBlendStateCreateInfo *pColorBlendState; const VkPipelineDynamicStateCreateInfo *pDynamicState;
                 VkPipelineLayout layout; VkRenderPass renderPass; uint32_t subpass; VkPipeline basePipelineHandle; int32_t basePipelineIndex; } VkGraphicsPipelineCreateInfo;
typedef struct { uint32_t type, descriptorCount; } VkDescriptorPoolSize;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t maxSets, poolSizeCount; const VkDescriptorPoolSize *pPoolSizes; } VkDescriptorPoolCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkDescriptorPool descriptorPool; uint32_t descriptorSetCount; const VkDescriptorSetLayout *pSetLayouts; } VkDescriptorSetAllocateInfo;
typedef struct { VkSampler sampler; VkImageView imageView; uint32_t imageLayout; } VkDescriptorImageInfo;
typedef struct { uint32_t sType; const void *pNext; VkDescriptorSet dstSet; uint32_t dstBinding, dstArrayElement, descriptorCount, descriptorType;
                 const VkDescriptorImageInfo *pImageInfo; const void *pBufferInfo; const void *pTexelBufferView; } VkWriteDescriptorSet;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t queueFamilyIndex; } VkCommandPoolCreateInfo;
typedef struct { uint32_t sType; const void *pNext; VkCommandPool commandPool; uint32_t level, commandBufferCount; } VkCommandBufferAllocateInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; const void *pInheritanceInfo; } VkCommandBufferBeginInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags srcAccessMask, dstAccessMask; uint32_t oldLayout, newLayout, srcQueueFamilyIndex, dstQueueFamilyIndex;
                 VkImage image; VkImageSubresourceRange subresourceRange; } VkImageMemoryBarrier;
typedef struct { VkImageSubresourceLayers srcSubresource; VkOffset3D srcOffset; VkImageSubresourceLayers dstSubresource; VkOffset3D dstOffset; VkExtent3D extent; } VkImageCopy;
typedef struct { uint32_t sType; const void *pNext; VkRenderPass renderPass; VkFramebuffer framebuffer; VkRect2D renderArea; uint32_t clearValueCount; const void *pClearValues; } VkRenderPassBeginInfo;
typedef struct { uint32_t sType; const void *pNext; uint32_t waitSemaphoreCount; const VkSemaphore *pWaitSemaphores; const VkFlags *pWaitDstStageMask;
                 uint32_t commandBufferCount; const VkCommandBuffer *pCommandBuffers; uint32_t signalSemaphoreCount; const VkSemaphore *pSignalSemaphores; } VkSubmitInfo;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; } VkSimpleCreateInfo;   /* semaphore, fence */
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t queueFamilyIndex, queueIndex; } VkDeviceQueueInfo2;
typedef struct { uint32_t sType; const void *pNext; VkFlags flags; uint32_t queueCreateInfoCount; const void *pQueueCreateInfos; uint32_t enabledLayerCount;
                 const char *const *ppEnabledLayerNames; uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames; const void *pEnabledFeatures; } VkDeviceCreateInfo;

/* structure types and the other constants used */
#define ST_SUBMIT 4
#define ST_MEMALLOC 5
#define ST_FENCE 8
#define ST_SEMAPHORE 9
#define ST_IMAGE 14
#define ST_IMAGEVIEW 15
#define ST_SHADER 16
#define ST_STAGE 18
#define ST_VERTEXINPUT 19
#define ST_ASSEMBLY 20
#define ST_VIEWPORT 22
#define ST_RASTER 23
#define ST_MULTISAMPLE 24
#define ST_BLEND 26
#define ST_DYNAMIC 27
#define ST_GRAPHICSPIPE 28
#define ST_PIPELAYOUT 30
#define ST_SAMPLER 31
#define ST_SETLAYOUT 32
#define ST_DESCPOOL 33
#define ST_DESCALLOC 34
#define ST_WRITEDESC 35
#define ST_FRAMEBUFFER 37
#define ST_RENDERPASS 38
#define ST_CMDPOOL 39
#define ST_CMDALLOC 40
#define ST_CMDBEGIN 42
#define ST_RPBEGIN 43
#define ST_IMGBARRIER 45
#define ST_LOADER_INSTANCE 47
#define ST_LOADER_DEVICE 48
#define LAYER_LINK_INFO 0
#define LOADER_DATA_CALLBACK 1
#define LAYOUT_UNDEFINED 0
#define LAYOUT_COLOR 2
#define LAYOUT_SHADER_READ 5
#define LAYOUT_TRANSFER_SRC 6
#define LAYOUT_TRANSFER_DST 7
#define LAYOUT_PRESENT 1000001002u
#define USAGE_TRANSFER_SRC 0x1
#define USAGE_TRANSFER_DST 0x2
#define USAGE_SAMPLED 0x4
#define USAGE_COLOR 0x10
#define STAGE_FRAGMENT 0x80
#define STAGE_COLOR_OUT 0x400
#define STAGE_TRANSFER 0x1000
#define STAGE_ALL 0x10000
#define ACCESS_SHADER_READ 0x20
#define ACCESS_COLOR_WRITE 0x100
#define ACCESS_TRANSFER_READ 0x800
#define ACCESS_TRANSFER_WRITE 0x1000
#define ACCESS_MEMORY_WRITE 0x10000   /* the game's frame, also when it presents without a semaphore */
#define QUEUE_IGNORED 0xFFFFFFFFu

/* ---------------------------------------------------------------- a full-screen triangle, SPIR-V 1.0 written by hand:
   gl_Position = vec4(gl_VertexIndex == 1 ? 3 : -1, gl_VertexIndex == 2 ? 3 : -1, 0, 1) */
static const uint32_t TRIANGLE_VS[] = {
    0x07230203, 0x00010000, 0, 25, 0,
    0x00020011, 1,                                      /* OpCapability Shader */
    0x0003000E, 0, 1,                                   /* OpMemoryModel Logical GLSL450 */
    0x0007000F, 0, 1, 0x6E69616D, 0, 2, 3,              /* OpEntryPoint Vertex %1 "main" %2 %3 */
    0x00040047, 2, 11, 42,                              /* OpDecorate %2 BuiltIn VertexIndex */
    0x00040047, 3, 11, 0,                               /* OpDecorate %3 BuiltIn Position */
    0x00020013, 4,                                      /* %4 void */
    0x00030021, 5, 4,                                   /* %5 fn void() */
    0x00040015, 6, 32, 1,                               /* %6 int */
    0x00030016, 7, 32,                                  /* %7 float */
    0x00040017, 8, 7, 4,                                /* %8 vec4 */
    0x00020014, 9,                                      /* %9 bool */
    0x00040020, 10, 1, 6,                               /* %10 Input int* */
    0x00040020, 11, 3, 8,                               /* %11 Output vec4* */
    0x0004003B, 10, 2, 1,                               /* %2 gl_VertexIndex */
    0x0004003B, 11, 3, 3,                               /* %3 gl_Position */
    0x0004002B, 6, 12, 1,                               /* %12 int 1 */
    0x0004002B, 6, 13, 2,                               /* %13 int 2 */
    0x0004002B, 7, 14, 0xBF800000,                      /* %14 -1.0 */
    0x0004002B, 7, 15, 0x40400000,                      /* %15 3.0 */
    0x0004002B, 7, 16, 0x00000000,                      /* %16 0.0 */
    0x0004002B, 7, 17, 0x3F800000,                      /* %17 1.0 */
    0x00050036, 4, 1, 0, 5,                             /* %1 main */
    0x000200F8, 18,                                     /* label */
    0x0004003D, 6, 19, 2,                               /* %19 = load vertex index */
    0x000500AA, 9, 20, 19, 12,                          /* %20 = %19 == 1 */
    0x000500AA, 9, 21, 19, 13,                          /* %21 = %19 == 2 */
    0x000600A9, 7, 22, 20, 15, 14,                      /* %22 = %20 ? 3 : -1 */
    0x000600A9, 7, 23, 21, 15, 14,                      /* %23 = %21 ? 3 : -1 */
    0x00070050, 8, 24, 22, 23, 16, 17,                  /* %24 = vec4(%22, %23, 0, 1) */
    0x0003003E, 3, 24,                                  /* store gl_Position */
    0x000100FD,                                         /* return */
    0x00010038,                                         /* function end */
};

/* ---------------------------------------------------------------- dispatch: the next layer's functions */
#define NEXT(f) PFN_vkVoidFunction f
typedef struct {
    void *key; VkInstance instance; PFN_vkGetInstanceProcAddr gipa;
    void (VKAPI *DestroyInstance)(VkInstance, const void *);
    VkResult (VKAPI *CreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo *, const void *, VkDevice *);
    void (VKAPI *GetMemProps)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *);
    void (VKAPI *GetQueueFamilies)(VkPhysicalDevice, uint32_t *, VkQueueFamilyProperties *);
    VkResult (VKAPI *GetSurfaceCaps)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *);
    VkResult (VKAPI *GetSurfaceCaps2)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, VkSurfaceCapabilities2KHR *);
} Inst;
typedef struct Swap_ Swap;
typedef struct {
    void *key; VkDevice device; PFN_vkGetDeviceProcAddr gdpa; PFN_vkSetDeviceLoaderData setData; Inst *inst; VkPhysicalDevice phys;
    VkPhysicalDeviceMemoryProperties mem; uint32_t graphicsFamilies;   /* bit per queue family with graphics */
    struct { VkQueue q; uint32_t family; } queues[16]; int nqueues;
    BOOL broken;   /* something failed: frames pass untouched from then on */
    VkShaderModule vs, fs, fs2; VkDescriptorSetLayout dsl; VkPipelineLayout layout; VkSampler sampler;
    void (VKAPI *DestroyDevice)(VkDevice, const void *);
    void (VKAPI *GetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue *);
    void (VKAPI *GetDeviceQueue2)(VkDevice, const VkDeviceQueueInfo2 *, VkQueue *);
    VkResult (VKAPI *CreateSwapchain)(VkDevice, const VkSwapchainCreateInfoKHR *, const void *, VkSwapchainKHR *);
    void (VKAPI *DestroySwapchain)(VkDevice, VkSwapchainKHR, const void *);
    VkResult (VKAPI *GetSwapchainImages)(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
    VkResult (VKAPI *QueuePresent)(VkQueue, const VkPresentInfoKHR *);
    VkResult (VKAPI *CreateImage)(VkDevice, const VkImageCreateInfo *, const void *, VkImage *);
    void (VKAPI *DestroyImage)(VkDevice, VkImage, const void *);
    void (VKAPI *GetImageMemReq)(VkDevice, VkImage, VkMemoryRequirements *);
    VkResult (VKAPI *AllocateMemory)(VkDevice, const VkMemoryAllocateInfo *, const void *, VkDeviceMemory *);
    void (VKAPI *FreeMemory)(VkDevice, VkDeviceMemory, const void *);
    VkResult (VKAPI *BindImageMemory)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
    VkResult (VKAPI *CreateImageView)(VkDevice, const VkImageViewCreateInfo *, const void *, VkImageView *);
    void (VKAPI *DestroyImageView)(VkDevice, VkImageView, const void *);
    VkResult (VKAPI *CreateSampler)(VkDevice, const VkSamplerCreateInfo *, const void *, VkSampler *);
    void (VKAPI *DestroySampler)(VkDevice, VkSampler, const void *);
    VkResult (VKAPI *CreateRenderPass)(VkDevice, const VkRenderPassCreateInfo *, const void *, VkRenderPass *);
    void (VKAPI *DestroyRenderPass)(VkDevice, VkRenderPass, const void *);
    VkResult (VKAPI *CreateFramebuffer)(VkDevice, const VkFramebufferCreateInfo *, const void *, VkFramebuffer *);
    void (VKAPI *DestroyFramebuffer)(VkDevice, VkFramebuffer, const void *);
    VkResult (VKAPI *CreateShaderModule)(VkDevice, const VkShaderModuleCreateInfo *, const void *, VkShaderModule *);
    void (VKAPI *DestroyShaderModule)(VkDevice, VkShaderModule, const void *);
    VkResult (VKAPI *CreateSetLayout)(VkDevice, const VkDescriptorSetLayoutCreateInfo *, const void *, VkDescriptorSetLayout *);
    void (VKAPI *DestroySetLayout)(VkDevice, VkDescriptorSetLayout, const void *);
    VkResult (VKAPI *CreatePipelineLayout)(VkDevice, const VkPipelineLayoutCreateInfo *, const void *, VkPipelineLayout *);
    void (VKAPI *DestroyPipelineLayout)(VkDevice, VkPipelineLayout, const void *);
    VkResult (VKAPI *CreateGraphicsPipelines)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo *, const void *, VkPipeline *);
    void (VKAPI *DestroyPipeline)(VkDevice, VkPipeline, const void *);
    VkResult (VKAPI *CreateDescPool)(VkDevice, const VkDescriptorPoolCreateInfo *, const void *, VkDescriptorPool *);
    void (VKAPI *DestroyDescPool)(VkDevice, VkDescriptorPool, const void *);
    VkResult (VKAPI *AllocDescSets)(VkDevice, const VkDescriptorSetAllocateInfo *, VkDescriptorSet *);
    void (VKAPI *UpdateDescSets)(VkDevice, uint32_t, const VkWriteDescriptorSet *, uint32_t, const void *);
    VkResult (VKAPI *CreateCmdPool)(VkDevice, const VkCommandPoolCreateInfo *, const void *, VkCommandPool *);
    void (VKAPI *DestroyCmdPool)(VkDevice, VkCommandPool, const void *);
    VkResult (VKAPI *AllocCmdBuffers)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
    VkResult (VKAPI *BeginCmd)(VkCommandBuffer, const VkCommandBufferBeginInfo *);
    VkResult (VKAPI *EndCmd)(VkCommandBuffer);
    void (VKAPI *CmdBarrier)(VkCommandBuffer, VkFlags, VkFlags, VkFlags, uint32_t, const void *, uint32_t, const void *, uint32_t, const VkImageMemoryBarrier *);
    void (VKAPI *CmdCopyImage)(VkCommandBuffer, VkImage, uint32_t, VkImage, uint32_t, uint32_t, const VkImageCopy *);
    void (VKAPI *CmdBeginRP)(VkCommandBuffer, const VkRenderPassBeginInfo *, uint32_t);
    void (VKAPI *CmdEndRP)(VkCommandBuffer);
    void (VKAPI *CmdBindPipeline)(VkCommandBuffer, uint32_t, VkPipeline);
    void (VKAPI *CmdBindSets)(VkCommandBuffer, uint32_t, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet *, uint32_t, const uint32_t *);
    void (VKAPI *CmdPush)(VkCommandBuffer, VkPipelineLayout, VkFlags, uint32_t, uint32_t, const void *);
    void (VKAPI *CmdViewport)(VkCommandBuffer, uint32_t, uint32_t, const VkViewport *);
    void (VKAPI *CmdScissor)(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D *);
    void (VKAPI *CmdDraw)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
    VkResult (VKAPI *QueueSubmit)(VkQueue, uint32_t, const VkSubmitInfo *, VkFence);
    VkResult (VKAPI *CreateSemaphore)(VkDevice, const VkSimpleCreateInfo *, const void *, VkSemaphore *);
    void (VKAPI *DestroySemaphore)(VkDevice, VkSemaphore, const void *);
    VkResult (VKAPI *CreateFence)(VkDevice, const VkSimpleCreateInfo *, const void *, VkFence *);
    void (VKAPI *DestroyFence)(VkDevice, VkFence, const void *);
    VkResult (VKAPI *WaitFences)(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t);
    VkResult (VKAPI *ResetFences)(VkDevice, uint32_t, const VkFence *);
    VkResult (VKAPI *DeviceWaitIdle)(VkDevice);
    Swap *swaps;
} Dev;
#define MAXIMG 8
struct Swap_ {
    Swap *next; VkSwapchainKHR sc; VkFormat format; VkExtent2D extent; BOOL usable; uint32_t family;
    BOOL proxy; VkExtent2D appExtent;   /* upscale: the game draws into app[] (appExtent), the layer fills img[] (extent) */
    uint32_t n; VkImage img[MAXIMG]; VkImageView view[MAXIMG]; VkFramebuffer fb[MAXIMG];
    VkImage app[MAXIMG]; VkDeviceMemory appMem[MAXIMG]; VkImageView appView[MAXIMG];
    VkImage mid[MAXIMG]; VkDeviceMemory midMem[MAXIMG]; VkImageView midView[MAXIMG]; VkFramebuffer midFb[MAXIMG];   /* EASU's result, RCAS's source */
    VkImage copy[MAXIMG]; VkDeviceMemory copyMem[MAXIMG]; VkImageView copyView[MAXIMG];   /* sharpen only: the game's frame */
    VkDescriptorSet setE[MAXIMG], setR[MAXIMG];
    VkCommandBuffer cmd[MAXIMG]; VkFence fence[MAXIMG]; VkSemaphore done[MAXIMG];
    VkRenderPass rp, rpMid; VkPipeline pipeE, pipeR; VkDescriptorPool pool; VkCommandPool cmdPool; BOOL built;
};
static Inst g_inst[8]; static Dev g_dev[8]; static CRITICAL_SECTION g_lock; static FILE *g_log;
/* 6.2: texture quality for any Vulkan game (PORTHEON_TEX_ANISO 2..16, PORTHEON_TEX_BIAS): every trilinear/bilinear sampler the game makes gets the
   anisotropic filter and a mip bias; the feature is switched on at vkCreateDevice. Without PORTHEON_FSR_SCALE the layer leaves the swapchain alone */
static BOOL g_fsrOn, g_texOn; static int g_texAniso; static float g_texBias; static volatile LONG g_smpAll, g_smpMod;
static float g_scale = 1.0f, g_sharp = 0.25f;   /* PORTHEON_FSR_SCALE (0.5 .. 1), PORTHEON_FSR_SHARP (stops; below 0: no RCAS) */
static BOOL Sharpen(void) { return g_sharp >= 0.0f; }
static void Log(const char *fmt, ...)
{
    if (!g_log) return;
    va_list a; va_start(a, fmt); SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds); vfprintf(g_log, fmt, a); fputc('\n', g_log); fflush(g_log);
    va_end(a);
}
static void *Key(const void *h) { return *(void **)h; }
static Inst *FindInst(const void *h) { void *k = Key(h); for (int i = 0; i < 8; i++) if (g_inst[i].key == k) return &g_inst[i]; return NULL; }
static Dev *FindDev(const void *h) { void *k = Key(h); for (int i = 0; i < 8; i++) if (g_dev[i].key == k) return &g_dev[i]; return NULL; }

/* ---------------------------------------------------------------- instance */
static VkResult VKAPI L_CreateInstance(const VkInstanceCreateInfo *ci, const void *alloc, VkInstance *out)
{
    VkLayerInstanceCreateInfo *li = (VkLayerInstanceCreateInfo *)ci->pNext;
    while (li && !(li->sType == ST_LOADER_INSTANCE && li->function == LAYER_LINK_INFO)) li = (VkLayerInstanceCreateInfo *)li->pNext;
    if (!li) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr next = li->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    li->u.pLayerInfo = li->u.pLayerInfo->pNext;   /* the next layer reads its own link */
    VkResult (VKAPI *create)(const VkInstanceCreateInfo *, const void *, VkInstance *) = (void *)next(NULL, "vkCreateInstance");
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = create(ci, alloc, out);
    if (r != VK_SUCCESS) return r;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < 8; i++) if (!g_inst[i].key) {
        Inst *s = &g_inst[i]; s->key = Key(*out); s->instance = *out; s->gipa = next;
        s->DestroyInstance = (void *)next(*out, "vkDestroyInstance");
        s->CreateDevice = (void *)next(*out, "vkCreateDevice");
        s->GetMemProps = (void *)next(*out, "vkGetPhysicalDeviceMemoryProperties");
        s->GetQueueFamilies = (void *)next(*out, "vkGetPhysicalDeviceQueueFamilyProperties");
        s->GetSurfaceCaps = (void *)next(*out, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        s->GetSurfaceCaps2 = (void *)next(*out, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
        break;
    }
    LeaveCriticalSection(&g_lock);
    return r;
}
static void VKAPI L_DestroyInstance(VkInstance inst, const void *alloc)
{
    Inst *s = FindInst(inst); if (!s) return;
    void (VKAPI *destroy)(VkInstance, const void *) = s->DestroyInstance;
    EnterCriticalSection(&g_lock); memset(s, 0, sizeof(*s)); LeaveCriticalSection(&g_lock);
    if (destroy) destroy(inst, alloc);
}

/* the game is told its window is smaller (only the current extent: the real size stays allowed) */
static void Shrink(VkSurfaceCapabilitiesKHR *c)
{
    if (g_scale >= 0.999f || c->currentExtent.width == 0xFFFFFFFFu || !c->currentExtent.width || !c->currentExtent.height) return;
    uint32_t w = (uint32_t)(c->currentExtent.width * g_scale + 0.5f) & ~1u, h = (uint32_t)(c->currentExtent.height * g_scale + 0.5f) & ~1u;
    if (w < 64 || h < 64) return;
    if (w < c->minImageExtent.width) w = c->minImageExtent.width; if (h < c->minImageExtent.height) h = c->minImageExtent.height;
    c->currentExtent.width = w; c->currentExtent.height = h;
}
static VkResult VKAPI L_GetSurfaceCaps(VkPhysicalDevice phys, VkSurfaceKHR surf, VkSurfaceCapabilitiesKHR *caps)
{
    Inst *s = FindInst(phys); if (!s || !s->GetSurfaceCaps) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = s->GetSurfaceCaps(phys, surf, caps); if (r == VK_SUCCESS) Shrink(caps); return r;
}
static VkResult VKAPI L_GetSurfaceCaps2(VkPhysicalDevice phys, const VkPhysicalDeviceSurfaceInfo2KHR *info, VkSurfaceCapabilities2KHR *caps)
{
    Inst *s = FindInst(phys); if (!s || !s->GetSurfaceCaps2) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r = s->GetSurfaceCaps2(phys, info, caps); if (r == VK_SUCCESS) Shrink(&caps->surfaceCapabilities); return r;
}

/* ---------------------------------------------------------------- device */
#define GET(f, name) d->f = (void *)gdpa(*out, name)
static VkResult VKAPI L_CreateDevice(VkPhysicalDevice phys, const VkDeviceCreateInfo *ci, const void *alloc, VkDevice *out)
{
    Inst *inst = FindInst(phys); if (!inst) return VK_ERROR_INITIALIZATION_FAILED;
    VkLayerDeviceCreateInfo *link = (VkLayerDeviceCreateInfo *)ci->pNext, *data = (VkLayerDeviceCreateInfo *)ci->pNext;
    while (link && !(link->sType == ST_LOADER_DEVICE && link->function == LAYER_LINK_INFO)) link = (VkLayerDeviceCreateInfo *)link->pNext;
    while (data && !(data->sType == ST_LOADER_DEVICE && data->function == LOADER_DATA_CALLBACK)) data = (VkLayerDeviceCreateInfo *)data->pNext;
    if (!link) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    VkResult (VKAPI *create)(VkPhysicalDevice, const VkDeviceCreateInfo *, const void *, VkDevice *) = (void *)gipa(inst->instance, "vkCreateDevice");
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    VkDeviceCreateInfo ci2 = *ci; uint32_t feat[55]; uint32_t *f2 = NULL, f2old = 0;
    if (g_texOn && g_texAniso > 1) {   /* 6.2: samplerAnisotropy (the 20th VkBool32 of VkPhysicalDeviceFeatures), when the card has it */
        void (VKAPI *getf)(VkPhysicalDevice, uint32_t *) = (void *)inst->gipa(inst->instance, "vkGetPhysicalDeviceFeatures"); uint32_t have[55]; memset(have, 0, sizeof(have));
        if (getf) getf(phys, have);
        if (!have[19]) { g_texAniso = 0; Log("textures: this card has no anisotropic filter"); }
        else {
            typedef struct { uint32_t sType; void *pNext; uint32_t f[55]; } F2; const F2 *p = (const F2 *)ci->pNext;
            while (p && p->sType != 1000059000u) p = (const F2 *)p->pNext;
            if (p) { f2 = (uint32_t *)&p->f[19]; f2old = *f2; *f2 = 1; }
            else { if (ci->pEnabledFeatures) memcpy(feat, ci->pEnabledFeatures, sizeof(feat)); else memset(feat, 0, sizeof(feat)); feat[19] = 1; ci2.pEnabledFeatures = feat; }
        }
    }
    VkResult r = create(phys, &ci2, alloc, out);
    if (f2) *f2 = f2old;
    if (r != VK_SUCCESS) return r;
    EnterCriticalSection(&g_lock);
    Dev *d = NULL; for (int i = 0; i < 8 && !d; i++) if (!g_dev[i].key) d = &g_dev[i];
    if (d) {
        memset(d, 0, sizeof(*d)); d->key = Key(*out); d->device = *out; d->gdpa = gdpa; d->inst = inst; d->phys = phys;
        d->setData = data ? data->u.pfnSetDeviceLoaderData : NULL;
        if (inst->GetMemProps) inst->GetMemProps(phys, &d->mem);
        if (inst->GetQueueFamilies) {
            uint32_t n = 0; inst->GetQueueFamilies(phys, &n, NULL); VkQueueFamilyProperties qf[32]; if (n > 32) n = 32;
            inst->GetQueueFamilies(phys, &n, qf); for (uint32_t i = 0; i < n; i++) if (qf[i].queueFlags & 1) d->graphicsFamilies |= 1u << i;
        }
        GET(DestroyDevice, "vkDestroyDevice"); GET(GetDeviceQueue, "vkGetDeviceQueue"); GET(GetDeviceQueue2, "vkGetDeviceQueue2");
        GET(CreateSwapchain, "vkCreateSwapchainKHR"); GET(DestroySwapchain, "vkDestroySwapchainKHR"); GET(GetSwapchainImages, "vkGetSwapchainImagesKHR");
        GET(QueuePresent, "vkQueuePresentKHR"); GET(CreateImage, "vkCreateImage"); GET(DestroyImage, "vkDestroyImage");
        GET(GetImageMemReq, "vkGetImageMemoryRequirements"); GET(AllocateMemory, "vkAllocateMemory"); GET(FreeMemory, "vkFreeMemory");
        GET(BindImageMemory, "vkBindImageMemory"); GET(CreateImageView, "vkCreateImageView"); GET(DestroyImageView, "vkDestroyImageView");
        GET(CreateSampler, "vkCreateSampler"); GET(DestroySampler, "vkDestroySampler"); GET(CreateRenderPass, "vkCreateRenderPass");
        GET(DestroyRenderPass, "vkDestroyRenderPass"); GET(CreateFramebuffer, "vkCreateFramebuffer"); GET(DestroyFramebuffer, "vkDestroyFramebuffer");
        GET(CreateShaderModule, "vkCreateShaderModule"); GET(DestroyShaderModule, "vkDestroyShaderModule");
        GET(CreateSetLayout, "vkCreateDescriptorSetLayout"); GET(DestroySetLayout, "vkDestroyDescriptorSetLayout");
        GET(CreatePipelineLayout, "vkCreatePipelineLayout"); GET(DestroyPipelineLayout, "vkDestroyPipelineLayout");
        GET(CreateGraphicsPipelines, "vkCreateGraphicsPipelines"); GET(DestroyPipeline, "vkDestroyPipeline");
        GET(CreateDescPool, "vkCreateDescriptorPool"); GET(DestroyDescPool, "vkDestroyDescriptorPool"); GET(AllocDescSets, "vkAllocateDescriptorSets");
        GET(UpdateDescSets, "vkUpdateDescriptorSets"); GET(CreateCmdPool, "vkCreateCommandPool"); GET(DestroyCmdPool, "vkDestroyCommandPool");
        GET(AllocCmdBuffers, "vkAllocateCommandBuffers"); GET(BeginCmd, "vkBeginCommandBuffer"); GET(EndCmd, "vkEndCommandBuffer");
        GET(CmdBarrier, "vkCmdPipelineBarrier"); GET(CmdCopyImage, "vkCmdCopyImage"); GET(CmdBeginRP, "vkCmdBeginRenderPass");
        GET(CmdEndRP, "vkCmdEndRenderPass"); GET(CmdBindPipeline, "vkCmdBindPipeline"); GET(CmdBindSets, "vkCmdBindDescriptorSets");
        GET(CmdPush, "vkCmdPushConstants"); GET(CmdViewport, "vkCmdSetViewport"); GET(CmdScissor, "vkCmdSetScissor"); GET(CmdDraw, "vkCmdDraw");
        GET(QueueSubmit, "vkQueueSubmit"); GET(CreateSemaphore, "vkCreateSemaphore"); GET(DestroySemaphore, "vkDestroySemaphore");
        GET(CreateFence, "vkCreateFence"); GET(DestroyFence, "vkDestroyFence"); GET(WaitFences, "vkWaitForFences"); GET(ResetFences, "vkResetFences");
        GET(DeviceWaitIdle, "vkDeviceWaitIdle");
        if (!d->setData || !d->QueuePresent || !d->CreateGraphicsPipelines) { d->broken = TRUE; Log("device %p: missing functions, FSR off", (void *)*out); }
        else Log("device %p: FSR ready (scale %.3f, sharpness %.2f), graphics families 0x%x", (void *)*out, g_scale, g_sharp, d->graphicsFamilies);
    }
    LeaveCriticalSection(&g_lock);
    return r;
}
static void FreeSwap(Dev *d, Swap *s);
static void VKAPI L_DestroyDevice(VkDevice dev, const void *alloc)
{
    Dev *d = FindDev(dev); if (!d) return;
    if (d->DeviceWaitIdle) d->DeviceWaitIdle(dev);
    while (d->swaps) { Swap *s = d->swaps; d->swaps = s->next; FreeSwap(d, s); }
    if (d->layout) d->DestroyPipelineLayout(dev, d->layout, NULL);
    if (d->dsl) d->DestroySetLayout(dev, d->dsl, NULL);
    if (d->sampler) d->DestroySampler(dev, d->sampler, NULL);
    if (d->vs) d->DestroyShaderModule(dev, d->vs, NULL);
    if (d->fs) d->DestroyShaderModule(dev, d->fs, NULL);
    if (d->fs2) d->DestroyShaderModule(dev, d->fs2, NULL);
    void (VKAPI *destroy)(VkDevice, const void *) = d->DestroyDevice;
    EnterCriticalSection(&g_lock); memset(d, 0, sizeof(*d)); LeaveCriticalSection(&g_lock);
    if (destroy) destroy(dev, alloc);
}
static void Remember(Dev *d, VkQueue q, uint32_t family)
{
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < d->nqueues; i++) if (d->queues[i].q == q) { LeaveCriticalSection(&g_lock); return; }
    if (d->nqueues < 16) { d->queues[d->nqueues].q = q; d->queues[d->nqueues].family = family; d->nqueues++; }
    LeaveCriticalSection(&g_lock);
}
static void VKAPI L_GetDeviceQueue(VkDevice dev, uint32_t family, uint32_t index, VkQueue *q)
{
    Dev *d = FindDev(dev); d->GetDeviceQueue(dev, family, index, q); if (*q) Remember(d, *q, family);
}
static void VKAPI L_GetDeviceQueue2(VkDevice dev, const VkDeviceQueueInfo2 *info, VkQueue *q)
{
    Dev *d = FindDev(dev); d->GetDeviceQueue2(dev, info, q); if (*q) Remember(d, *q, info->queueFamilyIndex);
}

/* ---------------------------------------------------------------- swapchains */
static int MemType(Dev *d, uint32_t bits, VkFlags want)
{
    for (uint32_t i = 0; i < d->mem.memoryTypeCount; i++) if ((bits & (1u << i)) && (d->mem.memoryTypes[i].propertyFlags & want) == want) return (int)i;
    for (uint32_t i = 0; i < d->mem.memoryTypeCount; i++) if (bits & (1u << i)) return (int)i;
    return -1;
}
static BOOL NewImage(Dev *d, VkFormat fmt, VkExtent2D e, VkFlags usage, VkImage *img, VkDeviceMemory *mem)
{
    VkDevice v = d->device;
    VkImageCreateInfo ic = {ST_IMAGE, NULL, 0, 1, fmt, {e.width, e.height, 1}, 1, 1, 1, 0, usage, 0, 0, NULL, LAYOUT_UNDEFINED};
    if (d->CreateImage(v, &ic, NULL, img)) { *img = 0; return FALSE; }
    VkMemoryRequirements mr; d->GetImageMemReq(v, *img, &mr);
    int t = MemType(d, mr.memoryTypeBits, 1 /* device local */); if (t < 0) return FALSE;
    VkMemoryAllocateInfo ma = {ST_MEMALLOC, NULL, mr.size, (uint32_t)t};
    if (d->AllocateMemory(v, &ma, NULL, mem)) { *mem = 0; return FALSE; }
    return d->BindImageMemory(v, *img, *mem, 0) == VK_SUCCESS;
}
static void FreeApp(Dev *d, Swap *s)
{
    for (uint32_t i = 0; i < MAXIMG; i++) {
        if (s->appView[i]) d->DestroyImageView(d->device, s->appView[i], NULL);
        if (s->app[i]) d->DestroyImage(d->device, s->app[i], NULL);
        if (s->appMem[i]) d->FreeMemory(d->device, s->appMem[i], NULL);
        s->appView[i] = 0; s->app[i] = 0; s->appMem[i] = 0;
    }
}
static VkResult VKAPI L_CreateSwapchain(VkDevice dev, const VkSwapchainCreateInfoKHR *ci, const void *alloc, VkSwapchainKHR *out)
{
    Dev *d = FindDev(dev);
    if (!d || d->broken || (g_scale >= 0.999f && !Sharpen())) return d->CreateSwapchain(dev, ci, alloc, out);
    /* upscale when the game followed the smaller window: the real window size from the surface itself */
    VkExtent2D real = ci->imageExtent; BOOL proxy = FALSE;
    if (g_scale < 0.999f && d->inst->GetSurfaceCaps && ci->imageArrayLayers == 1) {
        VkSurfaceCapabilitiesKHR caps; memset(&caps, 0, sizeof(caps));
        if (d->inst->GetSurfaceCaps(d->phys, ci->surface, &caps) == VK_SUCCESS && caps.currentExtent.width != 0xFFFFFFFFu) real = caps.currentExtent;
        proxy = real.width > ci->imageExtent.width + 8 && real.height > ci->imageExtent.height + 8;
        if (!proxy) Log("swapchain %ux%u: the game kept its window's size (%ux%u): sharpen only", ci->imageExtent.width, ci->imageExtent.height, real.width, real.height);
    }
    VkSwapchainCreateInfoKHR c = *ci;
    if (proxy) { c.imageExtent = real; c.imageUsage = USAGE_COLOR | (ci->imageUsage & USAGE_TRANSFER_DST); }
    else c.imageUsage |= USAGE_TRANSFER_SRC | USAGE_COLOR;   /* copied from, then drawn into */
    VkResult r = d->CreateSwapchain(dev, &c, alloc, out);
    BOOL usable = r == VK_SUCCESS;
    if (r != VK_SUCCESS) { r = d->CreateSwapchain(dev, ci, alloc, out); usable = FALSE; proxy = FALSE; }
    if (r != VK_SUCCESS) return r;
    Swap *s = (Swap *)calloc(1, sizeof(Swap));
    if (!s) {   /* no record: a real swapchain of another size than the game asked for cannot be handed over */
        if (proxy) { d->DestroySwapchain(dev, *out, alloc); return d->CreateSwapchain(dev, ci, alloc, out); }
        return r;
    }
    s->sc = *out; s->format = ci->imageFormat; s->extent = c.imageExtent; s->appExtent = ci->imageExtent; s->usable = usable && ci->imageArrayLayers == 1; s->proxy = proxy && s->usable;
    if (s->proxy) {   /* the game's own images, at the size it asked for, sampled by EASU */
        uint32_t n = 0; BOOL ok = d->GetSwapchainImages(dev, *out, &n, NULL) == VK_SUCCESS && n && n <= MAXIMG;
        for (uint32_t i = 0; ok && i < n; i++) ok = NewImage(d, ci->imageFormat, ci->imageExtent, ci->imageUsage | USAGE_SAMPLED, &s->app[i], &s->appMem[i]);
        if (ok) s->n = n;
        else {   /* back to a plain swapchain of the size the game asked for */
            Log("swapchain: the game's images could not be made: sharpen only");
            FreeApp(d, s); d->DestroySwapchain(dev, *out, alloc); s->proxy = FALSE; s->extent = ci->imageExtent;
            c = *ci; c.imageUsage |= USAGE_TRANSFER_SRC | USAGE_COLOR;
            r = d->CreateSwapchain(dev, &c, alloc, out); if (r != VK_SUCCESS) { r = d->CreateSwapchain(dev, ci, alloc, out); s->usable = FALSE; }
            if (r != VK_SUCCESS) { free(s); return r; }
            s->sc = *out;
        }
    }
    EnterCriticalSection(&g_lock); s->next = d->swaps; d->swaps = s; LeaveCriticalSection(&g_lock);
    Log("swapchain %llx: game %ux%u, screen %ux%u, format %u -> %s", (unsigned long long)*out, s->appExtent.width, s->appExtent.height, s->extent.width, s->extent.height, ci->imageFormat,
        !s->usable ? "untouched" : s->proxy ? (Sharpen() ? "FSR 1 upscale + sharpen" : "FSR 1 upscale") : "FSR 1 sharpen");
    return r;
}
static VkResult VKAPI L_GetSwapchainImages(VkDevice dev, VkSwapchainKHR sc, uint32_t *count, VkImage *imgs)
{
    Dev *d = FindDev(dev);
    EnterCriticalSection(&g_lock); Swap *s = d->swaps; while (s && s->sc != sc) s = s->next; LeaveCriticalSection(&g_lock);
    if (!s || !s->proxy) return d->GetSwapchainImages(dev, sc, count, imgs);
    if (!imgs) { *count = s->n; return VK_SUCCESS; }
    uint32_t k = *count < s->n ? *count : s->n; for (uint32_t i = 0; i < k; i++) imgs[i] = s->app[i];
    *count = k; return k < s->n ? 5 /* VK_INCOMPLETE */ : VK_SUCCESS;
}
static void FreeSwap(Dev *d, Swap *s)
{
    VkDevice v = d->device;
    if (s->built) {
        for (uint32_t i = 0; i < s->n; i++) if (s->fence[i]) d->WaitFences(v, 1, &s->fence[i], 1, 2000000000ull);   /* a failed setup leaves some unmade */
        for (uint32_t i = 0; i < s->n; i++) {
            if (s->fb[i]) d->DestroyFramebuffer(v, s->fb[i], NULL);
            if (s->view[i]) d->DestroyImageView(v, s->view[i], NULL);
            if (s->midFb[i]) d->DestroyFramebuffer(v, s->midFb[i], NULL);
            if (s->midView[i]) d->DestroyImageView(v, s->midView[i], NULL);
            if (s->mid[i]) d->DestroyImage(v, s->mid[i], NULL);
            if (s->midMem[i]) d->FreeMemory(v, s->midMem[i], NULL);
            if (s->copyView[i]) d->DestroyImageView(v, s->copyView[i], NULL);
            if (s->copy[i]) d->DestroyImage(v, s->copy[i], NULL);
            if (s->copyMem[i]) d->FreeMemory(v, s->copyMem[i], NULL);
            if (s->fence[i]) d->DestroyFence(v, s->fence[i], NULL);
            if (s->done[i]) d->DestroySemaphore(v, s->done[i], NULL);
        }
        if (s->cmdPool) d->DestroyCmdPool(v, s->cmdPool, NULL);
        if (s->pool) d->DestroyDescPool(v, s->pool, NULL);
        if (s->pipeE) d->DestroyPipeline(v, s->pipeE, NULL);
        if (s->pipeR) d->DestroyPipeline(v, s->pipeR, NULL);
        if (s->rp) d->DestroyRenderPass(v, s->rp, NULL);
        if (s->rpMid) d->DestroyRenderPass(v, s->rpMid, NULL);
    } else if (s->proxy) for (uint32_t i = 0; i < s->n; i++) if (s->app[i]) {}   /* nothing in flight yet */
    FreeApp(d, s);
    free(s);
}
static void VKAPI L_DestroySwapchain(VkDevice dev, VkSwapchainKHR sc, const void *alloc)
{
    Dev *d = FindDev(dev);
    EnterCriticalSection(&g_lock);
    Swap **p = &d->swaps, *s = NULL; while (*p) { if ((*p)->sc == sc) { s = *p; *p = s->next; break; } p = &(*p)->next; }
    LeaveCriticalSection(&g_lock);
    if (s) { if (s->proxy && !s->built && d->DeviceWaitIdle) d->DeviceWaitIdle(dev); FreeSwap(d, s); }
    d->DestroySwapchain(dev, sc, alloc);
}

/* ---------------------------------------------------------------- the FSR 1 passes */
static BOOL DeviceObjects(Dev *d)
{
    if (d->layout) return TRUE;
    VkDevice v = d->device;
    VkShaderModuleCreateInfo m = {ST_SHADER, NULL, 0, sizeof(TRIANGLE_VS), TRIANGLE_VS};
    if (d->CreateShaderModule(v, &m, NULL, &d->vs)) return FALSE;
    m.codeSize = sizeof(FSR_EASU); m.pCode = FSR_EASU;
    if (d->CreateShaderModule(v, &m, NULL, &d->fs)) return FALSE;
    m.codeSize = sizeof(FSR_RCAS); m.pCode = FSR_RCAS;
    if (d->CreateShaderModule(v, &m, NULL, &d->fs2)) return FALSE;
    VkSamplerCreateInfo sc = {ST_SAMPLER, NULL, 0, 1, 1, 0, 2, 2, 2, 0.0f, 0, 1.0f, 0, 0, 0.0f, 0.0f, 0, 0};   /* linear, clamp to edge */
    if (d->CreateSampler(v, &sc, NULL, &d->sampler)) return FALSE;
    VkDescriptorSetLayoutBinding b = {0, 1 /* combined image sampler */, 1, 0x10 /* fragment */, NULL};
    VkDescriptorSetLayoutCreateInfo lc = {ST_SETLAYOUT, NULL, 0, 1, &b};
    if (d->CreateSetLayout(v, &lc, NULL, &d->dsl)) return FALSE;
    VkPushConstantRange pr = {0x10, 0, 24};   /* vec2 inSize, vec2 outSize, float sharp, float pad */
    VkPipelineLayoutCreateInfo pl = {ST_PIPELAYOUT, NULL, 0, 1, &d->dsl, 1, &pr};
    return d->CreatePipelineLayout(v, &pl, NULL, &d->layout) == VK_SUCCESS;
}
static BOOL Pipe(Dev *d, Swap *s, VkShaderModule fs, VkPipeline *out)
{
    VkPipelineShaderStageCreateInfo st[2] = {{ST_STAGE, NULL, 0, 0x1, d->vs, "main", NULL}, {ST_STAGE, NULL, 0, 0x10, fs, "main", NULL}};
    VkPipelineVertexInputStateCreateInfo vi = {ST_VERTEXINPUT, NULL, 0, 0, NULL, 0, NULL};
    VkPipelineInputAssemblyStateCreateInfo ia = {ST_ASSEMBLY, NULL, 0, 3 /* triangle list */, 0};
    VkPipelineViewportStateCreateInfo vp = {ST_VIEWPORT, NULL, 0, 1, NULL, 1, NULL};
    VkPipelineRasterizationStateCreateInfo rs = {ST_RASTER, NULL, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f};
    VkPipelineMultisampleStateCreateInfo ms = {ST_MULTISAMPLE, NULL, 0, 1, 0, 0.0f, NULL, 0, 0};
    VkPipelineColorBlendAttachmentState ba = {0, 0, 0, 0, 0, 0, 0, 0xF};
    VkPipelineColorBlendStateCreateInfo cb = {ST_BLEND, NULL, 0, 0, 0, 1, &ba, {0, 0, 0, 0}};
    uint32_t dyn[2] = {0 /* viewport */, 1 /* scissor */};
    VkPipelineDynamicStateCreateInfo ds = {ST_DYNAMIC, NULL, 0, 2, dyn};
    VkGraphicsPipelineCreateInfo gp = {ST_GRAPHICSPIPE, NULL, 0, 2, st, &vi, &ia, NULL, &vp, &rs, &ms, NULL, &cb, &ds, d->layout, s->rp, 0, 0, -1};
    return d->CreateGraphicsPipelines(d->device, 0, 1, &gp, NULL, out) == VK_SUCCESS;
}
static BOOL MakeSet(Dev *d, Swap *s, VkImageView view, VkDescriptorSet *set)
{
    VkDescriptorSetAllocateInfo da = {ST_DESCALLOC, NULL, s->pool, 1, &d->dsl};
    if (d->AllocDescSets(d->device, &da, set)) return FALSE;
    VkDescriptorImageInfo ii = {d->sampler, view, LAYOUT_SHADER_READ};
    VkWriteDescriptorSet w = {ST_WRITEDESC, NULL, *set, 0, 0, 1, 1, &ii, NULL, NULL};
    d->UpdateDescSets(d->device, 1, &w, 0, NULL); return TRUE;
}
static BOOL BuildSwap(Dev *d, Swap *s, uint32_t family)
{
    VkDevice v = d->device; s->built = TRUE; s->family = family;
    uint32_t n = 0; if (d->GetSwapchainImages(v, s->sc, &n, NULL) || n == 0 || n > MAXIMG) return FALSE;
    if (d->GetSwapchainImages(v, s->sc, &n, s->img)) return FALSE;
    if (s->proxy && n != s->n) return FALSE;
    s->n = n;
    BOOL mid = s->proxy && Sharpen();
    VkAttachmentDescription at = {0, s->format, 1, 2 /* don't care: every pixel is drawn */, 0 /* store */, 2, 1, LAYOUT_COLOR, LAYOUT_PRESENT};
    VkAttachmentReference ref = {0, LAYOUT_COLOR};
    VkSubpassDescription sp = {0, 0, 0, NULL, 1, &ref, NULL, NULL, 0, NULL};
    VkRenderPassCreateInfo rc = {ST_RENDERPASS, NULL, 0, 1, &at, 1, &sp, 0, NULL};
    if (d->CreateRenderPass(v, &rc, NULL, &s->rp)) return FALSE;
    if (mid) { at.finalLayout = LAYOUT_SHADER_READ; if (d->CreateRenderPass(v, &rc, NULL, &s->rpMid)) return FALSE; }
    if (s->proxy && !Pipe(d, s, d->fs, &s->pipeE)) return FALSE;
    if (Sharpen() && !Pipe(d, s, d->fs2, &s->pipeR)) return FALSE;
    VkDescriptorPoolSize ps = {1, 2 * n};
    VkDescriptorPoolCreateInfo pc = {ST_DESCPOOL, NULL, 0, 2 * n, 1, &ps};
    if (d->CreateDescPool(v, &pc, NULL, &s->pool)) return FALSE;
    VkCommandPoolCreateInfo cp = {ST_CMDPOOL, NULL, 0x2 /* reset each buffer */, family};
    if (d->CreateCmdPool(v, &cp, NULL, &s->cmdPool)) return FALSE;
    VkCommandBufferAllocateInfo ca = {ST_CMDALLOC, NULL, s->cmdPool, 0, n};
    if (d->AllocCmdBuffers(v, &ca, s->cmd)) return FALSE;
    for (uint32_t i = 0; i < n; i++) if (d->setData(v, s->cmd[i])) return FALSE;   /* the loader's dispatch for buffers made below it */
    for (uint32_t i = 0; i < n; i++) {
        VkImageViewCreateInfo vc = {ST_IMAGEVIEW, NULL, 0, s->img[i], 1, s->format, {0, 0, 0, 0}, {1, 0, 1, 0, 1}};
        if (d->CreateImageView(v, &vc, NULL, &s->view[i])) return FALSE;
        VkFramebufferCreateInfo fc = {ST_FRAMEBUFFER, NULL, 0, s->rp, 1, &s->view[i], s->extent.width, s->extent.height, 1};
        if (d->CreateFramebuffer(v, &fc, NULL, &s->fb[i])) return FALSE;
        if (s->proxy) {
            vc.image = s->app[i]; if (d->CreateImageView(v, &vc, NULL, &s->appView[i]) || !MakeSet(d, s, s->appView[i], &s->setE[i])) return FALSE;
            if (mid) {
                if (!NewImage(d, s->format, s->extent, USAGE_COLOR | USAGE_SAMPLED, &s->mid[i], &s->midMem[i])) return FALSE;
                vc.image = s->mid[i]; if (d->CreateImageView(v, &vc, NULL, &s->midView[i])) return FALSE;
                VkFramebufferCreateInfo fm = {ST_FRAMEBUFFER, NULL, 0, s->rpMid, 1, &s->midView[i], s->extent.width, s->extent.height, 1};
                if (d->CreateFramebuffer(v, &fm, NULL, &s->midFb[i]) || !MakeSet(d, s, s->midView[i], &s->setR[i])) return FALSE;
            }
        } else {
            if (!NewImage(d, s->format, s->extent, USAGE_TRANSFER_DST | USAGE_SAMPLED, &s->copy[i], &s->copyMem[i])) return FALSE;
            vc.image = s->copy[i]; if (d->CreateImageView(v, &vc, NULL, &s->copyView[i]) || !MakeSet(d, s, s->copyView[i], &s->setR[i])) return FALSE;
        }
        VkSimpleCreateInfo fci = {ST_FENCE, NULL, 0x1 /* signaled */}, sci = {ST_SEMAPHORE, NULL, 0};
        if (d->CreateFence(v, &fci, NULL, &s->fence[i]) || d->CreateSemaphore(v, &sci, NULL, &s->done[i])) return FALSE;
    }
    Log("swapchain %llx: %u images ready for FSR 1 on queue family %u", (unsigned long long)s->sc, n, family);
    return TRUE;
}
static void Draw(Dev *d, Swap *s, VkCommandBuffer c, VkRenderPass rp, VkFramebuffer fb, VkPipeline pipe, VkDescriptorSet set, VkExtent2D in)
{
    VkRenderPassBeginInfo rb = {ST_RPBEGIN, NULL, rp, fb, {{0, 0}, s->extent}, 0, NULL};
    d->CmdBeginRP(c, &rb, 0);
    d->CmdBindPipeline(c, 0, pipe);
    VkViewport vw = {0.0f, 0.0f, (float)s->extent.width, (float)s->extent.height, 0.0f, 1.0f};
    VkRect2D sc = {{0, 0}, s->extent};
    d->CmdViewport(c, 0, 1, &vw); d->CmdScissor(c, 0, 1, &sc);
    d->CmdBindSets(c, 0, d->layout, 0, 1, &set, 0, NULL);
    struct { float in[2], out[2], sharp, pad; } pc = {{(float)in.width, (float)in.height}, {(float)s->extent.width, (float)s->extent.height}, g_sharp, 0.0f};
    d->CmdPush(c, d->layout, 0x10, 0, sizeof(pc), &pc);
    d->CmdDraw(c, 3, 1, 0, 0);
    d->CmdEndRP(c);
}
/* records and submits the passes for one presented image; returns the semaphore the present has to wait for (0: none) */
static VkSemaphore FsrPass(Dev *d, Swap *s, VkQueue q, uint32_t idx, const VkPresentInfoKHR *pi)
{
    VkDevice v = d->device; if (idx >= s->n) return 0;
    if (d->WaitFences(v, 1, &s->fence[idx], 1, 1000000000ull)) return 0;
    d->ResetFences(v, 1, &s->fence[idx]);
    VkCommandBuffer c = s->cmd[idx];
    VkCommandBufferBeginInfo bi = {ST_CMDBEGIN, NULL, 0x1, NULL};
    if (d->BeginCmd(c, &bi)) return 0;
    VkImageSubresourceRange rng = {1, 0, 1, 0, 1};
    if (s->proxy) {
        BOOL mid = Sharpen();
        VkImageMemoryBarrier b1[3] = {
            {ST_IMGBARRIER, NULL, ACCESS_MEMORY_WRITE, ACCESS_SHADER_READ, LAYOUT_PRESENT, LAYOUT_SHADER_READ, QUEUE_IGNORED, QUEUE_IGNORED, s->app[idx], rng},
            {ST_IMGBARRIER, NULL, 0, ACCESS_COLOR_WRITE, LAYOUT_UNDEFINED, LAYOUT_COLOR, QUEUE_IGNORED, QUEUE_IGNORED, s->img[idx], rng},
            {ST_IMGBARRIER, NULL, 0, ACCESS_COLOR_WRITE, LAYOUT_UNDEFINED, LAYOUT_COLOR, QUEUE_IGNORED, QUEUE_IGNORED, mid ? s->mid[idx] : s->img[idx], rng}};
        d->CmdBarrier(c, STAGE_ALL, STAGE_FRAGMENT | STAGE_COLOR_OUT, 0, 0, NULL, 0, NULL, mid ? 3 : 2, b1);
        if (mid) {
            Draw(d, s, c, s->rpMid, s->midFb[idx], s->pipeE, s->setE[idx], s->appExtent);
            VkImageMemoryBarrier bm = {ST_IMGBARRIER, NULL, ACCESS_COLOR_WRITE, ACCESS_SHADER_READ, LAYOUT_SHADER_READ, LAYOUT_SHADER_READ, QUEUE_IGNORED, QUEUE_IGNORED, s->mid[idx], rng};
            d->CmdBarrier(c, STAGE_COLOR_OUT, STAGE_FRAGMENT, 0, 0, NULL, 0, NULL, 1, &bm);
            Draw(d, s, c, s->rp, s->fb[idx], s->pipeR, s->setR[idx], s->extent);
        } else Draw(d, s, c, s->rp, s->fb[idx], s->pipeE, s->setE[idx], s->appExtent);
        /* the game's image goes back to the layout it presented it in */
        VkImageMemoryBarrier b3 = {ST_IMGBARRIER, NULL, ACCESS_SHADER_READ, 0, LAYOUT_SHADER_READ, LAYOUT_PRESENT, QUEUE_IGNORED, QUEUE_IGNORED, s->app[idx], rng};
        d->CmdBarrier(c, STAGE_FRAGMENT, STAGE_ALL, 0, 0, NULL, 0, NULL, 1, &b3);
    } else {
        VkImageMemoryBarrier b1[2] = {
            {ST_IMGBARRIER, NULL, ACCESS_MEMORY_WRITE, ACCESS_TRANSFER_READ, LAYOUT_PRESENT, LAYOUT_TRANSFER_SRC, QUEUE_IGNORED, QUEUE_IGNORED, s->img[idx], rng},
            {ST_IMGBARRIER, NULL, 0, ACCESS_TRANSFER_WRITE, LAYOUT_UNDEFINED, LAYOUT_TRANSFER_DST, QUEUE_IGNORED, QUEUE_IGNORED, s->copy[idx], rng}};
        d->CmdBarrier(c, STAGE_ALL, STAGE_TRANSFER, 0, 0, NULL, 0, NULL, 2, b1);
        VkImageCopy cp = {{1, 0, 0, 1}, {0, 0, 0}, {1, 0, 0, 1}, {0, 0, 0}, {s->extent.width, s->extent.height, 1}};
        d->CmdCopyImage(c, s->img[idx], LAYOUT_TRANSFER_SRC, s->copy[idx], LAYOUT_TRANSFER_DST, 1, &cp);
        VkImageMemoryBarrier b2[2] = {
            {ST_IMGBARRIER, NULL, ACCESS_TRANSFER_WRITE, ACCESS_SHADER_READ, LAYOUT_TRANSFER_DST, LAYOUT_SHADER_READ, QUEUE_IGNORED, QUEUE_IGNORED, s->copy[idx], rng},
            {ST_IMGBARRIER, NULL, ACCESS_TRANSFER_READ, ACCESS_COLOR_WRITE, LAYOUT_TRANSFER_SRC, LAYOUT_COLOR, QUEUE_IGNORED, QUEUE_IGNORED, s->img[idx], rng}};
        d->CmdBarrier(c, STAGE_TRANSFER, STAGE_FRAGMENT | STAGE_COLOR_OUT, 0, 0, NULL, 0, NULL, 2, b2);
        Draw(d, s, c, s->rp, s->fb[idx], s->pipeR, s->setR[idx], s->extent);
    }
    if (d->EndCmd(c)) return 0;
    VkFlags stages[16]; uint32_t nw = pi->waitSemaphoreCount;   /* at most 16 (L_QueuePresent) */
    for (uint32_t i = 0; i < nw; i++) stages[i] = STAGE_ALL;
    VkSubmitInfo si = {ST_SUBMIT, NULL, nw, pi->pWaitSemaphores, stages, 1, &c, 1, &s->done[idx]};
    if (d->QueueSubmit(q, 1, &si, s->fence[idx])) return 0;
    return s->done[idx];
}
static VkResult VKAPI L_QueuePresent(VkQueue q, const VkPresentInfoKHR *pi)
{
    Dev *d = FindDev(q);
    if (!d) return -4;   /* VK_ERROR_DEVICE_LOST: a queue of a device this layer never saw */
    if (d->broken || pi->swapchainCount != 1 || pi->waitSemaphoreCount > 16) return d->QueuePresent(q, pi);
    uint32_t family = 0xFFFFFFFFu;
    EnterCriticalSection(&g_lock); for (int i = 0; i < d->nqueues; i++) if (d->queues[i].q == q) family = d->queues[i].family;
    Swap *s = d->swaps; while (s && s->sc != pi->pSwapchains[0]) s = s->next; LeaveCriticalSection(&g_lock);
    if (!s || !s->usable || family == 0xFFFFFFFFu || !(d->graphicsFamilies & (1u << family))) return d->QueuePresent(q, pi);
    if (!s->built) {
        if (!DeviceObjects(d) || !BuildSwap(d, s, family)) {
            Log("swapchain %llx: setup failed, FSR off for it", (unsigned long long)s->sc);
            if (s->proxy) { d->broken = TRUE; return -4; }   /* the game's frame is not in the real image: nothing right to show */
            s->usable = FALSE; return d->QueuePresent(q, pi);
        }
    }
    if (s->family != family) return s->proxy ? -4 : d->QueuePresent(q, pi);
    VkSemaphore done = FsrPass(d, s, q, pi->pImageIndices[0], pi);
    if (!done) { Log("present: the FSR pass failed"); if (!s->proxy) { d->broken = TRUE; return d->QueuePresent(q, pi); } return -4; }
    VkPresentInfoKHR p = *pi; p.waitSemaphoreCount = 1; p.pWaitSemaphores = &done;
    return d->QueuePresent(q, &p);
}

static VkResult VKAPI L_CreateSampler(VkDevice dev, const VkSamplerCreateInfo *ci, const void *alloc, VkSampler *out)
{
    Dev *d = FindDev(dev); if (!d || !d->CreateSampler) return VK_ERROR_INITIALIZATION_FAILED;
    VkSamplerCreateInfo s = *ci; LONG all = InterlockedIncrement(&g_smpAll);
    if (g_texOn && !s.pNext && !s.compareEnable && !s.unnormalizedCoordinates && s.minFilter == 1 && s.magFilter == 1) {
        if (g_texAniso > 1) { s.anisotropyEnable = 1; s.maxAnisotropy = (float)g_texAniso; }
        s.mipLodBias += g_texBias; InterlockedIncrement(&g_smpMod);
    }
    if (all == 1 || all == 16 || all == 256 || all == 4096) Log("textures: %ld samplers so far, %ld changed (anisotropy %d, bias %.2f)", all, g_smpMod, g_texAniso, g_texBias);
    return d->CreateSampler(dev, &s, alloc, out);
}
/* ---------------------------------------------------------------- the loader interface */
static PFN_vkVoidFunction VKAPI L_GetDeviceProcAddr(VkDevice dev, const char *name);
static PFN_vkVoidFunction Mine(const char *name)
{
    static const struct { const char *n; PFN_vkVoidFunction f; } T[] = {
        {"vkCreateInstance", (PFN_vkVoidFunction)L_CreateInstance}, {"vkDestroyInstance", (PFN_vkVoidFunction)L_DestroyInstance},
        {"vkCreateDevice", (PFN_vkVoidFunction)L_CreateDevice}, {"vkDestroyDevice", (PFN_vkVoidFunction)L_DestroyDevice},
        {"vkGetDeviceProcAddr", (PFN_vkVoidFunction)L_GetDeviceProcAddr}, {"vkGetDeviceQueue", (PFN_vkVoidFunction)L_GetDeviceQueue},
        {"vkGetDeviceQueue2", (PFN_vkVoidFunction)L_GetDeviceQueue2}, {"vkCreateSwapchainKHR", (PFN_vkVoidFunction)L_CreateSwapchain},
        {"vkDestroySwapchainKHR", (PFN_vkVoidFunction)L_DestroySwapchain}, {"vkQueuePresentKHR", (PFN_vkVoidFunction)L_QueuePresent},
        {"vkGetSwapchainImagesKHR", (PFN_vkVoidFunction)L_GetSwapchainImages},
        {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", (PFN_vkVoidFunction)L_GetSurfaceCaps}, {"vkGetPhysicalDeviceSurfaceCapabilities2KHR", (PFN_vkVoidFunction)L_GetSurfaceCaps2}};
    if (g_texOn && !strcmp(name, "vkCreateSampler")) return (PFN_vkVoidFunction)L_CreateSampler;
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++) if (!strcmp(name, T[i].n)) {
        if (!g_fsrOn && i > 3) return NULL;   /* 6.2: textures only: the swapchain is not this layer's */
        return T[i].f;
    }
    return NULL;
}
static PFN_vkVoidFunction VKAPI L_GetDeviceProcAddr(VkDevice dev, const char *name)
{
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)L_GetDeviceProcAddr;
    Dev *d = dev ? FindDev(dev) : NULL;
    PFN_vkVoidFunction f = Mine(name);
    if (f && strcmp(name, "vkCreateInstance") && strcmp(name, "vkDestroyInstance") && strcmp(name, "vkCreateDevice") && strncmp(name, "vkGetPhysicalDevice", 19)) {
        if (d && !strcmp(name, "vkGetDeviceQueue2") && !d->GetDeviceQueue2) return NULL;
        return f;
    }
    return d ? d->gdpa(dev, name) : NULL;
}
static PFN_vkVoidFunction VKAPI L_GetInstanceProcAddr(VkInstance inst, const char *name)
{
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)L_GetInstanceProcAddr;
    Inst *s = inst ? FindInst(inst) : NULL;
    if (!strcmp(name, "vkGetPhysicalDeviceSurfaceCapabilities2KHR") && (!s || !s->GetSurfaceCaps2)) return s ? s->gipa(inst, name) : NULL;
    PFN_vkVoidFunction f = Mine(name); if (f) return f;
    return s ? s->gipa(inst, name) : NULL;
}
__declspec(dllexport) VkResult VKAPI vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v)
{
    if (!v || v->sType != 1) return VK_ERROR_INITIALIZATION_FAILED;
    if (v->loaderLayerInterfaceVersion > 2) v->loaderLayerInterfaceVersion = 2;
    v->pfnGetInstanceProcAddr = L_GetInstanceProcAddr; v->pfnGetDeviceProcAddr = L_GetDeviceProcAddr; v->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
__declspec(dllexport) PFN_vkVoidFunction VKAPI vkGetInstanceProcAddr(VkInstance inst, const char *name) { return L_GetInstanceProcAddr(inst, name); }
__declspec(dllexport) PFN_vkVoidFunction VKAPI vkGetDeviceProcAddr(VkDevice dev, const char *name) { return L_GetDeviceProcAddr(dev, name); }
BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID r)
{
    (void)r;
    if (why == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&g_lock);
        char lv[32];
        if (GetEnvironmentVariableA("PORTHEON_TEX_ANISO", lv, 32)) { g_texAniso = atoi(lv); if (g_texAniso > 16) g_texAniso = 16; g_texOn = TRUE; }
        if (GetEnvironmentVariableA("PORTHEON_TEX_BIAS", lv, 32)) { g_texBias = (float)atof(lv); if (g_texBias < -1.0f) g_texBias = -1.0f; if (g_texBias > 2.0f) g_texBias = 2.0f; g_texOn = TRUE; }
        if (GetEnvironmentVariableA("PORTHEON_FSR_SCALE", lv, 32)) { g_fsrOn = TRUE; g_scale = (float)atof(lv); if (!(g_scale >= 0.5f && g_scale <= 1.0f)) g_scale = 1.0f; }
        if (GetEnvironmentVariableA("PORTHEON_FSR_SHARP", lv, 32)) { g_sharp = (float)atof(lv); if (g_sharp > 2.0f) g_sharp = 2.0f; }
        char p[MAX_PATH]; if (GetEnvironmentVariableA("PORTHEON_FSR_LOG", p, MAX_PATH)) g_log = fopen(p, "w");
        Log("PortheonFSR loaded: FSR %s, scale %.3f, sharpness %.2f%s; textures %s (anisotropy %d, bias %.2f)", g_fsrOn ? "on" : "off", g_scale, g_sharp, g_sharp < 0 ? " (no RCAS)" : "", g_texOn ? "on" : "off", g_texAniso, g_texBias);
    }
    (void)h; return TRUE;
}
