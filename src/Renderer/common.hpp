#pragma once

#include <iostream>
#include <vector>
#include <optional>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan_initializers.hpp"

#include <vulkan/vulkan.h>

constexpr int INIT_WIDTH = 1280;
constexpr int INIT_HEIGHT = 720;

constexpr bool mouse_bounding_enabled = false;
constexpr bool mouse_drawing_enabled = true;

constexpr float		relative_velocity = 0.1f;
constexpr float		relative_scale = 1.0f;

static float mouse_draw_radius = 30.0f;

static char title[64];

constexpr uint64_t	instance_count = (1 << 13);

#define Log(str) std::cout << str << std::endl

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT maessageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl << std::endl;

	return VK_FALSE;
}

namespace renderer
{
	namespace helper
	{
		struct QueueFamilyIndices
		{
			std::optional<uint32_t> graphics_family;
			std::optional<uint32_t> present_family;
			std::optional<uint32_t> compute_family;

			bool is_complete()
			{
				return graphics_family.has_value() && present_family.has_value() && compute_family.has_value();
			}
		};

		QueueFamilyIndices find_queue_family_indices(const VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface);

		uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties, VkPhysicalDevice physical_device);

		bool create_buffer(
			VkDevice device,
			VkPhysicalDevice physical_device,
			VkDeviceSize buffer_size,
			VkBufferUsageFlags usage,
			VkMemoryPropertyFlags memory_properties,
			VkBuffer& buffer,
			VkDeviceMemory& buffer_memory);

		bool copy_buffer(
			VkDevice device,
			VkCommandPool stage_command_pool,
			VkQueue queue,
			VkBuffer& src_buffer,
			VkBuffer& dst_buffer,
			VkDeviceSize buffer_size);

		VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code);
	};

	struct vertex
	{
		glm::vec2 pos;
		glm::vec3 color;
	};

	struct model
	{
		std::vector<vertex> vertices;
		std::vector<uint16_t> indices;
	};

	struct buffer
	{
		VkBuffer buffer;
		VkDeviceMemory device_memory;
		VkDeviceSize offset;
		VkDeviceSize size;

		VkDescriptorBufferInfo get_descriptor_info()
		{
			VkDescriptorBufferInfo info = {};
			info.buffer = this->buffer;
			info.offset = offset;
			info.range = size;
			return info;
		}

		void destroy(const VkDevice& device)
		{
			vkDestroyBuffer(device, this->buffer, nullptr);
			vkFreeMemory(device, this->device_memory, nullptr);
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	struct UniformBufferObject
	{
		glm::mat4 view;
		glm::mat4 proj;
	};

	inline void get_circle_model(const size_t& num_segments, model* model_out)
	{
		model_out->vertices.resize(num_segments + 1);
		model_out->indices.resize(num_segments * 3);

		float step = glm::two_pi<float>() / num_segments;

		model_out->vertices[0] = { {0.0f, 0.0f}, {0.5f, 0.6f, 0.7f} };

		for (size_t i = 1; i < num_segments + 1; ++i)
		{
			model_out->vertices[i] = { {glm::cos(i * step), glm::sin(i * step)}, {1.0f, 1.0f, 1.0f} };

			model_out->indices[i * 3 - 3] = 0;
			model_out->indices[i * 3 - 2] = i;
			model_out->indices[i * 3 - 1] = i + 1;
		}

		model_out->indices[model_out->indices.size() - 1] = 1;
	}

}