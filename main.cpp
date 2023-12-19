#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

#include <vulkan/vulkan.hpp>

constexpr vk::ApplicationInfo create_application_info() {
  auto application_info               = vk::ApplicationInfo{};
  application_info.pApplicationName   = "picante";
  application_info.applicationVersion = 0;
  application_info.pEngineName        = "picante";
  application_info.apiVersion         = 0;
  return application_info;
};

vk::UniqueInstance create_instance() {
  const auto application_info = create_application_info();
  const auto instance_info    = std::invoke([&application_info] {
    auto instance_info              = vk::InstanceCreateInfo{};
    instance_info.pApplicationInfo  = &application_info;
    instance_info.enabledLayerCount = 0;
    // TODO I think I need certain layers for present and stuff
    instance_info.ppEnabledLayerNames     = nullptr;
    instance_info.enabledExtensionCount   = 0;
    instance_info.ppEnabledExtensionNames = nullptr;
    return instance_info;
  });
  return vk::createInstanceUnique(instance_info);
}

std::optional<vk::PhysicalDevice>
get_discrete_gpu(const vk::Instance& instance) {
  const auto physical_devices = instance.enumeratePhysicalDevices();
  const auto discrete_gpu     = std::find_if(
      std::begin(physical_devices), std::end(physical_devices),
      [](const auto& device) {
        const auto properties = device.getProperties();
        return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
      });
  if (discrete_gpu != std::end(physical_devices)) {
    const auto properties = (*discrete_gpu).getProperties();
    std::cout << "Found a discrete gpu: " << properties.deviceName << "\n";
    return (*discrete_gpu);
  } else {
    std::cout << "No discrete gpu! Your rig sucks!\n";
    return std::nullopt;
  }
}

std::optional<std::size_t>
get_graphics_queue_family_index(const vk::PhysicalDevice& physical_device) {
  const auto properties   = physical_device.getQueueFamilyProperties();
  const auto queue_family = std::find_if(
      std::begin(properties), std::end(properties), [](const auto& property) {
        return property.queueFlags & vk::QueueFlagBits::eGraphics;
      });
  if (queue_family != std::end(properties)) {
    // return the index of the queue family
    return std::distance(std::begin(properties), queue_family);
  } else {
    return std::nullopt;
  }
}

int main() {
  return 0;
}
