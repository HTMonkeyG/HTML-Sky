// ----------------------------------------------------------------------------
// Vulkan layer implementation of HT's Mod Loader.
// Referring to the implementation of SML-PC.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>
#include "vulkan/vulkan.h"
#include "vulkan/vk_layer.h"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>

#include "aliases.h"
#include "globals.h"
#include "ui/gui.h"

// ----------------------------------------------------------------------------
// [SECTION] Type declarations.
// ----------------------------------------------------------------------------

// Local structure, only used for traversing linked lists.
struct VkLayerCreateInfo_ {
  VkStructureType sType;
  const void *pNext;
  VkLayerFunction function;
};

// Dispatch table for VkInstance.
struct InstanceDispatchTable {
  PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
  PFN_vkDestroyInstance DestroyInstance;
  PFN_vkCreateDevice CreateDevice;
};

// Dispatch table for VkDevice.
struct DeviceDispatchTable {
  PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
  PFN_vkDestroyDevice DestroyDevice;
  PFN_vkQueuePresentKHR QueuePresentKHR;
  PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
  PFN_vkGetDeviceQueue GetDeviceQueue;
};

struct QueueData;
// VkDevice related data.
struct DeviceData {
  DeviceDispatchTable deviceTable;
  PFN_vkSetDeviceLoaderData vkSetDeviceLoaderData;
  VkDevice device;
  QueueData *graphicQueue;
  std::vector<QueueData *> queues;
};

// VkQueue related data.
struct QueueData {
  DeviceData *device;
  VkQueue queue;
};

// ImGui related data.
struct GuiStatus {
  i32 isInited;
  VkDevice device;
  VkDevice fakeDevice;
  VkAllocationCallbacks *allocator;
  VkInstance instance;
  VkPhysicalDevice physicalDevice;
  VkDescriptorPool descriptorPool;
  VkRenderPass renderPass;
  VkExtent2D imageExtent;
  std::vector<VkQueueFamilyProperties> queueProperties;
  u32 queueFamily;
  u32 minImageCount;
  ImGui_ImplVulkanH_Frame frames[8];
  ImGui_ImplVulkanH_FrameSemaphores frameSemaphores[8];
};

// ----------------------------------------------------------------------------
// [SECTION] Variable declarations.
// ----------------------------------------------------------------------------

// Saved instance dispatch tables.
static std::unordered_map<VkInstance, InstanceDispatchTable> gInstanceTables;
// Saved device data objects.
static std::unordered_map<VkDevice, DeviceData> gDeviceData;
// Saved queue data objects.
static std::unordered_map<VkQueue, QueueData> gQueueData;
// Mutex.
static std::mutex gMutex;
// ImGui related data.
static GuiStatus gGuiStatus = {0};

// ----------------------------------------------------------------------------
// [SECTION] Local helper functions.
// ----------------------------------------------------------------------------

/**
 * Get associated dispatch table with given VkInstance object.
 */
static InstanceDispatchTable *getInstanceDispatchTable(VkInstance instance) {
  std::lock_guard<std::mutex> lock(gMutex);
  auto it = gInstanceTables.find(instance);
  if (it == gInstanceTables.end())
    return nullptr;
  return &it->second;
}

/**
 * Get associated dispatch table with given VkDevice object.
 */
static DeviceDispatchTable *getDeviceDispatchTable(VkDevice device) {
  std::lock_guard<std::mutex> lock(gMutex);
  auto it = gDeviceData.find(device);
  if (it == gDeviceData.end())
    return nullptr;
  return &it->second.deviceTable;
}

/**
 * Get associated DeviceData object with given VkDevice object.
 */
static DeviceData *getDeviceData(VkDevice device) {
  std::lock_guard<std::mutex> lock(gMutex);
  return &gDeviceData[device];
}

/**
 * Get associated QueueData object with given VkQueue object.
 */
static QueueData *getQueueData(VkQueue queue) {
  std::lock_guard<std::mutex> lock(gMutex);
  return &gQueueData[queue];
}

