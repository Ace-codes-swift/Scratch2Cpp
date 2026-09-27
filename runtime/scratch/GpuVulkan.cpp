#include "scratch/GpuBackends.hpp"

#ifndef SCRATCH_HAS_VULKAN
namespace scratch {
bool gpuTraceVulkan(const std::vector<GpuTriangle>&, const GpuParams&, std::vector<std::uint8_t>&) {
    return false;
}
}  // namespace scratch
#else

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>

#include <cstring>
#include <string>

#include "scratch/GpuSpirv.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace scratch {
namespace {

void* loadLib() {
#if defined(_WIN32)
    return static_cast<void*>(LoadLibraryA("vulkan-1.dll"));
#elif defined(__APPLE__)
    void* h = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
    return h;
#else
    void* h = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    return h;
#endif
}

void* libSym(void* lib, const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
#else
    return dlsym(lib, name);
#endif
}

#define LOAD(fn) fn = reinterpret_cast<PFN_##fn>(libSym(lib, #fn))
#define LOADI(fn) fn = reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(instance, #fn))
#define LOADD(fn) fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn))

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
PFN_vkCreateInstance vkCreateInstance = nullptr;
PFN_vkDestroyInstance vkDestroyInstance = nullptr;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
PFN_vkCreateDevice vkCreateDevice = nullptr;
PFN_vkDestroyDevice vkDestroyDevice = nullptr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
PFN_vkCreateBuffer vkCreateBuffer = nullptr;
PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
PFN_vkAllocateMemory vkAllocateMemory = nullptr;
PFN_vkFreeMemory vkFreeMemory = nullptr;
PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
PFN_vkMapMemory vkMapMemory = nullptr;
PFN_vkUnmapMemory vkUnmapMemory = nullptr;
PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
PFN_vkCmdDispatch vkCmdDispatch = nullptr;
PFN_vkQueueSubmit vkQueueSubmit = nullptr;
PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;

bool findMemory(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags flags, uint32_t& out) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(phys, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            out = i;
            return true;
        }
    }
    return false;
}

bool createBuffer(VkDevice device, VkPhysicalDevice phys, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkBuffer& buffer, VkDeviceMemory& memory, void** mapped) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bi, nullptr, &buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, buffer, &req);
    uint32_t type = 0;
    if (!findMemory(phys, req.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, type)) {
        return false;
    }
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = type;
    if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) return false;
    if (mapped && vkMapMemory(device, memory, 0, size, 0, mapped) != VK_SUCCESS) return false;
    return true;
}

}  // namespace

