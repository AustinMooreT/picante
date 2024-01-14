#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>

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
  static auto extensions =
      std::array<char*, 1>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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
  const auto surface_capabilities =
      physical_device.getSurfaceCapabilitiesKHR(surface);
  auto creation_info    = vk::SwapchainCreateInfoKHR{};
  creation_info.surface = surface;
  // make some assumpitons about what's available cause I'm lazy
  creation_info.minImageCount = surface_capabilities.minImageCount + 1;
  creation_info.imageFormat =
      vk::Format::eB8G8R8A8Srgb;  // blindly assume image format
  creation_info.imageColorSpace =
      vk::ColorSpaceKHR::eSrgbNonlinear;  // blindly assume color space
  creation_info.presentMode =
      vk::PresentModeKHR::eMailbox;  // blindly assume present mode
  creation_info.imageArrayLayers =
      1;  // Only not one when developing stereoscopic 3D app.
  creation_info.imageExtent      = vk::Extent2D{1024, 1024};
  creation_info.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
  creation_info.imageSharingMode = vk::SharingMode::eExclusive;
  creation_info.preTransform     = surface_capabilities.currentTransform;
  creation_info.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  creation_info.presentMode =
      vk::PresentModeKHR::eMailbox;  // blindly assuming mailbox presentation
                                     // mode
  creation_info.clipped = VK_TRUE;
  static auto index     = std::vector<uint32_t>{static_cast<uint32_t>(
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

vk::PipelineShaderStageCreateInfo
create_shader_pipeline_info(const vk::ShaderModule& module,
                            const vk::ShaderStageFlagBits shader_stage,
                            const std::string& name) {
  auto shader_pipeline_info   = vk::PipelineShaderStageCreateInfo{};
  shader_pipeline_info.module = module;
  shader_pipeline_info.stage  = shader_stage;
  shader_pipeline_info.pName  = name.c_str();
  return shader_pipeline_info;
}

// TODO clean this up I stole it from one of my old repos
vk::RenderPass create_render_pass(const vk::Device& logical_device) {
  static vk::AttachmentDescription colorAttachmentDescription{};
  colorAttachmentDescription.format         = vk::Format::eB8G8R8A8Srgb;
  colorAttachmentDescription.samples        = vk::SampleCountFlagBits::e1;
  colorAttachmentDescription.loadOp         = vk::AttachmentLoadOp::eClear;
  colorAttachmentDescription.storeOp        = vk::AttachmentStoreOp::eStore;
  colorAttachmentDescription.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
  colorAttachmentDescription.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  colorAttachmentDescription.initialLayout  = vk::ImageLayout::eUndefined;
  colorAttachmentDescription.finalLayout    = vk::ImageLayout::ePresentSrcKHR;
  static vk::AttachmentReference colorAttachmentReference;
  colorAttachmentReference.attachment = 0;
  colorAttachmentReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
  //
  // Subpass creation
  static vk::SubpassDescription basicSubpass;
  basicSubpass.pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;
  basicSubpass.colorAttachmentCount = 1;
  basicSubpass.pColorAttachments    = &colorAttachmentReference;
  //
  //
  // Render pass creation
  static vk::RenderPassCreateInfo renderPassInfo;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments    = &colorAttachmentDescription;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &basicSubpass;
  static vk::SubpassDependency subpassDependency;
  subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependency.dstSubpass = 0;
  subpassDependency.srcStageMask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpassDependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
  subpassDependency.dstStageMask =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  renderPassInfo.dependencyCount  = 1;
  renderPassInfo.pDependencies    = &subpassDependency;
  return logical_device.createRenderPass(renderPassInfo);
  //
}

vk::PipelineLayout
create_fixed_function_pipeline(const vk::Device& logical_device) {
  static vk::PipelineLayoutCreateInfo data{};
  return logical_device.createPipelineLayout(data);
}

void setup_render_pass(const vk::RenderPass& render_pass,
                       const vk::Pipeline& graphics_pipeline,
                       const vk::Framebuffer& frame_buffer,
                       const vk::CommandBuffer& command_buffer) {
  vk::CommandBufferBeginInfo commandBufferBeginInfo;
  command_buffer.begin(commandBufferBeginInfo);
  vk::RenderPassBeginInfo renderPassBeginInfo;
  renderPassBeginInfo.renderPass        = render_pass;
  renderPassBeginInfo.framebuffer       = frame_buffer;
  renderPassBeginInfo.renderArea.offset = vk::Offset2D{0, 0};
  renderPassBeginInfo.renderArea.extent = vk::Extent2D{1024, 1024};
  vk::ClearValue clearColor{std::array{0.0f, 0.0f, 0.0f, 1.0f}};  // black
  renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues    = &clearColor;
  // Begin recording
  command_buffer.beginRenderPass(&renderPassBeginInfo,
                                 vk::SubpassContents::eInline);
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                              graphics_pipeline);
  command_buffer.draw(3, 1, 0, 0);
  command_buffer.endRenderPass();
  command_buffer.end();
}

std::vector<vk::CommandBuffer> create_command_buffers(
    const vk::Device& logical_device,
    const std::vector<vk::Framebuffer>& frame_buffers,
    const std::function<void(const vk::Framebuffer&, const vk::CommandBuffer&)>&
        command_buffer_setup) {
  // Setup command buffers.
  vk::CommandPoolCreateInfo commandPoolInfo;
  commandPoolInfo.queueFamilyIndex = 0;
  vk::CommandPool commandPool{
      logical_device.createCommandPool(commandPoolInfo)};
  vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
  commandBufferAllocateInfo.commandPool = commandPool;
  commandBufferAllocateInfo.level       = vk::CommandBufferLevel::ePrimary;
  commandBufferAllocateInfo.commandBufferCount =
      static_cast<uint32_t>(frame_buffers.size());
  std::vector<vk::CommandBuffer> commandBuffers{
      logical_device.allocateCommandBuffers(commandBufferAllocateInfo)};
  for (const auto&& [frame_buffer, command_buffer] :
       std::ranges::views::zip(frame_buffers, commandBuffers)) {
    command_buffer_setup(frame_buffer, command_buffer);
  }
  return commandBuffers;
}

void draw_frame(const vk::Device& logical_device,
                const vk::SwapchainKHR& swapchain,
                const std::vector<vk::CommandBuffer>& command_buffers) {
  vk::SemaphoreCreateInfo imageIsAvailableInfo;
  vk::SemaphoreCreateInfo renderingFinishedInfo;
  vk::Semaphore imageIsAvailable{
      logical_device.createSemaphore(imageIsAvailableInfo)};
  vk::Semaphore renderingFinished{
      logical_device.createSemaphore(renderingFinishedInfo)};
  auto imageIndex = logical_device
                        .acquireNextImageKHR(swapchain, UINT64_MAX,
                                             imageIsAvailable, VK_NULL_HANDLE)
                        .value;
  vk::SubmitInfo submitInfo;
  std::array<vk::PipelineStageFlags, 1> waitStages{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  std::array waitSemaphore        = {imageIsAvailable};
  submitInfo.waitSemaphoreCount   = 1;
  submitInfo.pWaitSemaphores      = waitSemaphore.data();
  submitInfo.pWaitDstStageMask    = waitStages.data();
  submitInfo.pCommandBuffers      = &command_buffers[imageIndex];
  submitInfo.commandBufferCount   = 1;
  submitInfo.signalSemaphoreCount = 1;
  std::array signalSemaphore      = {renderingFinished};
  submitInfo.pSignalSemaphores    = signalSemaphore.data();
  vk::Queue queue                 = logical_device.getQueue(0, 0);
  if (vk::Result::eSuccess !=
      queue.submit(1, &submitInfo,
                   VK_NULL_HANDLE)) {  // submit draw work to queue
    // TODO blow up
  };
  vk::PresentInfoKHR presentInfo;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = signalSemaphore.data();
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = &swapchain;
  presentInfo.pImageIndices      = &imageIndex;
  if (vk::Result::eSuccess !=
      queue.presentKHR(&presentInfo)) {  // present queue
    // TODO blow up
  }
}

std::vector<vk::Framebuffer>
create_framebuffers(const vk::Device& logical_device,
                    const vk::RenderPass& render_pass,
                    const std::vector<vk::ImageView>& image_views) {
  // Create framebuffers
  std::vector<vk::Framebuffer> frameBuffers{image_views.size()};
  std::ranges::transform(
      image_views, frameBuffers.begin(),
      [&render_pass, &logical_device](const vk::ImageView& imageView) {
        std::array<vk::ImageView, 1> imageViewAttachment{imageView};
        vk::FramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.renderPass      = render_pass;
        frameBufferInfo.attachmentCount = 1;
        frameBufferInfo.pAttachments    = imageViewAttachment.data();
        frameBufferInfo.width           = 1024;
        frameBufferInfo.height          = 1024;
        frameBufferInfo.layers          = 1;
        return logical_device.createFramebuffer(frameBufferInfo);
      });
  return frameBuffers;
}

vk::Pipeline
create_graphics_pipeline(const vk::Device& logical_device,
                         const vk::RenderPass& render_pass,
                         const std::vector<vk::PipelineShaderStageCreateInfo>&
                             pipeline_shader_info) {
  // Setup vertex input
  static vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
  vertexInputInfo.vertexBindingDescriptionCount   = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  //
  // Setup input assembly
  static vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
  inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
  inputAssemblyInfo.primitiveRestartEnable = false;
  // Setup viewport
  static vk::Viewport viewport;
  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = static_cast<float>(1024);
  viewport.height   = static_cast<float>(1024);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  static vk::Rect2D scissor;
  scissor.offset = vk::Offset2D{0, 0};
  scissor.extent = vk::Extent2D{1024, 1024};
  static vk::PipelineViewportStateCreateInfo viewPortStateInfo;
  viewPortStateInfo.viewportCount = 1;
  viewPortStateInfo.pViewports    = &viewport;
  viewPortStateInfo.scissorCount  = 1;
  viewPortStateInfo.pScissors     = &scissor;
  // Setup rasterizer
  static vk::PipelineRasterizationStateCreateInfo rasterizerInfo;
  rasterizerInfo.depthClampEnable        = false;
  rasterizerInfo.rasterizerDiscardEnable = false;
  rasterizerInfo.polygonMode             = vk::PolygonMode::eFill;
  rasterizerInfo.lineWidth               = 1.0f;
  rasterizerInfo.cullMode                = vk::CullModeFlagBits::eBack;
  rasterizerInfo.frontFace               = vk::FrontFace::eClockwise;
  rasterizerInfo.depthBiasEnable         = false;
  //
  // Setup multisample

  static vk::PipelineMultisampleStateCreateInfo multisamplingInfo;
  multisamplingInfo.sampleShadingEnable  = false;
  multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
  //
  //
  // Setup color blending for framebuffers.
  static vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
  colorBlendAttachmentState.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eB |
      vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eA;
  colorBlendAttachmentState.blendEnable = false;
  static vk::PipelineColorBlendStateCreateInfo colorBlendStateInfo{};
  colorBlendStateInfo.logicOpEnable   = false;
  colorBlendStateInfo.attachmentCount = 1;
  colorBlendStateInfo.pAttachments    = &colorBlendAttachmentState;
  // Actually instantiate the pipeline
  static vk::GraphicsPipelineCreateInfo graphicsPipelineInfo{};
  graphicsPipelineInfo.stageCount          = pipeline_shader_info.size();
  graphicsPipelineInfo.pStages             = pipeline_shader_info.data();
  graphicsPipelineInfo.pVertexInputState   = &vertexInputInfo;
  graphicsPipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
  graphicsPipelineInfo.pViewportState      = &viewPortStateInfo;
  graphicsPipelineInfo.pRasterizationState = &rasterizerInfo;
  graphicsPipelineInfo.pMultisampleState   = &multisamplingInfo;
  graphicsPipelineInfo.pColorBlendState    = &colorBlendStateInfo;
  graphicsPipelineInfo.layout = create_fixed_function_pipeline(logical_device);
  graphicsPipelineInfo.renderPass = render_pass;
  graphicsPipelineInfo.subpass    = 0;
  return logical_device
      .createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineInfo)
      .value;
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
  const auto render_pass = create_render_pass(logical_device.value().get());
  const auto vertex_shader_str =
      "/home/maurice/picante_2/build/picante.vert.bin";
  const auto fragment_shader_str =
      "/home/maurice/picante_2/build/picante.frag.bin";
  const auto dummy_vertex_shader =
      load_shader_module(logical_device.value().get(), vertex_shader_str);
  const auto dummy_fragment_shader =
      load_shader_module(logical_device.value().get(), fragment_shader_str);
  static const auto shader_entry_point = std::string{"main"};
  const auto dummy_vertex_shader_info  = create_shader_pipeline_info(
      dummy_vertex_shader.value(), vk::ShaderStageFlagBits::eVertex,
      shader_entry_point);
  const auto dummy_fragment_shader_info = create_shader_pipeline_info(
      dummy_fragment_shader.value(), vk::ShaderStageFlagBits::eFragment,
      shader_entry_point);
  const auto shaders =
      std::vector{dummy_vertex_shader_info, dummy_fragment_shader_info};
  const auto graphics_pipeline = create_graphics_pipeline(
      logical_device.value().get(), render_pass, shaders);
  const auto frame_buffers = create_framebuffers(logical_device.value().get(),
                                                 render_pass, image_views);
  auto command_buffers =
      create_command_buffers(logical_device.value().get(), frame_buffers,
                             [&render_pass, &graphics_pipeline](
                                 const vk::Framebuffer& frame_buffer,
                                 const vk::CommandBuffer& command_buffer) {
                               setup_render_pass(render_pass, graphics_pipeline,
                                                 frame_buffer, command_buffer);
                             });

  glfwShowWindow(window.get());
  while (!glfwWindowShouldClose(window.get())) {
    draw_frame(logical_device.value().get(), swapchain, command_buffers);
    //  main loop do stuff
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}