/**
 * Modified from SML-PC.
 * 
 * Create a QueueData object associated with given VkQueue and DeviceData.
 */
static QueueData *createQueueData(
  VkQueue queue,
  DeviceData *deviceData
) {
  QueueData *queueData = getQueueData(queue);
  queueData->device = deviceData;
  queueData->queue = queue;
  deviceData->graphicQueue = queueData;
  return queueData;
}

/**
 * Modified from SML-PC.
 * 
 * Link VkDevice and VkQueue structure.
 */
static void setDeviceDataQueues(
  VkDevice device,
  DeviceData *data,
  const VkDeviceCreateInfo *pCreateInfo
) {
  for (u32 i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
    for (u32 j = 0; j < pCreateInfo->pQueueCreateInfos[i].queueCount; j++) {
      VkQueue queue;
      data->deviceTable.GetDeviceQueue(
        device,
        pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex,
        j,
        &queue);
      data->vkSetDeviceLoaderData(device, queue);
      data->queues.push_back(createQueueData(queue, data));
    }
  }
}

/**
 * Modified from SML-PC.
 * 
 * Walk through the chain list to get the target structure.
 */
static VkLayerCreateInfo_ *getChainInfo(
  const VkLayerCreateInfo_ *pCreateInfo,
  VkStructureType sType,
  VkLayerFunction func
) {
  VkLayerCreateInfo_ *e = (VkLayerCreateInfo_ *)pCreateInfo->pNext;
  for (; e; e = (VkLayerCreateInfo_ *)e->pNext)
    if (e->sType == sType && e->function == func)
      return e;
  return nullptr;
}

// ----------------------------------------------------------------------------
// [SECTION] Local Vulkan initialize functions.
// ----------------------------------------------------------------------------

/**
 * Modified from SML-PC.
 * 
 * Check if the queue is a graphic queue.
 */
static i32 isGraphicQueue(VkQueue queue, VkQueue *pGraphicQueue) {
  for (uint32_t i = 0; i < gGuiStatus.queueProperties.size(); ++i) {
    VkQueueFamilyProperties &family = gGuiStatus.queueProperties[i];
    for (uint32_t j = 0; j < family.queueCount; ++j) {
      VkQueue q = VK_NULL_HANDLE;
      vkGetDeviceQueue(gGuiStatus.device, i, j, &q);

      if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        if (pGraphicQueue && *pGraphicQueue == VK_NULL_HANDLE)
          *pGraphicQueue = q;
        if (queue == q)
          return 1;
      }
    }
  }

  return 0;
}

/**
 * Modified from ImGui_ImplVulkanH_SelectQueueFamilyIndex().
 * 
 * Select graphics queue family and store queue properties.
 */
