#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <ratio>
#include <sstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define SDL_MAIN_HANDLED 1
#endif

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_vulkan.h>
#else
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_vulkan.h>
#endif

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "lib.hpp"

namespace fs = std::filesystem;

static fs::file_time_type getFileTimestamp(const char* path) {
    std::error_code ec;
    fs::file_time_type ts = fs::last_write_time(path, ec);
    if (ec) {
        return fs::file_time_type::min();
    }
    return ts;
}

struct Vertex{
    float pos[3];
    float color[3];
};

struct PushConstants{
    float resolution[2];
    float time;
    float _pad;
};

struct State{
    const char* shaderFragPath = "kernels/shader.frag";
    const char* shaderVertPath = "kernels/shader.vert";
    fs::file_time_type fragTs{};
    SDL_Window* window;
    uint32_t width = 640;
    uint32_t height = 480;
    bool running = true;
    std::chrono::time_point<std::chrono::steady_clock> tStart{}, tEnd{};
    std::chrono::duration<float> runtime{};
    std::chrono::duration<float> frameTime{1.0f / 144.0f};

    std::vector<const char*> sdlExtensions{};
    std::vector<const char*> layers{};
    std::vector<const char*> extensions{};
    std::vector<const char*> deviceExtensions{};

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkDevice logicalDevice;

    uint32_t familyIndex;
    uint32_t queueIndex;
    uint32_t queueCount;
    float    priority;
    VkQueue  queue;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    VkFormat swapchainImageFormat;
    VkColorSpaceKHR swapchainColorspace;
    VkSurfaceTransformFlagBitsKHR swapchainTransform;
    VkCompositeAlphaFlagBitsKHR swapchainAlpha;
    VkPresentModeKHR swapchainPresentMode;

    VkRenderPass renderPass;
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<VkSubpassDescription> subpassDescriptions;
    std::vector<VkSubpassDependency> subpassDependencies;

    std::vector<VkFramebuffer> framebuffers;

    VkPipelineLayout shaderPipelineLayout;
    VkPipeline shaderPipeline;

    VkViewport viewport;
    VkRect2D scissor;

    VkShaderModule fragModule;
    VkShaderModule vertModule;

    uint32_t frameIndex{0uz};
    std::vector<VkFence> fences;
    std::vector<VkSemaphore> renderCompleteSemaphores;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<Vertex> vertices;
    std::vector<VkVertexInputAttributeDescription> vAttributeDescriptions;
    VkVertexInputBindingDescription vBindingDescription;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    void initVulkan();
    void setExtensions();
    void setLayers();
    void initInstance();

    uint32_t findMemoryType(VkMemoryPropertyFlags f, uint32_t typeFilter);

    void initDevice();
    void initFramebuffer();
    void initShaders();
    void buildPipeline(VkShaderModule fragModule);
    void initResources();
    void runRenderPass(uint32_t imgIdx, VkPipeline pipeline);
    void rebuildFragShader();

    void initSDL();
    void getInput();
    void renderLoop();
    void appLogic();
    void exit();
};

void State::setExtensions(){
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    VkExtensionProperties availableExtensions[count];
    vkEnumerateInstanceExtensionProperties(NULL, &count, availableExtensions);
    std::cout << "[+] Available Extensions" << std::endl;
    for(const auto& e : availableExtensions){
        std::cout << '\t' << e.extensionName << std::endl;
        for(const auto& s : sdlExtensions){
            if(strcmp(e.extensionName, s) == 0){
                extensions.push_back(s);
            }
        }
    }

    std::cout << "[+] Enabled Extensions:" << std::endl;
    for(const auto& s : extensions){
        std::cout << "\t" << s << std::endl;
    }
}

void State::setLayers(){
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, NULL);
    VkLayerProperties availableLayers[count];
    vkEnumerateInstanceLayerProperties(&count, availableLayers);
    std::cout << "[+] Available Layers" << std::endl;
    for(const auto& l : availableLayers){
        std::cout << '\t' << l.layerName << std::endl;
    }

    std::cout << "[+] Enabled Layers:" << std::endl;
    for(const auto& s : layers){
        std::cout << '\t' << s << std::endl;
    }
}

