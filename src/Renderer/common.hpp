#pragma once

#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

constexpr int INIT_WIDTH = 1280;
constexpr int INIT_HEIGHT = 720;

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