u32 selectQueueFamilyIndex(VkPhysicalDevice physical_device) {
  u32 count;

  vkGetPhysicalDeviceQueueFamilyProperties(
    physical_device,
    &count,
    nullptr);
  gGuiStatus.queueProperties.resize((int)count);
  vkGetPhysicalDeviceQueueFamilyProperties(
    physical_device,
    &count,
    gGuiStatus.queueProperties.data());
  for (u32 i = 0; i < count; i++)
    if (gGuiStatus.queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      return i;
  return (u32)-1;
}

/**
 * Initialize Vulkan objects for ImGui.
 */
static void initVulkan() {
  ImVector<const char*> extensions;
  GuiStatus *g = &gGuiStatus;

  extensions.push_back("VK_KHR_surface");
  extensions.push_back("VK_KHR_win32_surface");

  {
    // Create Vulkan Instance.
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.enabledExtensionCount = (u32)extensions.Size;
    createInfo.ppEnabledExtensionNames = extensions.Data;
    vkCreateInstance(&createInfo, g->allocator, &g->instance);
  }

  // Select Physical Device (GPU).
  g->physicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g->instance);
  //IM_ASSERT(gGuiStatus.physicalDevice != VK_NULL_HANDLE);

  // Select graphics queue family.
  g->queueFamily = selectQueueFamilyIndex(g->physicalDevice);
  //IM_ASSERT(gGuiStatus.queueFamily != (u32)-1);

  // Create Logical Device (with 1 queue)
  {
    ImVector<const char*> deviceExtensions;
    deviceExtensions.push_back("VK_KHR_swapchain");

    // Enumerate physical device extension.
    u32 propertiesCount;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateDeviceExtensionProperties(g->physicalDevice, nullptr, &propertiesCount, nullptr);
    properties.resize(propertiesCount);
    vkEnumerateDeviceExtensionProperties(g->physicalDevice, nullptr, &propertiesCount, properties.Data);

    const float queuePriority[] = { 1.0f };
    VkDeviceQueueCreateInfo queueInfo[1] = {};
    queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo[0].queueFamilyIndex = gGuiStatus.queueFamily;
    queueInfo[0].queueCount = 1;
    queueInfo[0].pQueuePriorities = queuePriority;
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = sizeof(queueInfo) / sizeof(queueInfo[0]);
    createInfo.pQueueCreateInfos = queueInfo;
    createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.Size;
    createInfo.ppEnabledExtensionNames = deviceExtensions.Data;
    vkCreateDevice(g->physicalDevice, &createInfo, g->allocator, &g->fakeDevice);
  }
}

/**
 * Create Vulkan render target for ImGui.
 */
static void createRenderTargetVk(
  VkDevice device,
  VkSwapchainKHR swapchain
) {
  u32 imageCount;
  VkImage images[8] = {0};
  GuiStatus *g = &gGuiStatus;

  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images);

  g->minImageCount = imageCount;
  for (u32 i = 0; i < imageCount; i++) {
    g->frames[i].Backbuffer = images[i];

    ImGui_ImplVulkanH_Frame *fd = (ImGui_ImplVulkanH_Frame *)&g->frames[i];
    ImGui_ImplVulkanH_FrameSemaphores *fsd = (ImGui_ImplVulkanH_FrameSemaphores *)&g->frameSemaphores[i];
    {
      VkCommandPoolCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      info.queueFamilyIndex = g->queueFamily;
      vkCreateCommandPool(device, &info, g->allocator, &fd->CommandPool);
    }
    {
      VkCommandBufferAllocateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      info.commandPool = fd->CommandPool;
      info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      info.commandBufferCount = 1;
      vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
    }
    {
      VkFenceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      vkCreateFence(device, &info, g->allocator, &fd->Fence);
    }
    {
      VkSemaphoreCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      vkCreateSemaphore(device, &info, g->allocator, &fsd->ImageAcquiredSemaphore);
      vkCreateSemaphore(device, &info, g->allocator, &fsd->RenderCompleteSemaphore);
    }
  }

  // Create the render pass.
  {
    VkAttachmentDescription attachment = {};
    attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;

    vkCreateRenderPass(device, &info, g->allocator, &g->renderPass);
  }

  // Create image views.
  {
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = VK_FORMAT_B8G8R8A8_UNORM;

    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    for (u32 i = 0; i < imageCount; i++) {
      ImGui_ImplVulkanH_Frame *fd = (ImGui_ImplVulkanH_Frame *)&g->frames[i];
      info.image = fd->Backbuffer;
      vkCreateImageView(device, &info, g->allocator, &fd->BackbufferView);
    }
  }

  // Create frame buffers.
  {
    VkImageView attachment[1];
    VkFramebufferCreateInfo info = { };
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = g->renderPass;
    info.attachmentCount = 1;
    info.pAttachments = attachment;
    info.layers = 1;
    if (g->imageExtent.width == 0 || g->imageExtent.height == 0){
      info.width = 3840;
      info.height = 2160;
    } else {
      info.width = g->imageExtent.width;
      info.height = g->imageExtent.height;
    }

    for (uint32_t i = 0; i < imageCount; i++) {
      ImGui_ImplVulkanH_Frame *fd = (ImGui_ImplVulkanH_Frame *)&g->frames[i];
      attachment[0] = fd->BackbufferView;

      vkCreateFramebuffer(device, &info, g->allocator, &fd->Framebuffer);
    }
  }

  if (!gGuiStatus.descriptorPool) {
    // Create descriptor pool.
    VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo pool_info = { };
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    pool_info.pPoolSizes = poolSizes;

    vkCreateDescriptorPool(device, &pool_info, g->allocator, &g->descriptorPool);
  }
}

