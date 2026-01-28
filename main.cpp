#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "helpers.hpp"

struct State{
    const char* shaderPath = "../shader.frag";
    SDL_Window* window;
    VkInstance instance;
    uint32_t width = 640;
    uint32_t height = 480;

    std::vector<const char*> layers{};
    std::vector<const char*> extensions{};

    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    uint32_t graphicsQueueIndex;
    VkQueue graphicsQueue;

    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFramebuffer framebuffer;

    VkShaderModule shader;

    void initVulkan();
    void setExtensions();
    void setLayers();
    void initInstance();
    void initDevice();
    void initFramebuffer();

    void initSDL();
    void renderLoop();
    void exit();
    static VkShaderModule shaderProcessor(std::string shaderPath);
};

void State::setExtensions(){
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    VkExtensionProperties availableExtensions[count];
    vkEnumerateInstanceExtensionProperties(NULL, &count, availableExtensions);
    std::cout << "[+] Available Extensions" << std::endl;
    for(const auto& e : availableExtensions){
        std::cout << '\t' << e.extensionName << std::endl;
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
}

void State::initInstance(){
    VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        NULL,
        "shaderEditor",
        VK_MAKE_VERSION(1,0,0),
        "NULL",
        VK_MAKE_VERSION(0,0,0),
        VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        NULL,
        0,
        &appInfo,
        static_cast<uint32_t>(layers.size()),
        layers.data(),
        static_cast<uint32_t>(extensions.size()),
        extensions.data(),
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

    queueFamilyIndex = 0;
    queueCount = 1;
    graphicsQueueIndex = 0;

    VkDeviceQueueCreateInfo queueCI = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        NULL,
        0,
        queueFamilyIndex,
        queueCount,
        0
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {queueCI};
    VkDeviceCreateInfo deviceCI = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        NULL,
        0,
        1,
        &queueCI,
        0,      //legacy
        NULL,   //legacy
        0,
        NULL,
        NULL // look into these tbh
    };
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCI, NULL, &logicalDevice));

    vkGetDeviceQueue(logicalDevice, queueFamilyIndex, graphicsQueueIndex, &graphicsQueue);
}

void State::initFramebuffer(){
    VkSwapchainCreateInfoKHR swapchainCI = {
        //     VkStructureType                  sType;
        //     const void*                      pNext;
        //     VkSwapchainCreateFlagsKHR        flags;
        //     VkSurfaceKHR                     surface;
        //     uint32_t                         minImageCount;
        //     VkFormat                         imageFormat;
        //     VkColorSpaceKHR                  imageColorSpace;
        //     VkExtent2D                       imageExtent;
        //     uint32_t                         imageArrayLayers;
        //     VkImageUsageFlags                imageUsage;
        //     VkSharingMode                    imageSharingMode;
        //     uint32_t                         queueFamilyIndexCount;
        //     const uint32_t*                  pQueueFamilyIndices;
        //     VkSurfaceTransformFlagBitsKHR    preTransform;
        //     VkCompositeAlphaFlagBitsKHR      compositeAlpha;
        //     VkPresentModeKHR                 presentMode;
        //     VkBool32                         clipped;
        //     VkSwapchainKHR                   oldSwapchain;

    };
    VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &swapchainCI, NULL, &swapchain));

    for(int i = 0; i < images.size(); i++){
        VkImageViewCreateInfo imageviewCI = {

//    VkStructureType            sType;
//    const void*                pNext;
//    VkImageViewCreateFlags     flags;
//    VkImage                    image;
//    VkImageViewType            viewType;
//    VkFormat                   format;
//    VkComponentMapping         components;
//    VkImageSubresourceRange    subresourceRange;

        };
        vkCreateImageView(logicalDevice, &imageviewCI, NULL, &imageViews[i]);
    }
    VkRenderPassCreateInfo renderPassCI = {
    };
    VK_CHECK(vkCreateRenderPass(logicalDevice, &renderPassCI, NULL, &renderPass));

    VkFramebufferCreateInfo framebufferCI = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        renderPass,
        static_cast<uint32_t>(images.size()),
        imageViews.data(),
        width,
        height,
        0
    };
    VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferCI, NULL, &framebuffer));
};

void State::initVulkan(){
    setExtensions();
    setLayers();
    initInstance();
    initDevice();
    initFramebuffer();
}

void State::initSDL(){

}

VkShaderModule State::shaderProcessor(std::string shaderPath){
    VkShaderModule spv;
    std::string cmd = "glslc " + static_cast<std::string>(shaderPath);
    std::system(cmd.data());
    return spv;
};


void State::renderLoop(){
    shader = shaderProcessor(shaderPath);
    while(1){
        std::cout << "running" << '\r' << std::flush;
    }
};

int main(){
    State state;
    state.initVulkan();
    state.initSDL();
    state.renderLoop();
    state.exit();
}

void State::exit(){

    vkDestroyInstance(instance, NULL);
    std::cout << "exit(0)" << std::endl;
};