bool gpuTraceVulkan(const std::vector<GpuTriangle>& tris, const GpuParams& params,
                    std::vector<std::uint8_t>& outRgba) {
    if (tris.empty() || params.grid <= 0) return false;
    void* lib = loadLib();
    if (!lib) return false;
    LOAD(vkGetInstanceProcAddr);
    if (!vkGetInstanceProcAddr) return false;
    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    if (!vkCreateInstance) return false;

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "scratch2cpp";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) return false;

    LOADI(vkDestroyInstance);
    LOADI(vkEnumeratePhysicalDevices);
    LOADI(vkGetPhysicalDeviceQueueFamilyProperties);
    LOADI(vkGetPhysicalDeviceMemoryProperties);
    LOADI(vkCreateDevice);
    LOADI(vkGetDeviceProcAddr);

    uint32_t physCount = 0;
    vkEnumeratePhysicalDevices(instance, &physCount, nullptr);
    if (physCount == 0) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    std::vector<VkPhysicalDevice> physList(physCount);
    vkEnumeratePhysicalDevices(instance, &physCount, physList.data());

    VkPhysicalDevice phys = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    for (VkPhysicalDevice p : physList) {
        uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(p, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(p, &qn, qf.data());
        for (uint32_t i = 0; i < qn; ++i) {
            if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                phys = p;
                queueFamily = i;
                break;
            }
        }
        if (phys) break;
    }
    if (!phys) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(phys, &dci, nullptr, &device) != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }

    LOADD(vkDestroyDevice);
    LOADD(vkGetDeviceQueue);
    LOADD(vkCreateBuffer);
    LOADD(vkDestroyBuffer);
    LOADD(vkGetBufferMemoryRequirements);
    LOADD(vkAllocateMemory);
    LOADD(vkFreeMemory);
    LOADD(vkBindBufferMemory);
    LOADD(vkMapMemory);
    LOADD(vkUnmapMemory);
    LOADD(vkCreateShaderModule);
    LOADD(vkDestroyShaderModule);
    LOADD(vkCreateDescriptorSetLayout);
    LOADD(vkDestroyDescriptorSetLayout);
    LOADD(vkCreatePipelineLayout);
    LOADD(vkDestroyPipelineLayout);
    LOADD(vkCreateComputePipelines);
    LOADD(vkDestroyPipeline);
    LOADD(vkCreateDescriptorPool);
    LOADD(vkDestroyDescriptorPool);
    LOADD(vkAllocateDescriptorSets);
    LOADD(vkUpdateDescriptorSets);
    LOADD(vkCreateCommandPool);
    LOADD(vkDestroyCommandPool);
    LOADD(vkAllocateCommandBuffers);
    LOADD(vkBeginCommandBuffer);
    LOADD(vkEndCommandBuffer);
    LOADD(vkCmdBindPipeline);
    LOADD(vkCmdBindDescriptorSets);
    LOADD(vkCmdDispatch);
    LOADD(vkQueueSubmit);
    LOADD(vkQueueWaitIdle);

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    const VkDeviceSize triBytes = tris.size() * sizeof(GpuTriangle);
    const VkDeviceSize outBytes = static_cast<VkDeviceSize>(params.grid) * params.grid * 4;
    const VkDeviceSize uniBytes = sizeof(GpuParams);
    VkBuffer triBuf = VK_NULL_HANDLE, outBuf = VK_NULL_HANDLE, uniBuf = VK_NULL_HANDLE;
    VkDeviceMemory triMem = VK_NULL_HANDLE, outMem = VK_NULL_HANDLE, uniMem = VK_NULL_HANDLE;
    void *triPtr = nullptr, *outPtr = nullptr, *uniPtr = nullptr;
    bool ok = createBuffer(device, phys, triBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, triBuf, triMem, &triPtr) &&
              createBuffer(device, phys, outBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, outBuf, outMem, &outPtr) &&
              createBuffer(device, phys, uniBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniBuf, uniMem, &uniPtr);
    if (!ok) {
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    std::memcpy(triPtr, tris.data(), static_cast<size_t>(triBytes));
    std::memcpy(uniPtr, &params, sizeof(params));
    std::memset(outPtr, 0, static_cast<size_t>(outBytes));

    VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smi.codeSize = kTraceSpirvSize;
    smi.pCode = reinterpret_cast<const uint32_t*>(kTraceSpirv);
    VkShaderModule shader = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &smi, nullptr, &shader) != VK_SUCCESS) {
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return false;
    }

    VkDescriptorSetLayoutBinding binds[3]{};
    binds[0].binding = 0;
    binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binds[0].descriptorCount = 1;
    binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    binds[1].binding = 1;
    binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binds[1].descriptorCount = 1;
    binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    binds[2].binding = 0;
    binds[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binds[2].descriptorCount = 1;
    binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo l0{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    l0.bindingCount = 2;
    l0.pBindings = binds;
    VkDescriptorSetLayoutCreateInfo l1{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    l1.bindingCount = 1;
    l1.pBindings = binds + 2;
    VkDescriptorSetLayout layouts[2]{};
    vkCreateDescriptorSetLayout(device, &l0, nullptr, &layouts[0]);
    vkCreateDescriptorSetLayout(device, &l1, nullptr, &layouts[1]);

    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pli.setLayoutCount = 2;
    pli.pSetLayouts = layouts;
    VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &pli, nullptr, &pipeLayout);

    VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pci.stage.module = shader;
    pci.stage.pName = "main";
    pci.layout = pipeLayout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return false;
    }

    VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = 2;
    dpi.poolSizeCount = 2;
    dpi.pPoolSizes = sizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    vkCreateDescriptorPool(device, &dpi, nullptr, &pool);
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = pool;
    dai.descriptorSetCount = 2;
    dai.pSetLayouts = layouts;
    VkDescriptorSet sets[2]{};
    vkAllocateDescriptorSets(device, &dai, sets);

    VkDescriptorBufferInfo bTri{triBuf, 0, triBytes};
    VkDescriptorBufferInfo bOut{outBuf, 0, outBytes};
    VkDescriptorBufferInfo bUni{uniBuf, 0, uniBytes};
    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = sets[0];
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &bTri;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = sets[0];
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &bOut;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = sets[1];
    writes[2].dstBinding = 0;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &bUni;
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = queueFamily;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    vkCreateCommandPool(device, &cpi, nullptr, &cmdPool);
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmdPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &cai, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &bi);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 2, sets, 0, nullptr);
    vkCmdDispatch(cmd, static_cast<uint32_t>((params.grid + 7) / 8), static_cast<uint32_t>((params.grid + 7) / 8), 1);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    if (vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    vkQueueWaitIdle(queue);

    outRgba.resize(static_cast<size_t>(outBytes));
    std::memcpy(outRgba.data(), outPtr, static_cast<size_t>(outBytes));

    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, layouts[0], nullptr);
    vkDestroyDescriptorSetLayout(device, layouts[1], nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkDestroyBuffer(device, triBuf, nullptr);
    vkDestroyBuffer(device, outBuf, nullptr);
    vkDestroyBuffer(device, uniBuf, nullptr);
    vkFreeMemory(device, triMem, nullptr);
    vkFreeMemory(device, outMem, nullptr);
    vkFreeMemory(device, uniMem, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Log("GPU: Vulkan compute path");
    return true;
}

}  // namespace scratch

#endif
