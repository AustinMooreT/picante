#include <functional>

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
  const auto instance_info    = std::invoke([&application_info]() {
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

int main() {
  return 0;
}