void State::initInstance(){
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "shaderEditor",
        .applicationVersion = VK_MAKE_VERSION(1,0,0),
        .pEngineName = "NULL",
        .engineVersion = VK_MAKE_VERSION(1,0,0),
        .apiVersion = VK_API_VERSION_1_0
    };
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    vkCreateInstance(&createInfo, NULL, &instance);
};

void State::initDevice(){
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    VkPhysicalDevice devices[count];
    vkEnumeratePhysicalDevices(instance, &count, devices);
    std::cout << "[+] Available Devices" << std::endl;
    for(const auto& d : devices){
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures feats;
        vkGetPhysicalDeviceProperties(d, &props);
        vkGetPhysicalDeviceFeatures(d, &feats);
        vkGetPhysicalDeviceMemoryProperties(d, &memoryProperties);
        printDeviceProps(props);
        std::cout << "\t\t"; printBreak(37);
        printSparseProps(props.sparseProperties);
        std::cout << "\t\t"; printBreak(37);
        printPhysicalLimits(props.limits);
        std::cout << "\t\t"; printBreak(37);
        printPhysicalFeatures(feats);
        std::cout << "\t\t"; printBreak(37);
        printMemoryProps(memoryProperties);
        std::cout << "\t\t"; printBreak(37);
    }

    physicalDevice = devices[0];

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties queueProps[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps);
    for(const auto& q : queueProps){
        printQueueFamilyProperties(q);
        std::cout << "\t\t"; printBreak(37);
    }

    familyIndex = 0;
    queueCount = 1;
    queueIndex = 0;
    priority = 1.0f;

    VkDeviceQueueCreateInfo queueCI = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = familyIndex,
        .queueCount = queueCount,
        .pQueuePriorities = &priority
    };
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {queueCI};

    VkPhysicalDeviceFeatures deviceFeatures = {
        .sampleRateShading = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
    };

    uint32_t extCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extCount, NULL);
    VkExtensionProperties extensions[extCount];
    vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extCount, extensions);
    std::cout << "[+] Found Physical Device Extensions" << std::endl;
    for(const auto& e : extensions){
        std::cout << "\t" << e.extensionName << std::endl;
    };
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    VkDeviceCreateInfo deviceCI = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledLayerCount = 0,         //legacy
        .ppEnabledLayerNames = NULL,    //legacy
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = NULL
    };
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCI, NULL, &logicalDevice));
    std::cout << "[+] Enabled Device Extensions:" << std::endl;
    for(const auto& s : deviceExtensions){
        std::cout << "\t" << s << std::endl;
    };
    vkGetDeviceQueue(logicalDevice, familyIndex, queueIndex, &queue);

    VkBool32 support = 0;
    assert(SDL_Vulkan_CreateSurface(window, instance, &surface));
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &support);
    assert(support);
}

