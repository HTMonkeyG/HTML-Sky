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

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>

#include "aliases.h"
#include "gui.h"

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

// ----------------------------------------------------------------------------
// [SECTION] Local functions.
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
  printf("HT_vkCreateInstance called!\n");

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

  printf("HT_vkCreateInstance returned!\n");

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
  return getDeviceDispatchTable(device)->CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

/**
 * Present draw data. ImGui calls injected here.
 */
static VKAPI_ATTR VkResult VKAPI_CALL HT_vkQueuePresentKHR(
  VkQueue queue,
  const VkPresentInfoKHR *pPresentInfo
) {
  return getDeviceDispatchTable(getQueueData(queue)->device->device)->QueuePresentKHR(queue, pPresentInfo);
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
