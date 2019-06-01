#pragma once

#include "common.hpp"

// Selected Functions from SaschaWilliens' Example vkinitializers.hpp

namespace initializers
{
	inline VkVertexInputBindingDescription vertex_input_binding_description(
		uint32_t binding,
		uint32_t stride,
		VkVertexInputRate inputRate)
	{
		VkVertexInputBindingDescription vInputBindDescription{};
		vInputBindDescription.binding = binding;
		vInputBindDescription.stride = stride;
		vInputBindDescription.inputRate = inputRate;
		return vInputBindDescription;
	}

	inline VkVertexInputAttributeDescription vertex_input_attribute_description(
		uint32_t binding,
		uint32_t location,
		VkFormat format,
		uint32_t offset)
	{
		VkVertexInputAttributeDescription vInputAttribDescription{};
		vInputAttribDescription.location = location;
		vInputAttribDescription.binding = binding;
		vInputAttribDescription.format = format;
		vInputAttribDescription.offset = offset;
		return vInputAttribDescription;
	}

	inline VkWriteDescriptorSet write_descriptors_set(
		VkDescriptorSet dstSet,
		VkDescriptorType type,
		uint32_t binding,
		VkDescriptorBufferInfo* bufferInfo,
		uint32_t descriptorCount = 1)
	{
		VkWriteDescriptorSet write_descriptors_set{};
		write_descriptors_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptors_set.dstSet = dstSet;
		write_descriptors_set.descriptorType = type;
		write_descriptors_set.dstBinding = binding;
		write_descriptors_set.pBufferInfo = bufferInfo;
		write_descriptors_set.descriptorCount = descriptorCount;
		return write_descriptors_set;
	}
}