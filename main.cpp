#include <cassert>
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

    VkShaderModule shader;

    void setExtensions();
    void setLayers();
    void initDevice();
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

void State::initDevice(){
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    VkPhysicalDevice devices[count];
    vkEnumeratePhysicalDevices(instance, &count, devices);
    std::cout << "[+] Available Devices" << std::endl;
    for(const auto& d : devices){
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        printDeviceProps(props);
    }
}

void State::initVulkan(){
    setExtensions();
    setLayers();
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