void State::initFramebuffer(){
    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL);
    VkSurfaceFormatKHR surfaceFormats[surfaceFormatCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
    std::cout << "[+] Found " << surfaceFormatCount << " Formats" << std::endl;
    bool foundFormat = false;
    for(const auto& f : surfaceFormats){
        if(f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            std::cout << "[!] Found Requested Format" << std::endl;
            swapchainColorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
            foundFormat = true;
        }
    }; assert(foundFormat);

    uint32_t presentCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, NULL);
    VkPresentModeKHR presentModes[presentCount];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes);
    bool foundPresent = false;
    for(const auto& p : presentModes){
        printPresentMode(p);
        if(p == VK_PRESENT_MODE_IMMEDIATE_KHR){
            swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            foundPresent = true;
        }
    }; assert(foundPresent);

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    swapchainImages.resize(capabilities.minImageCount);

    swapchainTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkSwapchainCreateInfoKHR scCI = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = surface,
        .minImageCount = static_cast<uint32_t>(swapchainImages.size()),
        .imageFormat = swapchainImageFormat,
        .imageColorSpace = swapchainColorspace,
        .imageExtent = {width, height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = swapchainTransform,
        .compositeAlpha = swapchainAlpha,
        .presentMode = swapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };
    VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &scCI, NULL, &swapchain));
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, NULL);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());
    std::cout << "[+] Using " << swapchainImages.size() << " Swapchain Images" << std::endl;

    swapchainImageViews.resize(swapchainImages.size());
    for(auto i{0uz} ; i < swapchainImageViews.size(); i++){
        VkImageViewCreateInfo iCI = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainImageFormat,
            .components = {},
            .subresourceRange = { 
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };
        VK_CHECK(vkCreateImageView(logicalDevice, &iCI, NULL, &swapchainImageViews[i]));
    }

    renderCompleteSemaphores.resize(swapchainImages.size());
    VkSemaphoreCreateInfo sCI = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for(auto& s : renderCompleteSemaphores)
        VK_CHECK(vkCreateSemaphore(logicalDevice, &sCI, NULL, &s));

    imageAvailableSemaphores.resize(swapchainImages.size());
    for(auto& s : imageAvailableSemaphores)
        VK_CHECK(vkCreateSemaphore(logicalDevice, &sCI, NULL, &s));

    VkFenceCreateInfo fCI = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    fences.resize(swapchainImages.size());
    for(auto& f : fences)
        VK_CHECK(vkCreateFence(logicalDevice, &fCI, NULL, &f));

    VkAttachmentDescription colorAttachment = {
        .flags = 0,
        .format = swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 
    };
    attachmentDescriptions.push_back(colorAttachment);
    VkAttachmentReference colorAttachmentReference = {
        .attachment = static_cast<uint32_t>(attachmentDescriptions.size() - 1),
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpassDescription = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentReference,
        .pResolveAttachments = 0,
        .pDepthStencilAttachment = 0,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = 0,
    };
    subpassDescriptions.push_back(subpassDescription);

    VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0,
    };
    subpassDependencies.push_back(subpassDependency);

    VkRenderPassCreateInfo rpCI = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()),
        .pAttachments = attachmentDescriptions.data(),
        .subpassCount = static_cast<uint32_t>(subpassDescriptions.size()),
        .pSubpasses = subpassDescriptions.data(),
        .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
        .pDependencies = subpassDependencies.data()
    };
    VK_CHECK(vkCreateRenderPass(logicalDevice, &rpCI, NULL, &renderPass));

    framebuffers.resize(swapchainImageViews.size());
    for(auto i{0uz}; i < framebuffers.size(); i++){
        std::vector<VkImageView> imageViews = {
            swapchainImageViews[i]
        };

        VkFramebufferCreateInfo fbCI = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .renderPass = renderPass,
            .attachmentCount = static_cast<uint32_t>(imageViews.size()),
            .pAttachments = imageViews.data(),
            .width = width,
            .height = height,
            .layers = 1,
        };
        VK_CHECK(vkCreateFramebuffer(logicalDevice, &fbCI, NULL, &framebuffers[i]));
    }
};

uint32_t State::findMemoryType(VkMemoryPropertyFlags f, uint32_t typeFilter){
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++){
        if((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & f) == f)
            return i;
    }
    return UINT32_MAX;
};

