#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

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

vk::DeviceQueueCreateInfo
create_logical_device_queue_info(const std::size_t queue_family_index,
                                 const std::array<float, 1>& priorities) {
  auto device_queue_info             = vk::DeviceQueueCreateInfo{};
  device_queue_info.queueFamilyIndex = queue_family_index;
  device_queue_info.queueCount       = 1;
  device_queue_info.setQueuePriorities(priorities);
  return device_queue_info;
}

vk::DeviceCreateInfo
create_logical_device_info(const vk::DeviceQueueCreateInfo& queue_info) {
  auto device_info                 = vk::DeviceCreateInfo{};
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos    = &queue_info;
  return device_info;
}

std::optional<vk::UniqueDevice>
create_logical_device(const vk::PhysicalDevice& physical_device) {
  const auto priorities = std::array<float, 1>{1.0f};
  return get_graphics_queue_family_index(physical_device)
      .transform([&priorities, &physical_device](const auto& index) {
        const auto device_queue_info =
            create_logical_device_queue_info(index, priorities);
        const auto device_info = create_logical_device_info(device_queue_info);
        return physical_device.createDeviceUnique(device_info);
      });
}

vk::Queue get_queue(const vk::PhysicalDevice& physical_device,
                    const vk::UniqueDevice& logical_device) {
  return logical_device.get().getQueue(
      get_graphics_queue_family_index(physical_device).value(), 0);
}

std::shared_ptr<SDL_Window> create_window() {
  const auto window = SDL_CreateWindow("picante", 0, 0, 1024, 1024,
                                       SDL_WINDOW_SHOWN & SDL_WINDOW_VULKAN);
  return std::shared_ptr<SDL_Window>{window, [](const auto* window_ptr) {
                                       SDL_DestroyWindow(window_ptr);
                                     }};
}

std::optional<std::pair<wl_display*, wl_surface*>>
get_wayland_goodies(const std::shared_ptr<SDL_Window>& window) {
  const auto info =
      std::invoke([&window] -> decltype(std::optional<SDL_SysWMinfo>()) {
        SDL_SysWMinfo info;
        if (SDL_GetWindowWMInfo(window.get(), &info)) {
          return info;
        } else {
          return std::nullopt;
        }
      });
  return info.transform([window](const auto& info) {
    return std::make_pair(info.info.wl.display, info.info.wl.surface);
  });
}

int main() {
  const auto instance        = create_instance();
  const auto physical_device = get_discrete_gpu(instance.get());
  const auto logical_device  = physical_device.and_then(create_logical_device);
  const auto queue = physical_device.and_then([&](const auto& physical_device) {
    return logical_device.transform(
        [&](const auto& logical_device) { return get_queue; });
  });

  return 0;
}
