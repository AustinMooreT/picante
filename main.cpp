#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>

#define VK_USE_PLATFORM_WAYLAND_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
  uint32_t glfw_extension_count = 0;
  const char** glfw_extensions{nullptr};
  glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
  std::vector<const char*> extensions(glfw_extensions,
                                      glfw_extensions + glfw_extension_count);
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  const std::vector<const char*> validation_layers = {
      "VK_LAYER_KHRONOS_validation"};
  const auto application_info = create_application_info();
  const auto instance_info =
      std::invoke([&application_info, &extensions, &validation_layers] {
        auto instance_info                    = vk::InstanceCreateInfo{};
        instance_info.pApplicationInfo        = &application_info;
        instance_info.enabledLayerCount       = validation_layers.size();
        instance_info.ppEnabledLayerNames     = validation_layers.data();
        instance_info.enabledExtensionCount   = extensions.size();
        instance_info.ppEnabledExtensionNames = extensions.data();
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
  static auto extensions = std::vector<char*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  device_info.ppEnabledExtensionNames = extensions.data();
  device_info.enabledExtensionCount   = 1;
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

std::shared_ptr<GLFWwindow> create_window() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // No need for opengl context
  glfwWindowHint(GLFW_RESIZABLE,
                 GLFW_FALSE);  // No window resizing cause I'm lazy
  const auto window = glfwCreateWindow(1024, 1024, "picante", nullptr, nullptr);
  return std::shared_ptr<GLFWwindow>{window, [](auto* window_ptr) {
                                       glfwDestroyWindow(window_ptr);
                                     }};
}

std::optional<vk::SurfaceKHR>
create_surface(const vk::Instance& instance,
               const std::shared_ptr<GLFWwindow>& window) {
  VkSurfaceKHR surface{};
  if (glfwCreateWindowSurface(static_cast<VkInstance>(instance), window.get(),
                              nullptr,
                              &surface) == VK_ERROR_INITIALIZATION_FAILED) {
    std::cout << "Uh oh! Failed to create a surface!\n";
    return std::nullopt;
  } else {
    return vk::SurfaceKHR{surface};
  }
}

VkSwapchainKHR create_swapchain(const vk::SurfaceKHR& surface,
                                const vk::PhysicalDevice& physical_device,
                                const vk::Device& logical_device) {
  auto creation_info    = vk::SwapchainCreateInfoKHR{};
  creation_info.surface = surface;
  // make some assumpitons about what's available cause I'm lazy
  creation_info.minImageCount   = 2;
  creation_info.imageColorSpace = vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;
  creation_info.imageFormat     = vk::Format::eB8G8R8A8Srgb;
  creation_info.imageExtent     = VkExtent2D{1024, 1024};
  creation_info.imageArrayLayers      = 1;
  creation_info.compositeAlpha        = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  creation_info.clipped               = true;
  creation_info.presentMode           = vk::PresentModeKHR::eMailbox;
  creation_info.queueFamilyIndexCount = 1;
  static auto index = std::vector<uint32_t>{static_cast<uint32_t>(
      get_graphics_queue_family_index(physical_device).value())};
  creation_info.pQueueFamilyIndices = index.data();
  return logical_device.createSwapchainKHR(creation_info);
}

std::optional<std::vector<char>>
load_shader_data(const std::filesystem::path& path_to_shader) {
  if (!std::filesystem::exists(path_to_shader)) {
    return std::nullopt;
  }
  std::ifstream file(path_to_shader, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return std::nullopt;
  }
  const auto file_size = file.tellg();
  auto file_data       = std::vector<char>{};
  file_data.resize(file_size);
  file.seekg(0);
  file.read(file_data.data(), file_size);
  return file_data;
}

std::optional<vk::ShaderModule>
load_shader_module(const vk::Device& logical_device,
                   const std::filesystem::path& path_to_shader) {
  return load_shader_data(path_to_shader)
      .transform([&logical_device](const auto& shader_data) {
        auto creation_info     = vk::ShaderModuleCreateInfo{};
        creation_info.codeSize = shader_data.size();
        creation_info.pCode =
            reinterpret_cast<const uint32_t*>(shader_data.data());
        return logical_device.createShaderModule(creation_info);
      });
}

std::vector<vk::ImageView>
create_image_views(const vk::Device& logical_device,
                   const std::vector<vk::Image> images) {

  auto image_views = std::vector<vk::ImageView>{images.size()};
  std::ranges::transform(
      images, image_views.begin(),
      [&logical_device](const vk::Image& image) -> vk::ImageView {
        vk::ImageViewCreateInfo imageInfo;
        imageInfo.image    = image;
        imageInfo.viewType = vk::ImageViewType::e2D;
        imageInfo.format =
            vk::Format::eB8G8R8A8Srgb;  // Blindly assume image format
        imageInfo.components.r                = vk::ComponentSwizzle::eIdentity;
        imageInfo.components.g                = vk::ComponentSwizzle::eIdentity;
        imageInfo.components.b                = vk::ComponentSwizzle::eIdentity;
        imageInfo.components.a                = vk::ComponentSwizzle::eIdentity;
        imageInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageInfo.subresourceRange.baseMipLevel   = 0;
        imageInfo.subresourceRange.levelCount     = 1;
        imageInfo.subresourceRange.baseArrayLayer = 0;
        imageInfo.subresourceRange.layerCount     = 1;
        return logical_device.createImageView(imageInfo);
      });
  return image_views;
}

int main() {
  glfwInit();

  // device and queue creation
  const auto instance        = create_instance();
  const auto physical_device = get_discrete_gpu(instance.get());
  const auto logical_device  = physical_device.and_then(create_logical_device);
  const auto queue = physical_device.and_then([&](const auto& physical_device) {
    return logical_device.transform(
        [&](const auto& logical_device) { return get_queue; });
  });
  const auto window    = create_window();
  const auto surface   = create_surface(instance.get(), window);
  const auto swapchain = create_swapchain(
      surface.value(), physical_device.value(), logical_device.value().get());
  const auto images =
      logical_device.value().get().getSwapchainImagesKHR(swapchain);
  const auto image_views =
      create_image_views(logical_device.value().get(), images);

  glfwShowWindow(window.get());
  while (!glfwWindowShouldClose(window.get())) {
    // main loop do stuff
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