void State::initShaders(){
    const std::string fragPath = "artifacts/frag.spv";
    const std::string vertPath = "artifacts/vert.spv";
    std::cout << "[+] Creating Kernels" << std::endl;
    std::string cmd = "glslc \"" + static_cast<std::string>(shaderFragPath) + "\" -o \"" + fragPath + "\"";

    std::system(cmd.c_str());
    fragTs = getFileTimestamp(shaderFragPath);
    unsigned char* code;
    size_t codeSize;
    code = readFile(fragPath.c_str(), &codeSize);
    VkShaderModuleCreateInfo sCI = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags =  0,
        .codeSize = codeSize,
        .pCode = (const uint32_t*)code,
    };
    VK_CHECK(vkCreateShaderModule(logicalDevice, &sCI, NULL, &fragModule));

    cmd =  "glslc \"" + static_cast<std::string>(shaderVertPath) + "\" -o \"" + vertPath + "\"";
    std::system(cmd.c_str());
    code = readFile(vertPath.c_str(), &codeSize);
    sCI = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags =  0,
        .codeSize = codeSize,
        .pCode = (const uint32_t*)code,
    };
    VK_CHECK(vkCreateShaderModule(logicalDevice, &sCI, NULL, &vertModule));

    free(code);

    vertices = {
        {{ 1.0, 1.0, 0.0},{1.0,1.0,1.0}},
        {{-1.0, 1.0, 0.0},{1.0,1.0,1.0}},
        {{-1.0,-1.0, 0.0},{1.0,1.0,1.0}},

        {{ 1.0, 1.0, 0.0},{1.0,1.0,1.0}},
        {{-1.0,-1.0, 0.0},{1.0,1.0,1.0}},
        {{ 1.0,-1.0, 0.0},{1.0,1.0,1.0}},
    };

    vBindingDescription = {
        .binding = 0,
        .stride = sizeof(struct Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    vAttributeDescriptions = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(struct Vertex, pos)
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(struct Vertex, color)
        }
    };

    VkDeviceSize bufferSize = vertices.size() * sizeof(struct Vertex);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bCI = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };
    vkCreateBuffer(logicalDevice, &bCI, NULL, &stagingBuffer);
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memReqs.memoryTypeBits),
    };
    vkAllocateMemory(logicalDevice, &allocInfo, NULL, &stagingBufferMemory);
    vkBindBufferMemory(logicalDevice, stagingBuffer, stagingBufferMemory, 0);

    void* data;
    vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(logicalDevice, stagingBufferMemory);

    VkBufferCreateInfo bCI2 = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };
    vkCreateBuffer(logicalDevice, &bCI2, NULL, &vertexBuffer);
    vkGetBufferMemoryRequirements(logicalDevice, vertexBuffer, &memReqs);
    VkMemoryAllocateInfo allocInfo2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memReqs.memoryTypeBits),
    };
    vkAllocateMemory(logicalDevice, &allocInfo2, NULL, &vertexBufferMemory);
    vkBindBufferMemory(logicalDevice, vertexBuffer, vertexBufferMemory, 0);

    VkCommandBufferBeginInfo cbBI = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    vkBeginCommandBuffer(commandBuffers[0], &cbBI);
    VkBufferCopy copyRegion = {.size = bufferSize};
    vkCmdCopyBuffer(commandBuffers[0], stagingBuffer, vertexBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffers[0]);

    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[0],
    };
    vkQueueSubmit(queue, 1, &si, NULL);
    vkQueueWaitIdle(queue); // safe but heavy -_-

    vkDestroyBuffer(logicalDevice, stagingBuffer, NULL);
    vkFreeMemory(logicalDevice, stagingBufferMemory, NULL);
};

