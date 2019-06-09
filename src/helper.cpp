#include "common.hpp"

namespace renderer
{
	namespace helper
	{
		void print_queue_flags(const int& index, const VkQueueFlags& queue_flags)
		{
			std::cout << "Queue Index " << index << " :" << std::endl;

			if (queue_flags & VK_QUEUE_GRAPHICS_BIT)
				std::cout << "\tVK_QUEUE_GRAPHICS_BIT" << std::endl;
			if (queue_flags & VK_QUEUE_COMPUTE_BIT)
				std::cout << "\tVK_QUEUE_COMPUTE_BIT" << std::endl;
			if (queue_flags & VK_QUEUE_TRANSFER_BIT)
				std::cout << "\tVK_QUEUE_TRANSFER_BIT" << std::endl;
			if (queue_flags & VK_QUEUE_SPARSE_BINDING_BIT)
				std::cout << "\tVK_QUEUE_SPARSE_BINDING_BIT" << std::endl;
			if (queue_flags & VK_QUEUE_PROTECTED_BIT)
				std::cout << "\tVK_QUEUE_PROTECTED_BIT" << std::endl;
		}

		QueueFamilyIndices find_queue_family_indices(const VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface)
		{
			QueueFamilyIndices indices;

			uint32_t queue_family_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

			std::cout << "\n\nQueue Families Count = " << queue_family_count << std::endl;

			std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());


			// IDEAL = Find a queue that only handled compute workloads
			VkQueueFlags compute_family_index = VK_QUEUE_FLAG_BITS_MAX_ENUM;
			for (auto i = 0; i < queue_families.size(); ++i)
			{
				const auto& queue_familiy = queue_families[i];
				print_queue_flags(i, queue_familiy.queueFlags);

				if (queue_familiy.queueCount > 0 && (queue_familiy.queueFlags & VK_QUEUE_COMPUTE_BIT))
				{
					if (queue_familiy.queueFlags < compute_family_index)
						compute_family_index = i;
				}
			}
			indices.compute_family = compute_family_index;

			for (auto i = 0; i < queue_families.size(); ++i)
			{
				const auto& queue_familiy = queue_families[i];

				if (queue_familiy.queueCount > 0 && (queue_familiy.queueFlags & VK_QUEUE_GRAPHICS_BIT))
				{
					indices.graphics_family = i;
				}

				VkBool32 present_support = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);

				if (queue_familiy.queueCount > 0 && present_support)
				{
					indices.present_family = i;
				}

				if (indices.is_complete())
					break;
			}

			return indices;
		}

		uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties, VkPhysicalDevice physical_device)
		{
			VkPhysicalDeviceMemoryProperties mem_properties;
			vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

			for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i)
			{
				if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}

			return 0;
		}

		bool create_buffer(
			VkDevice device,
			VkPhysicalDevice physical_device,
			VkDeviceSize buffer_size,
			VkBufferUsageFlags usage,
			VkMemoryPropertyFlags memory_properties,
			VkBuffer& buffer,
			VkDeviceMemory& buffer_memory)
		{
			VkBufferCreateInfo  buffer_info = {};

			buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_info.size = buffer_size;
			buffer_info.usage = usage;

			if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS)
			{
				return false;
			}

			VkMemoryRequirements memory_requirements;
			vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

			alloc_info.memoryTypeIndex = find_memory_type(
				memory_requirements.memoryTypeBits,
				memory_properties, physical_device);

			alloc_info.allocationSize = memory_requirements.size;

			if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS)
			{
				return false;
			}

			if (vkBindBufferMemory(device, buffer, buffer_memory, 0) != VK_SUCCESS)
			{
				return false;
			}

			return true;
		}

		bool copy_buffer(
			VkDevice device,
			VkCommandPool stage_command_pool,
			VkQueue queue,
			VkBuffer& src_buffer,
			VkBuffer& dst_buffer,
			VkDeviceSize buffer_size)
		{
			VkCommandBuffer command_buffer;

			VkCommandBufferAllocateInfo command_buffer_info = {};
			command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_info.commandPool = stage_command_pool;
			command_buffer_info.commandBufferCount = 1;

			if (vkAllocateCommandBuffers(device, &command_buffer_info, &command_buffer) != VK_SUCCESS)
			{
				return false;
			}

			// Record
			VkCommandBufferBeginInfo cmd_begin = {};
			cmd_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmd_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(command_buffer, &cmd_begin);

			VkBufferCopy copy_region = {};
			copy_region.srcOffset = 0;
			copy_region.dstOffset = 0;
			copy_region.size = buffer_size;
			vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

			vkEndCommandBuffer(command_buffer);

			// Submit
			VkSubmitInfo cmd_submit_info = {};
			cmd_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			cmd_submit_info.commandBufferCount = 1;
			cmd_submit_info.pCommandBuffers = &command_buffer;
			vkQueueSubmit(queue, 1, &cmd_submit_info, nullptr);
			vkQueueWaitIdle(queue);

			// Free
			vkFreeCommandBuffers(device, stage_command_pool, 1, &command_buffer);

			return true;
		}

		VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code)
		{
			VkShaderModuleCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.codeSize = code.size();
			create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

			VkShaderModule shader_module;

			if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
				log("Shader Coudn't be created");

			return shader_module;
		}
	}
}
