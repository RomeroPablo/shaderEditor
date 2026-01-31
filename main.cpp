#include <SDL2/SDL_vulkan.h>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <pthread.h>
#include <sstream>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "SDL_video.h"
#include "helpers.hpp"

struct State{
    const char* shaderPath = "../shader.frag";
    SDL_Window* window;
    uint32_t width = 640;
    uint32_t height = 480;

    std::vector<const char*> sdlExtensions{};
    std::vector<const char*> layers{};
    std::vector<const char*> extensions{};
    std::vector<const char*> deviceExtensions{};

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
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
    std::vector<VkSemaphore> presentationCompleteSemaphores;
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

    std::vector<VkDescriptorSetLayout> descriptorLayouts;
    std::vector<VkPushConstantRange> pcRanges;
    VkPipelineLayout shaderPipelineLayout;
    VkPipeline shaderPipeline;

    VkShaderModule shaderModule;

    void initVulkan();
    void setExtensions();
    void setLayers();
    void initInstance();

    void initGraphics();
    void initDevice();
    void initFramebuffer();
    void initShaders();
    void initPipeline();

    void initSDL();
    void renderLoop();
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
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceProperties(d, &props);
        vkGetPhysicalDeviceFeatures(d, &feats);
        vkGetPhysicalDeviceMemoryProperties(d, &memProps);
        printDeviceProps(props);
        std::cout << "\t\t"; printBreak(37);
        printSparseProps(props.sparseProperties);
        std::cout << "\t\t"; printBreak(37);
        printPhysicalLimits(props.limits);
        std::cout << "\t\t"; printBreak(37);
        printPhysicalFeatures(feats);
        std::cout << "\t\t"; printBreak(37);
        printMemoryProps(memProps);
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
            .format = surfaceFormats->format,
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

    presentationCompleteSemaphores.resize(swapchainImages.size());
    VkSemaphoreCreateInfo sCI = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for(auto& s : presentationCompleteSemaphores)
        VK_CHECK(vkCreateSemaphore(logicalDevice, &sCI, NULL, &s));

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

void State::initShaders(){
    std::string cmd = "glslc " + static_cast<std::string>(shaderPath);
    std::system(cmd.data());
    const char* spvPath = "../a.spv";
    unsigned char* code;
    size_t size;
    code = readFile(spvPath, &size);
    VkShaderModuleCreateInfo sCI = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags =  0,
        .codeSize = size,
        .pCode = (const uint32_t*)code,
    };
    VK_CHECK(vkCreateShaderModule(logicalDevice, &sCI, NULL, &shaderModule));
};

void State::initPipeline(){
    VkPipelineLayoutCreateInfo plCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size()),
        .pSetLayouts = descriptorLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size()),
        .pPushConstantRanges = pcRanges.data(),
    };
    vkCreatePipelineLayout(logicalDevice, &plCI, NULL, &shaderPipelineLayout);
};

void State::initVulkan(){
    setExtensions();
    setLayers();
    initInstance();
    initDevice();
    initFramebuffer();
    initShaders();
    initPipeline();
}

void State::initSDL(){
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

void State::renderLoop(){
    while(1){
        std::cout << "running" << '\r' << std::flush;
    }
};

int main(){
    State state;
    state.initSDL();
    state.initVulkan();
    state.renderLoop();
    state.exit();
}

void State::exit(){

    vkDestroyInstance(instance, NULL);
    std::cout << "exit(0)" << std::endl;
};