void State::buildPipeline(VkShaderModule fragModule){
    std::cout << "[+] Creating Pipelines" << std::endl;
    VkPushConstantRange pcR = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };

    VkPipelineLayoutCreateInfo plCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = NULL, 
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcR,
    };
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &plCI, NULL, &shaderPipelineLayout));

    VkPipelineShaderStageCreateInfo ssfCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragModule,
        .pName = "main",
        .pSpecializationInfo = NULL
    };

    VkPipelineShaderStageCreateInfo ssvCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertModule,
        .pName = "main",
        .pSpecializationInfo = NULL
    };

    VkPipelineShaderStageCreateInfo ss[] = {ssvCI, ssfCI};

    VkPipelineVertexInputStateCreateInfo viCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vBindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vAttributeDescriptions.size()),
        .pVertexAttributeDescriptions = vAttributeDescriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo iaCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = 0,
    };

    viewport = {
        .x = 0,
        .y = 0,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    scissor = {
        .offset = {},
        .extent = {
            .width = width,
            .height = height
        }
    };

    VkPipelineViewportStateCreateInfo vsCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rsCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthClampEnable = 0,
        .rasterizerDiscardEnable = 0,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = 0,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp = 0,
        .depthBiasSlopeFactor = 0,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo msCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = 0,
        .minSampleShading = 0,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = 0,
        .alphaToOneEnable = 0,
    };

    VkPipelineDepthStencilStateCreateInfo dsCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = 0,
        .depthWriteEnable = 0,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = 0,
        .stencilTestEnable = 0,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    VkPipelineColorBlendAttachmentState colorBlend = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo cbCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = 0,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlend,
        .blendConstants = {0.0f,0.0f,0.0f,0.0f}
    };

    VkPipelineDynamicStateCreateInfo dysCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = 0,
        .pDynamicStates = NULL
    };

    VkGraphicsPipelineCreateInfo gpCI = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stageCount = 2,
        .pStages = ss,
        .pVertexInputState = &viCI,
        .pInputAssemblyState = &iaCI,
        .pTessellationState = NULL,
        .pViewportState = &vsCI,
        .pRasterizationState = &rsCI,
        .pMultisampleState = &msCI,
        .pDepthStencilState = &dsCI,
        .pColorBlendState = &cbCI,
        .pDynamicState = &dysCI,
        .layout = shaderPipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1, 
    };
    VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, NULL, 1, &gpCI, NULL, &shaderPipeline));
};

void State::initResources(){
    std::cout << "[+] Creating Resources" << std::endl;
    VkCommandPoolCreateInfo cpCI = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = familyIndex,
    };
    VK_CHECK(vkCreateCommandPool(logicalDevice, &cpCI, NULL, &commandPool));

    commandBuffers.resize(swapchainImages.size());
    struct VkCommandBufferAllocateInfo cbAI = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 
        .pNext = NULL,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
    };
    VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &cbAI, commandBuffers.data()));
};

void State::initVulkan(){
    setExtensions();
    setLayers();
    initInstance();
    initDevice();
    initFramebuffer();
    initResources();
    initShaders();
    buildPipeline(fragModule);
}

void State::initSDL(){
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);
    window = SDL_CreateWindow("Shader Editor", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN);
    uint32_t count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &count, NULL);
    const char* names[count];
    SDL_Vulkan_GetInstanceExtensions(window, &count, names);
    std::cout << "[+] Found SDL Extensions" << std::endl;
    for(const auto& s : names){
        std::cout << "\t" << s << std::endl;
        sdlExtensions.push_back(s);
    };
}

void State::getInput(){
        SDL_Event e;
        SDL_PollEvent(&e);
        switch(e.type){
            case SDL_QUIT : running = false; break;
            case SDL_KEYDOWN : if(e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false; break;
            default: break;
        }
}

void State::runRenderPass(uint32_t imgIdx, VkPipeline pipeline){
    VkCommandBufferBeginInfo cbCI = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };
    VkClearValue clearColor[2] = {0};
    clearColor[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
    clearColor[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0}; 
    VkRenderPassBeginInfo rpBI = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = renderPass,
        .framebuffer = framebuffers[imgIdx],
        .renderArea = {{0,0},{width, height}},
        .clearValueCount = sizeof(clearColor)/sizeof(VkClearValue),
        .pClearValues = clearColor
    };

    vkBeginCommandBuffer(commandBuffers[frameIndex], &cbCI);
    vkCmdBeginRenderPass(commandBuffers[frameIndex], &rpBI, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    struct PushConstants pc = { {static_cast<float>(width), static_cast<float>(height)}, runtime.count(), 0.0f, };
    vkCmdPushConstants(commandBuffers[frameIndex], shaderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers[frameIndex], 0, 1, &vertexBuffer, offsets);
    vkCmdDraw(commandBuffers[frameIndex], vertices.size(), 1, 0, 0);

    vkCmdEndRenderPass(commandBuffers[frameIndex]);
    vkEndCommandBuffer(commandBuffers[frameIndex]);
}

void State::rebuildFragShader(){
    const std::string fragPath = "artifacts/frag.spv";
    std::cout << "[+] Rebuilding Frag ... " << std::endl;
    std::string cmd = "glslc \"" + static_cast<std::string>(shaderFragPath) + "\" -o \"" + fragPath + "\"";

    uint32_t res = std::system(cmd.c_str());
    if(res != 0){
        std::cout << "[!] Failed to build Frag" << std::endl;
        return;
    };

    fragTs = getFileTimestamp(shaderFragPath);
    unsigned char* code;
    size_t codeSize;
    code = readFile(fragPath.c_str(), &codeSize);
    VkShaderModuleCreateInfo sCI = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags =  0,
        .codeSize = codeSize,
        .pCode = (const uint32_t*)code,
    };
    VK_CHECK(vkCreateShaderModule(logicalDevice, &sCI, NULL, &fragModule));

    buildPipeline(fragModule);
};

