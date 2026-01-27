#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
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

    std::vector<const char*> layers{};
    std::vector<const char*> extensions{};

    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;
    size_t queueFamilyIndex;

    VkShaderModule shader;

    void setExtensions();
    void setLayers();
    void initDevice();
    void initInstance();
    void initVulkan();

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

    VkDeviceQueueCreateInfo queueCI = {
    /*
    VkStructureType             sType;
    const void*                 pNext;
    VkDeviceQueueCreateFlags    flags;
    uint32_t                    queueFamilyIndex;
    uint32_t                    queueCount;
    const float*                pQueuePriorities;
    */
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        NULL,
        0,
        0,
        1,
        0
    };
    std::vector<VkDeviceCreateInfo> queueCreateInfos;
    VkDeviceCreateInfo deviceCI = {
        /*
    uint32_t                           queueCreateInfoCount;
    const VkDeviceQueueCreateInfo*     pQueueCreateInfos;
    // enabledLayerCount is legacy and should not be used
    uint32_t                           enabledLayerCount;
    // ppEnabledLayerNames is legacy and should not be used
    const char* const*                 ppEnabledLayerNames;
    uint32_t                           enabledExtensionCount;
    const char* const*                 ppEnabledExtensionNames;
    const VkPhysicalDeviceFeatures*    pEnabledFeatures;
    */
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        NULL,
        0,
        1,
        &queueCI,
        0,      //legacy
        NULL,   //legacy
        0,
        NULL,
        NULL
    };

    vkCreateDevice(physicalDevice, &deviceCI, NULL, &logicalDevice);
}

void State::initVulkan(){
    setExtensions();
    setLayers();
    initInstance();
    initDevice();
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
