#pragma once
#include <sstream>
#include <string>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(x) do { VkResult err = x; if (err) { \
    std::cout << "Detected Vulkan error: " << err << " at " \
    << __FILE__ << ":" << __LINE__ << std::endl; \
    abort(); \
} } while(0)

std::string inline toHexString( uint32_t v ) { 
    std::stringstream s; s << std::hex << v; return s.str();}

std::string inline ver_string(const uint32_t v){
    return std::to_string(VK_VERSION_MAJOR(v)) + "." 
         + std::to_string(VK_VERSION_MINOR(v)) + "."
         + std::to_string(VK_VERSION_PATCH(v));
}

std::string inline vid_string(uint32_t v){
    switch(v){
      case VK_VENDOR_ID_KHRONOS     : return "Khronos";
      case VK_VENDOR_ID_VIV         : return "VIV";
      case VK_VENDOR_ID_VSI         : return "VSI";
      case VK_VENDOR_ID_KAZAN       : return "Kazan";
      case VK_VENDOR_ID_CODEPLAY    : return "Codeplay";
      case VK_VENDOR_ID_MESA        : return "MESA";
      case VK_VENDOR_ID_POCL        : return "Pocl";
      case VK_VENDOR_ID_MOBILEYE    : return "Mobileye";
      case 0x1002                   : return "AMD";
      case 0x1010                   : return "ImgTec";
      case 0x10DE                   : return "NVIDIA";
      case 0x13B5                   : return "ARM";
      case 0x5143                   : return "Qualcomm";
      case 0x8086                   : return "INTEL";
      default : return "invalid ( 0x" + toHexString( static_cast<uint32_t>( v ) ) + " )";
    }
}

std::string inline deviceType_string(VkPhysicalDeviceType v){
    switch(v){
      case VK_PHYSICAL_DEVICE_TYPE_OTHER            : return "Other";
      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU   : return "IntegratedGpu";
      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     : return "DiscreteGpu";
      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU      : return "VirtualGpu";
      case VK_PHYSICAL_DEVICE_TYPE_CPU              : return "Cpu";
      default : return "invalid ( 0x" + toHexString( static_cast<uint32_t>( v ) ) + " )";
    }
};

void inline printDeviceProps(const VkPhysicalDeviceProperties& p){
    std::cout << "\t"   << p.deviceName << std::endl;
    std::cout << "\t\tAPI Version    : " << ver_string(p.apiVersion) << std::endl;
    std::cout << "\t\tDriver Version : " << ver_string(p.driverVersion) << std::endl;
    std::cout << "\t\tDevice Info    : " << vid_string(p.vendorID) << " - " 
        << "0x" << toHexString(p.deviceID) << " - " 
        << deviceType_string(p.deviceType) << std::endl;
}

void inline printSparseProps(const VkPhysicalDeviceSparseProperties& p){
    std::cout << "\t\t" << p.residencyStandard2DBlockShape << std::endl;
    std::cout << "\t\t" << p.residencyStandard2DMultisampleBlockShape << std::endl;
    std::cout << "\t\t" << p.residencyStandard3DBlockShape << std::endl;
    std::cout << "\t\t" << p.residencyAlignedMipSize << std::endl;
    std::cout << "\t\t" << p.residencyNonResidentStrict << std::endl;
}

void inline printPhysicalLimits(const VkPhysicalDeviceLimits& p){

};