void State::appLogic(){
    getInput();
    fs::file_time_type shaderTs = getFileTimestamp(shaderFragPath);
    if(shaderTs != fs::file_time_type::min() && shaderTs > fragTs){
        std::cout << "file has been written" << std::endl;
        fragTs = shaderTs;
        rebuildFragShader();
    }
};

void State::renderLoop(){
    std::cout << "[+] Entering RenderLoop" << std::endl;
    while(running){
        tStart = std::chrono::steady_clock::now();
        appLogic();

        vkWaitForFences(logicalDevice, 1, &fences[frameIndex], VK_TRUE, UINT64_MAX);
        uint32_t imgIdx;
        vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &imgIdx);
        vkResetFences(logicalDevice, 1, &fences[frameIndex]);
        vkResetCommandBuffer(commandBuffers[frameIndex], 0);

        runRenderPass(imgIdx, shaderPipeline);

        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = 0,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &imageAvailableSemaphores[frameIndex],
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderCompleteSemaphores[imgIdx], 
        };
        vkQueueSubmit(queue, 1, &submitInfo, fences[frameIndex]);

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = NULL,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderCompleteSemaphores[imgIdx],
            .swapchainCount = 1,
            .pSwapchains = &swapchain, 
            .pImageIndices = &imgIdx,
            .pResults = NULL
        };
        vkQueuePresentKHR(queue, &presentInfo);
        frameIndex = (frameIndex + 1) % swapchainImages.size();

        tEnd = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = tEnd - tStart;
        runtime += delta;
        if(delta < frameTime){
            std::this_thread::sleep_for(frameTime - delta);
            runtime+=(frameTime - delta);
        }
    }
    vkDeviceWaitIdle(logicalDevice);
};

int main(){
    State state;
    state.initSDL();
    state.initVulkan();
    state.renderLoop();
    state.exit();
}

void State::exit(){
    for(size_t i{0uz}; i<swapchainImages.size(); i++){
        vkDestroyFramebuffer(logicalDevice, framebuffers[i], NULL);
        vkDestroyImageView(logicalDevice, swapchainImageViews[i], NULL);
        vkDestroyFence(logicalDevice, fences[i], NULL);
        vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(logicalDevice, renderCompleteSemaphores[i], NULL);
    };
    vkDestroySwapchainKHR(logicalDevice, swapchain, NULL);
    SDL_DestroyWindow(window);

    vkDestroyBuffer(logicalDevice, vertexBuffer, NULL);
    vkFreeMemory(logicalDevice, vertexBufferMemory, NULL);

    vkDestroyPipeline(logicalDevice, shaderPipeline, NULL);
    vkDestroyPipelineLayout(logicalDevice, shaderPipelineLayout, NULL);

    vkDestroyRenderPass(logicalDevice, renderPass, NULL);
    vkDestroyShaderModule(logicalDevice, fragModule, NULL);
    vkDestroyShaderModule(logicalDevice, vertModule, NULL);

    vkFreeCommandBuffers(logicalDevice, commandPool, commandBuffers.size(), commandBuffers.data());
    vkDestroyCommandPool(logicalDevice, commandPool, NULL);

    vkDestroyDevice(logicalDevice, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);

    vkDestroyInstance(instance, NULL);
    SDL_Quit();

    std::cout << std::endl << "exit(0)" << std::endl;
};