/**
 * ImGui initialization and drawing.
 */
static VkResult renderGui(
  VkQueue queue,
  const VkPresentInfoKHR *pPresentInfo
) {
  VkResult result = VK_SUCCESS;
  QueueData *queueData = getQueueData(queue);
  GuiStatus *g = &gGuiStatus;
  VkQueue graphicQueue = queueData->device->graphicQueue->queue;
  i32 isGraphic;

  g->device = queueData->device->device;
  isGraphic = isGraphicQueue(queue, &graphicQueue);

  for (u32 i = 0; i < pPresentInfo->swapchainCount; i++) {
    VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];
    u32 imageIndex = pPresentInfo->pImageIndices[i];
    ImGui_ImplVulkanH_Frame *f = (ImGui_ImplVulkanH_Frame *)&g->frames[imageIndex];
    ImGui_ImplVulkanH_FrameSemaphores* fs = &g->frameSemaphores[imageIndex];

    if (g->frames[0].Framebuffer == VK_NULL_HANDLE)
      createRenderTargetVk(g->device, swapchain);

    // Wait indefinitely instead of periodically checking.
    vkWaitForFences(g->device, 1, &f->Fence, VK_TRUE, UINT64_MAX);
    vkResetFences(g->device, 1, &f->Fence);

    {
      vkResetCommandPool(g->device, f->CommandPool, 0);
      VkCommandBufferBeginInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(f->CommandBuffer, &info);
    }
    {
      VkRenderPassBeginInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      info.renderPass = g->renderPass;
      info.framebuffer = f->Framebuffer;
      if (g->imageExtent.width == 0 || g->imageExtent.height == 0) {
        // We don't know the window size the first time. So we just set it to 4K.
        info.renderArea.extent.width = 3840;
        info.renderArea.extent.height = 2160;
      } else
        info.renderArea.extent = g->imageExtent;
      vkCmdBeginRenderPass(f->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    if (!ImGui::GetIO().BackendRendererUserData) {
      ImGui_ImplVulkan_InitInfo initInfo = {};
      initInfo.Instance = g->instance;
      initInfo.PhysicalDevice = g->physicalDevice;
      initInfo.Device = g->device;
      initInfo.QueueFamily = g->queueFamily;
      initInfo.Queue = graphicQueue;
      initInfo.DescriptorPool = g->descriptorPool;
      initInfo.RenderPass = g->renderPass;
      initInfo.MinImageCount = g->minImageCount;
      initInfo.ImageCount = g->minImageCount;
      initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
      initInfo.Allocator = g->allocator;
      // (Optional).
      initInfo.PipelineCache = VK_NULL_HANDLE;
      initInfo.Subpass = 0;
      ImGui_ImplVulkan_Init(&initInfo);
      ImGui_ImplVulkan_CreateFontsTexture();
    }

    // Create new frame.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Render ImGui.
    HTUpdateGUI();

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    // Record dear imgui primitives into command buffer.
    ImGui_ImplVulkan_RenderDrawData(drawData, f->CommandBuffer);

    // Submit command buffer.
    vkCmdEndRenderPass(f->CommandBuffer);
    vkEndCommandBuffer(f->CommandBuffer);
  
    u32 waitSemaphoresCount = i == 0 ? pPresentInfo->waitSemaphoreCount : 0;
    if (waitSemaphoresCount == 0 && !isGraphic) {
      VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      {
        // Submit an empty submission message on the current present queue.
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.pWaitDstStageMask = &waitStage;
        info.signalSemaphoreCount = 1;
        // Send a signal.
        info.pSignalSemaphores = &fs->RenderCompleteSemaphore;
        vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE);
      }
      {
        // Submit real ImGui rendering commands on the graphic queue.
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &f->CommandBuffer;
        info.pWaitDstStageMask = &waitStage;
        info.waitSemaphoreCount = 1;
        // Let the graphics queue wait for the semaphore sent by the present
        // queue in the previous step.
        info.pWaitSemaphores = &fs->RenderCompleteSemaphore;
        info.signalSemaphoreCount = 1;
        // Emit another semaphore after rendering is complete.
        info.pSignalSemaphores = &fs->ImageAcquiredSemaphore;
        vkQueueSubmit(graphicQueue, 1, &info, f->Fence);
      }
    } else {
      std::vector<VkPipelineStageFlags> waitStage(
        waitSemaphoresCount,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

      VkSubmitInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      info.commandBufferCount = 1;
      info.pCommandBuffers = &f->CommandBuffer;

      info.pWaitDstStageMask = waitStage.data();
      info.waitSemaphoreCount = waitSemaphoresCount;
      info.pWaitSemaphores = pPresentInfo->pWaitSemaphores;

      info.signalSemaphoreCount = 1;
      info.pSignalSemaphores = &fs->ImageAcquiredSemaphore;
      vkQueueSubmit(graphicQueue, 1, &info, f->Fence);

      VkPresentInfoKHR presentInfo = *pPresentInfo;
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = &swapchain;
      presentInfo.pImageIndices = &imageIndex;
      presentInfo.pWaitSemaphores = &fs->ImageAcquiredSemaphore;
      presentInfo.waitSemaphoreCount = 1;

      VkResult r = queueData->device->deviceTable.QueuePresentKHR(queue, &presentInfo);
      if (pPresentInfo->pResults)
        pPresentInfo->pResults[i] = r;
      if (r != VK_SUCCESS && result == VK_SUCCESS)
        result = r;
    }
  }

  return result;
}

// ----------------------------------------------------------------------------
// [SECTION] Local vulkan layer functions.
// ----------------------------------------------------------------------------

/**
 * Modified from SML-PC.
 * 
 * Create vulkan instance.
 */
static VKAPI_ATTR VkResult VKAPI_CALL HT_vkCreateInstance(
  const VkInstanceCreateInfo *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkInstance *pInstance
) {
  // Find the layer contains the loader's link info.
  VkLayerInstanceCreateInfo *createInfo = (VkLayerInstanceCreateInfo *)getChainInfo(
    (VkLayerCreateInfo_ *)pCreateInfo,
    VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO,
    VK_LAYER_LINK_INFO);

  if (!createInfo)
    // Can't find the link info.
    return VK_ERROR_INITIALIZATION_FAILED;

  // Create instance with the next layer's function.
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddrNext = createInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  // Move to the next layer.
  createInfo->u.pLayerInfo = createInfo->u.pLayerInfo->pNext;
  PFN_vkCreateInstance vkCreateInstanceNext = (PFN_vkCreateInstance)vkGetInstanceProcAddrNext(
    VK_NULL_HANDLE, "vkCreateInstance");
  if (!vkCreateInstanceNext)
    return VK_ERROR_INITIALIZATION_FAILED;
  VkResult ret = vkCreateInstanceNext(pCreateInfo, pAllocator, pInstance);
  if (ret != VK_SUCCESS)
    return ret;

  // Initialize the instance dispatch table with functions from the next layer.
  InstanceDispatchTable instanceTable;
  instanceTable.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddrNext(
    *pInstance, "vkGetInstanceProcAddr");
  instanceTable.DestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddrNext(
    *pInstance, "vkDestroyInstance");
  instanceTable.CreateDevice = (PFN_vkCreateDevice)vkGetInstanceProcAddrNext(
    *pInstance, "vkCreateDevice");

  // Store the table.
  std::lock_guard<std::mutex> lock(gMutex);
  gInstanceTables[*pInstance] = instanceTable;

  return VK_SUCCESS;
}

/**
 * Destroy VkInstance object.
 */
static VKAPI_ATTR void VKAPI_CALL HT_vkDestroyInstance(
  VkInstance instance,
  const VkAllocationCallbacks *pAllocator
) {
  InstanceDispatchTable *table = getInstanceDispatchTable(instance);

  if (table && table->DestroyInstance)
    table->DestroyInstance(instance, pAllocator);

  std::lock_guard<std::mutex> lock(gMutex);
  gInstanceTables.erase(instance);
}

/**
 * Modified from SML-PC.
 * 
 * Create VkDevice object, and record its related VkQueue object.
 */
static VKAPI_ATTR VkResult VKAPI_CALL HT_vkCreateDevice(
  VkPhysicalDevice physicalDevice,
  const VkDeviceCreateInfo *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkDevice *pDevice
) {
  // Find the layer contains the loader's link info.
  VkLayerDeviceCreateInfo *createInfo = (VkLayerDeviceCreateInfo *)getChainInfo(
    (VkLayerCreateInfo_ *)pCreateInfo,
    VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO,
    VK_LAYER_LINK_INFO);

  if (createInfo == nullptr)
    // Can't find the link info.
    return VK_ERROR_INITIALIZATION_FAILED;
  
  // Get the next layer's functions.
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddrNext = createInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddrNext = createInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  // Move to the next layer.
  createInfo->u.pLayerInfo = createInfo->u.pLayerInfo->pNext;
  // Create device with the next layer's vkCreateDevice() function.
  PFN_vkCreateDevice vkCreateDeviceNext = (PFN_vkCreateDevice)vkGetInstanceProcAddrNext(
    VK_NULL_HANDLE, "vkCreateDevice");
  if (!vkCreateDeviceNext)
    return VK_ERROR_INITIALIZATION_FAILED;
  VkResult ret = vkCreateDeviceNext(physicalDevice, pCreateInfo, pAllocator, pDevice);
  if (ret != VK_SUCCESS)
    return ret;

  // Initialize device dispatch table with functions from the next layer.
  DeviceDispatchTable deviceTable;
  deviceTable.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetDeviceProcAddrNext(
    *pDevice, "vkGetDeviceProcAddr");
  deviceTable.DestroyDevice = (PFN_vkDestroyDevice)vkGetDeviceProcAddrNext(
    *pDevice, "vkDestroyDevice");
  deviceTable.QueuePresentKHR = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddrNext(
    *pDevice, "vkQueuePresentKHR");
  deviceTable.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddrNext(
    *pDevice, "vkCreateSwapchainKHR");
  deviceTable.GetDeviceQueue = (PFN_vkGetDeviceQueue)vkGetDeviceProcAddrNext(
    *pDevice, "vkGetDeviceQueue");

  // Store the table and related VkQueue.
  std::lock_guard<std::mutex> lock(gMutex);
  DeviceData *deviceData = getDeviceData(*pDevice);
  deviceData->deviceTable = deviceTable;
  deviceData->device = *pDevice;
  VkLayerDeviceCreateInfo *loadDataInfo = (VkLayerDeviceCreateInfo *)getChainInfo(
    (VkLayerCreateInfo_ *)pCreateInfo,
    VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO,
    VK_LOADER_DATA_CALLBACK);

  deviceData->vkSetDeviceLoaderData = loadDataInfo->u.pfnSetDeviceLoaderData;
  setDeviceDataQueues(*pDevice, deviceData, pCreateInfo);

  return VK_SUCCESS;
}

/**
 * Destroy VkDevice object.
 */
static VKAPI_ATTR void VKAPI_CALL HT_vkDestroyDevice(
  VkDevice device,
  const VkAllocationCallbacks *pAllocator
) {
  DeviceDispatchTable *table = getDeviceDispatchTable(device);

  if (table && table->DestroyDevice)
    table->DestroyDevice(device, pAllocator);

  std::lock_guard<std::mutex> lock(gMutex);
  gDeviceData.erase(device);
}

/**
 * Create VkSwapchainKHR object.
 */
static VKAPI_ATTR VkResult VKAPI_CALL HT_vkCreateSwapchainKHR(
  VkDevice device,
  const VkSwapchainCreateInfoKHR *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkSwapchainKHR *pSwapchain
) {
  gGuiStatus.imageExtent = pCreateInfo->imageExtent;
  return getDeviceDispatchTable(device)->CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

/**
 * Present draw data. The ImGui calls injected here.
 */
static VKAPI_ATTR VkResult VKAPI_CALL HT_vkQueuePresentKHR(
  VkQueue queue,
  const VkPresentInfoKHR *pPresentInfo
) {
  if (!gGameStatus.window)
    return getDeviceDispatchTable(getQueueData(queue)->device->device)->QueuePresentKHR(queue, pPresentInfo);
  if (!gGuiStatus.isInited) {
    initVulkan();
    HTInitGUI();
    gGuiStatus.isInited = 1;
  }
  return renderGui(queue, pPresentInfo);
}

// ----------------------------------------------------------------------------
// [SECTION] Exported functions.
// ----------------------------------------------------------------------------

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL HT_vkGetDeviceProcAddr(
  VkDevice device,
  const char *pName
);

/**
 * The core export function of Vulkan layer.
 */
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL HT_vkGetInstanceProcAddr(
  VkInstance instance,
  const char *pName
) {
  // Instance functions.
  if (!strcmp(pName, "vkGetInstanceProcAddr"))
    return (PFN_vkVoidFunction)HT_vkGetInstanceProcAddr;
  if (!strcmp(pName, "vkCreateInstance"))
    return (PFN_vkVoidFunction)HT_vkCreateInstance;
  if (!strcmp(pName, "vkDestroyInstance"))
    return (PFN_vkVoidFunction)HT_vkDestroyInstance;
  
  // Device functions.
  if (!strcmp(pName, "vkGetDeviceProcAddr"))
    return (PFN_vkVoidFunction)HT_vkGetDeviceProcAddr;
  if (!strcmp(pName, "vkCreateDevice"))
    return (PFN_vkVoidFunction)HT_vkCreateDevice;
  if (!strcmp(pName, "vkDestroyDevice"))
    return (PFN_vkVoidFunction)HT_vkDestroyDevice;

  if (instance) {
    InstanceDispatchTable *table = getInstanceDispatchTable(instance);
    if (table && table->GetInstanceProcAddr) {
      return table->GetInstanceProcAddr(instance, pName);
    }
  }

  return nullptr;
}

/**
 * The core export function of Vulkan layer.
 */
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL HT_vkGetDeviceProcAddr(
  VkDevice device,
  const char *pName
) {
  if (!strcmp(pName, "vkGetDeviceProcAddr"))
    return (PFN_vkVoidFunction)HT_vkGetDeviceProcAddr;
  if (!strcmp(pName, "vkCreateDevice"))
    return (PFN_vkVoidFunction)HT_vkCreateDevice;
  if (!strcmp(pName, "vkDestroyDevice"))
    return (PFN_vkVoidFunction)HT_vkDestroyDevice;
  if (!strcmp(pName, "vkCreateSwapchainKHR"))
    return (PFN_vkVoidFunction)HT_vkCreateSwapchainKHR;
  if (!strcmp(pName, "vkQueuePresentKHR"))
    return (PFN_vkVoidFunction)HT_vkQueuePresentKHR;

  if (device) {
    DeviceDispatchTable *table = getDeviceDispatchTable(device);
    if (table && table->GetDeviceProcAddr) {
      return table->GetDeviceProcAddr(device, pName);
    }
  }

  return nullptr;
}
