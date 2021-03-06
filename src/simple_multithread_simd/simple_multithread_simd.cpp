#include "simple_multithread_simd.h"

#include <set>
#include <fstream>
#include <algorithm>
#include <stdio.h>
#include <atomic>
#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>

#define VERTEX_BUFFER_BIND_ID				0 // PER VERTEX
#define COLOR_BUFFER_BIND_ID				1 // PER INSTANCE
#define XPOSITIONS_BUFFER_BIND_ID			2 // PER INSTANCE
#define YPOSITIONS_BUFFER_BIND_ID			3 // PER INSTANCE
#define SCALE_BUFFER_BIND_ID				4 // PER INSTANCE

static int64_t sum_time = 0;
static size_t count_frames = 0;

// Multi Threading Constants

constexpr size_t num_pairs = vectors_size * (vectors_size - 1) / 2;
constexpr size_t num_threads = 8;
static_assert(num_threads <= vectors_size);

size_t checks_i[num_threads + 1];
size_t checks_j[num_threads + 1];

std::mutex							collided_mutex;

std::vector<std::pair<size, size>>	collided;
std::vector<__m256>					collided_mask;
std::vector<short>					permute;


using namespace std::chrono_literals;
namespace sync
{
	static std::atomic<bool> should_close = false;

	std::mutex					num_finished_mutex;
	std::atomic<size>			num_finished = num_threads;

	std::atomic<bool>			should_start[num_threads];
}

// SIMD Constants

#define SIMD

static __m256 zero = _mm256_set1_ps(0);
static __m256 one = _mm256_set1_ps(1);
static __m256 two = _mm256_set1_ps(2);
static __m256 five = _mm256_set1_ps(5);
static __m256 minus_two = _mm256_set1_ps(-2);
static __m256 width_vec = _mm256_set1_ps(screen_width);
static __m256 height_vec = _mm256_set1_ps(screen_height);

// 8x8 Latin Square
__m256i rows[8] =
{
	_mm256_setr_epi32(0,1,7,2,6,3,5,4),
	_mm256_setr_epi32(1,2,0,3,7,4,6,5),
	_mm256_setr_epi32(2,3,1,4,0,5,7,6),
	_mm256_setr_epi32(3,4,2,5,1,6,0,7),
	_mm256_setr_epi32(4,5,3,6,2,7,1,0),
	_mm256_setr_epi32(5,6,4,7,3,0,2,1),
	_mm256_setr_epi32(6,7,5,0,4,1,3,2),
	_mm256_setr_epi32(7,0,6,1,5,2,4,3)
};

// 8x8 Latin Square
static __m256i reverse_rows[8];

__m256 identity_rows[8] =
{
	_mm256_setr_ps(1,0,0,0,0,0,0,0),
	_mm256_setr_ps(0,1,0,0,0,0,0,0),
	_mm256_setr_ps(0,0,1,0,0,0,0,0),
	_mm256_setr_ps(0,0,0,1,0,0,0,0),
	_mm256_setr_ps(0,0,0,0,1,0,0,0),
	_mm256_setr_ps(0,0,0,0,0,1,0,0),
	_mm256_setr_ps(0,0,0,0,0,0,1,0),
	_mm256_setr_ps(0,0,0,0,0,0,0,1),
};

__m256i self_check_rows[4] =
{
	_mm256_setr_epi32(1,2,3,4,5,6,7,0),
	_mm256_setr_epi32(2,3,4,5,6,7,0,1),
	_mm256_setr_epi32(3,4,5,6,7,0,1,2),
	_mm256_setr_epi32(4,5,6,7,0,1,2,3),
};

__m256i reverse_self_check_rows[4];

static void generate_rows()
{
	for (short i = 0; i < 8; ++i)
	{
		identity_rows[i] = _mm256_cmp_ps(identity_rows[i], one, _CMP_EQ_OQ);
	}

	for (short i = 0; i < 8; ++i)
	{
		uint32_t current[8];
		uint32_t new_row[8];
		_mm256_storeu_si256((__m256i*)(current), rows[i]);
		for (size_t j = 0; j < 8; ++j)
		{
			new_row[current[j]] = j;
		}
		reverse_rows[i] = _mm256_loadu_si256((__m256i*)(new_row));
	}

	for (short i = 0; i < 4; ++i)
	{
		uint32_t current[8];
		uint32_t new_row[8];
		_mm256_storeu_si256((__m256i*)(current), self_check_rows[i]);
		for (size_t j = 0; j < 8; ++j)
		{
			new_row[current[j]] = j;
		}
		reverse_self_check_rows[i] = _mm256_loadu_si256((__m256i*)(new_row));
	}
}

inline float rand_vel_pre()
{
	return relative_velocity * (((rand() % 100) / 200.0f) * ((rand() % 2) * 2 - 1.0f)) * 3.0f;
}

inline __m256 rand_vel_vec(const __m256& scale)
{
	const __m256 pre = _mm256_setr_ps(
		rand_vel_pre(),
		rand_vel_pre(),
		rand_vel_pre(),
		rand_vel_pre(),
		rand_vel_pre(),
		rand_vel_pre(),
		rand_vel_pre(),
		rand_vel_pre());
	return _mm256_div_ps(pre, _mm256_sqrt_ps(scale));
}

inline float rand_pos(const int& max)
{
	return rand() % (max - 2);
}

inline __m256 rand_pos_vec(const __m256& scale, const int& max)
{
	__m256 add = _mm256_setr_ps(
		rand_pos(max),
		rand_pos(max),
		rand_pos(max),
		rand_pos(max),
		rand_pos(max),
		rand_pos(max),
		rand_pos(max),
		rand_pos(max));

	return _mm256_add_ps(scale, add);
}

inline float rand_scale()
{
	return min_size + (rand() % (max_size - min_size));
}

inline __m256 rand_scale_vec()
{
	return _mm256_setr_ps(
		rand_scale(),
		rand_scale(),
		rand_scale(),
		rand_scale(),
		rand_scale(),
		rand_scale(),
		rand_scale(),
		rand_scale()
	);
}


using namespace renderer;

const std::vector<const char*> required_validation_layers =
{
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> device_extensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	mouse_draw_radius += yoffset;

	if (mouse_draw_radius < 1)
		mouse_draw_radius = 1;
}

static void resize_callback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<CircleCollisionMultiThreadSIMD*>(glfwGetWindowUserPointer(window));
	app->window_resize();
}

static std::vector<char> read_file(const std::string& fileName)
{
	std::ifstream file(fileName, std::ios::ate | std::ios::binary);
	std::vector<char> buffer;

	if (!file.is_open())
		return buffer;

	size_t fileSize = (size_t)file.tellg();
	buffer.resize(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

void CircleCollisionMultiThreadSIMD::initialize()
{
	srand(time(NULL));
	validation_layers_enabled = false;
}

bool CircleCollisionMultiThreadSIMD::run()
{
	if (!setup_window())
		return false;

	if (!setup_vulkan())
		return false;

	if (!main_loop())
		return false;

	if (!release())
		return false;

	return true;
}

bool CircleCollisionMultiThreadSIMD::setup_window()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	this->window = glfwCreateWindow(screen_width, screen_height, "Vulkan-Learn-1", nullptr, nullptr);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetFramebufferSizeCallback(this->window, resize_callback);
	glfwSetWindowPos(this->window, 0, 50);
	glfwSetWindowUserPointer(this->window, this);

	return true;
}

bool CircleCollisionMultiThreadSIMD::setup_vulkan()
{
	if (!create_instance())
		return false;
	if (this->validation_layers_enabled && !set_up_debug_messenger())
		return false;
	if (!create_surface())
		return false;
	if (!pick_physical_device())
		return false;
	if (!create_logical_device())
		return false;
	if (!create_swap_chain())
		return false;
	if (!create_image_views())
		return false;
	if (!create_renderpass())
		return false;
	if (!set_viewport_scissor())
		return false;
	if (!create_descriptor_set_layout())
		return false;
	if (!create_graphics_pipeline())
		return false;
	if (!create_frame_buffers())
		return false;
	if (!create_command_pool())
		return false;
	if (!create_vertex_buffer())
		return false;
	if (!create_index_buffer())
		return false;
	if (!create_instance_buffers())
		return false;
	if (!create_uniform_buffers())
		return false;
	if (!create_descriptor_pool())
		return false;
	if (!create_descriptor_sets())
		return false;
	if (!create_command_buffers())
		return false;
	if (!create_sync_objects())
		return false;

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_instance()
{
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "vk-1";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Mir Engine";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

#pragma region validation layers

#ifdef _DEBUG
	uint32_t available_layer_count;
	vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);
	std::vector<VkLayerProperties> available_layers(available_layer_count);
	vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data());

	// check if extentions required by glfw is available
	if (required_validation_layers.size() > available_layer_count)
	{
		log("Validation Layers Not Supported");
		return false;
	}

	for (auto i = 0; i < required_validation_layers.size(); ++i)
	{
		bool found_layer = false;

		for (const auto& extension : available_layers)
		{
			if (strcmp(extension.layerName, required_validation_layers[i]) == 0)
				found_layer = true;
		}

		if (!found_layer)
		{
			log("Validation Layer << " << required_validation_layers[i] << " >> is not available");
			return false;
		}
	}

	this->validation_layers_enabled = true;
#else
	this->validation_layers_enabled = false;
#endif


#pragma endregion

#pragma region extention
	// check available extentions
	uint32_t available_extention_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &available_extention_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(available_extention_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &available_extention_count, available_extensions.data());

	log("available extensions" << "(" << available_extention_count << ") : ");

	for (const auto& extension : available_extensions) {
		log("\t" << extension.extensionName);
	}

	// Ok Let's Do It :(
	uint32_t glfw_extensions_count = 0;
	const char** glfw_extensions;

	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

	std::vector<const char*> required_extentions(glfw_extensions, glfw_extensions + glfw_extensions_count);

	if (validation_layers_enabled)
		required_extentions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// check if extentions required is available
	if (glfw_extensions_count > available_extention_count)
	{
		log("Extentions for glfw are not available");
		return false;
	}

	for (auto i = 0; i < glfw_extensions_count; ++i)
	{
		bool found_extention = false;

		for (const auto& extension : available_extensions)
		{
			if (strcmp(extension.extensionName, glfw_extensions[i]) == 0)
				found_extention = true;
		}

		if (!found_extention)
		{
			log("Extention << " << glfw_extensions[i] << " >> for glfw is not available");
			return false;
		}
	}
#pragma endregion

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = static_cast<uint32_t>(required_extentions.size());
	create_info.ppEnabledExtensionNames = required_extentions.data();
	// Validation layer 
	if (validation_layers_enabled)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers.size());
		create_info.ppEnabledLayerNames = required_validation_layers.data();
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	VkResult result = vkCreateInstance(&create_info, nullptr, &(this->instance));

	return result == VK_SUCCESS;
}

bool CircleCollisionMultiThreadSIMD::set_up_debug_messenger()
{
#if defined(_DEBUG)
	if (!validation_layers_enabled)
		return false;

	VkDebugUtilsMessengerCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	create_info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	create_info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	create_info.pfnUserCallback = debug_callback;
	create_info.pUserData = nullptr;

	auto CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (CreateDebugUtilsMessengerEXT != nullptr)
	{
		const auto result = CreateDebugUtilsMessengerEXT(this->instance, &create_info, nullptr, &this->debug_messenger);
		return result == VK_SUCCESS;
	}
	else
	{
		return false;
	}
#else
	return true;
#endif
}

bool CircleCollisionMultiThreadSIMD::pick_physical_device()
{
	uint32_t available_physical_devices_count = 0;
	vkEnumeratePhysicalDevices(this->instance, &available_physical_devices_count, nullptr);

	if (available_physical_devices_count == 0)
	{
		log("No Supported Vulkan GPU found.");
		return false;
	}

	std::vector<VkPhysicalDevice> available_physical_devices(available_physical_devices_count);
	vkEnumeratePhysicalDevices(this->instance, &available_physical_devices_count, available_physical_devices.data());

	auto selected_device = -1;

	for (auto i = 0; i < available_physical_devices_count; ++i)
	{
		const auto physical_device = available_physical_devices[i];
		VkPhysicalDeviceProperties device_properties;
		VkPhysicalDeviceFeatures device_features;
		vkGetPhysicalDeviceProperties(physical_device, &device_properties);
		vkGetPhysicalDeviceFeatures(physical_device, &device_features);

		//I just skip here since i only have Intel's GPU on Surface Pro 6 
		if (helper::find_queue_family_indices(physical_device, this->surface).is_complete())
		{
			selected_device = i;
			break;
		}
	}

	if (selected_device < 0)
	{
		log("No Suitable Physical Device Found.");
		return false;
	}

	this->physical_device = available_physical_devices[selected_device];

	return true;
}

bool CircleCollisionMultiThreadSIMD::check_device_extensions_support()
{
	uint32_t available_extensions_count;
	vkEnumerateDeviceExtensionProperties(this->physical_device, nullptr, &available_extensions_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(available_extensions_count);
	vkEnumerateDeviceExtensionProperties(this->physical_device, nullptr, &available_extensions_count, available_extensions.data());

	std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

	for (auto& extension : available_extensions)
		required_extensions.erase(extension.extensionName);

	return required_extensions.empty();
}

bool CircleCollisionMultiThreadSIMD::create_logical_device()
{
	if (!check_device_extensions_support())
		return false;

	this->family_indices = helper::find_queue_family_indices(this->physical_device, this->surface);
	std::set<uint32_t> unique_queue_families = { family_indices.graphics_family.value(), family_indices.present_family.value() };

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	float queue_priorities[1] = { 1.0f };

	for (auto queue_family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueCount = 1;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.pQueuePriorities = queue_priorities;
		queue_create_infos.push_back(queue_create_info);
	}

	// nothing for now
	VkPhysicalDeviceFeatures device_features = {};

	VkDeviceCreateInfo  create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	create_info.ppEnabledExtensionNames = device_extensions.data();

	if (validation_layers_enabled)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers.size());
		create_info.ppEnabledLayerNames = required_validation_layers.data();
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	auto result = vkCreateDevice(this->physical_device, &create_info, nullptr, &this->device);

	vkGetDeviceQueue(device, family_indices.graphics_family.value(), 0, &graphics_queue);
	vkGetDeviceQueue(device, family_indices.present_family.value(), 0, &present_queue);

	return result == VK_SUCCESS;
}

bool CircleCollisionMultiThreadSIMD::create_surface()
{
	return glfwCreateWindowSurface(this->instance, this->window, nullptr, &this->surface) == VK_SUCCESS;
}

bool CircleCollisionMultiThreadSIMD::create_swap_chain()
{
	// Get Properties

	SwapChainSupportDetails properties;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->physical_device, this->surface, &properties.capabilities);

	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(this->physical_device, this->surface, &format_count, nullptr);

	if (format_count > 0)
	{
		properties.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(this->physical_device, this->surface, &format_count, properties.formats.data());
	}

	uint32_t present_modes_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(this->physical_device, this->surface, &present_modes_count, nullptr);

	if (present_modes_count > 0)
	{
		properties.present_modes.resize(present_modes_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(this->physical_device, this->surface, &format_count, properties.present_modes.data());
	}

	// Choose Best Config

	if (properties.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		this->swap_chain_extent = properties.capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetWindowSize(this->window, &width, &height);

		this->swap_chain_extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		this->swap_chain_extent.width = std::max(properties.capabilities.maxImageExtent.width, std::min(this->swap_chain_extent.width, properties.capabilities.minImageExtent.width));
		this->swap_chain_extent.height = std::max(properties.capabilities.maxImageExtent.height, std::min(this->swap_chain_extent.height, properties.capabilities.minImageExtent.height));
	}

	if (properties.formats.empty())
	{
		log("No Format Exists for this Physical Device");
		return false;
	}


	VkSurfaceFormatKHR selected_format = properties.formats[0];

	if (selected_format.format == VK_FORMAT_UNDEFINED)
	{
		// Best Case
		selected_format = { VK_FORMAT_B8G8R8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}
	else
	{
		for (const auto& format : properties.formats)
		{
			if (format.format == VK_FORMAT_B8G8R8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				selected_format = format;
				break;
			}
		}
	}

	this->swap_chain_image_format = selected_format.format;



	VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& available_present_mode : properties.present_modes)
	{
		if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			selected_present_mode = available_present_mode;
			break;
		}
		else if (available_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			selected_present_mode = available_present_mode;
		}
	}

	uint32_t min_image_count = properties.capabilities.minImageCount + 1;
	if (min_image_count > properties.capabilities.maxImageCount)
		min_image_count = properties.capabilities.maxImageCount;

	// Create Swap Chain
	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.imageExtent = this->swap_chain_extent;
	create_info.imageArrayLayers = 1;
	create_info.imageFormat = selected_format.format;
	create_info.imageColorSpace = selected_format.colorSpace;
	create_info.presentMode = selected_present_mode;
	create_info.minImageCount = min_image_count;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (this->family_indices.graphics_family != family_indices.present_family)
	{
		uint32_t queue_family_indices[] = { this->family_indices.graphics_family.value(), family_indices.present_family.value() };

		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = 0;
	}

	create_info.preTransform = properties.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;
	create_info.surface = this->surface;

	if (vkCreateSwapchainKHR(this->device, &create_info, nullptr, &this->swap_chain) != VK_SUCCESS)
		return false;

	uint32_t images_count;
	vkGetSwapchainImagesKHR(this->device, this->swap_chain, &images_count, nullptr);
	this->swap_chain_images.resize(images_count);
	vkGetSwapchainImagesKHR(this->device, this->swap_chain, &images_count, this->swap_chain_images.data());

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_image_views()
{
	this->swap_chain_image_views.resize(this->swap_chain_images.size());

	for (auto i = 0; i < this->swap_chain_image_views.size(); ++i)
	{
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.format = this->swap_chain_image_format;
		create_info.image = this->swap_chain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(this->device, &create_info, nullptr, &this->swap_chain_image_views[i]) != VK_SUCCESS)
			return false;
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_renderpass()
{
	// Graphics Subpass
	VkAttachmentReference color_attach_ref = {};
	color_attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_attach_ref.attachment = 0;

	VkSubpassDescription subpass_description = {};
	subpass_description.colorAttachmentCount = 1;
	subpass_description.pColorAttachments = &color_attach_ref;
	subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	// Render Pass Color Attachment
	VkAttachmentDescription color_attachement = {};
	color_attachement.format = this->swap_chain_image_format;
	color_attachement.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachement.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	color_attachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// Render Pass
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachement;
	render_pass_info.subpassCount = 1;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;
	render_pass_info.pSubpasses = &subpass_description;

	if (vkCreateRenderPass(this->device, &render_pass_info, nullptr, &this->render_pass) != VK_SUCCESS)
	{
		log("Create Render Pass Failed.");
		return false;
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_descriptor_set_layout()
{
	VkDescriptorSetLayoutBinding descriptor_set_binding = {};
	descriptor_set_binding.binding = 0;
	descriptor_set_binding.descriptorCount = 1;
	descriptor_set_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_set_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = &descriptor_set_binding;
	//descriptor_set_info.flags = 

	if (vkCreateDescriptorSetLayout(this->device, &layout_info, nullptr, &this->ubo_descriptor_set_layout) != VK_SUCCESS)
		return false;

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_graphics_pipeline()
{
	std::string path = files::get_app_path();

	auto vert_shader = read_file(path + "\\..\\..\\..\\..\\..\\src\\simple_multithread_simd\\shaders\\shaders.vert.spv");
	auto frag_shader = read_file(path + "\\..\\..\\..\\..\\..\\src\\simple_multithread_simd\\shaders\\shaders.frag.spv");

	if (vert_shader.empty() || frag_shader.empty())
	{
		log("Make sure shaders are correctly read from file.");
		return false;
	}
	VkShaderModule vert_shader_module = helper::create_shader_module(this->device, vert_shader);
	VkShaderModule frag_shader_module = helper::create_shader_module(this->device, frag_shader);

	// Shaders
	VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
	vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_stage_info.module = vert_shader_module;
	vert_shader_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
	frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_stage_info.module = frag_shader_module;
	frag_shader_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

	std::vector<VkVertexInputBindingDescription> bindings =
	{
		initializers::vertex_input_binding_description(VERTEX_BUFFER_BIND_ID, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		initializers::vertex_input_binding_description(COLOR_BUFFER_BIND_ID, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_INSTANCE),
		initializers::vertex_input_binding_description(XPOSITIONS_BUFFER_BIND_ID, sizeof(float), VK_VERTEX_INPUT_RATE_INSTANCE),
		initializers::vertex_input_binding_description(YPOSITIONS_BUFFER_BIND_ID, sizeof(float), VK_VERTEX_INPUT_RATE_INSTANCE),
		initializers::vertex_input_binding_description(SCALE_BUFFER_BIND_ID, sizeof(float), VK_VERTEX_INPUT_RATE_INSTANCE),
	};

	std::vector<VkVertexInputAttributeDescription> attributes =
	{
		initializers::vertex_input_attribute_description(VERTEX_BUFFER_BIND_ID,		0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, pos)),
		initializers::vertex_input_attribute_description(XPOSITIONS_BUFFER_BIND_ID,	1, VK_FORMAT_R32_SFLOAT, 0),
		initializers::vertex_input_attribute_description(YPOSITIONS_BUFFER_BIND_ID,	2, VK_FORMAT_R32_SFLOAT, 0),
		initializers::vertex_input_attribute_description(COLOR_BUFFER_BIND_ID,		3, VK_FORMAT_R32G32B32_SFLOAT, 0),
		initializers::vertex_input_attribute_description(SCALE_BUFFER_BIND_ID,		4, VK_FORMAT_R32_SFLOAT, 0),
	};

	// VI
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
	vertexInputInfo.pVertexBindingDescriptions = bindings.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

	// IA
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &this->viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &this->scissor;

	// RS
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.lineWidth = 1.0f;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //VK_FRONT_FACE_COUNTER_CLOCKWISE //VK_FRONT_FACE_CLOCKWISE
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// MS
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// CB
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	// Pipeline 

	// Pipeline Layout : Q : Why layout should be created and what is it's usage?
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &this->ubo_descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(this->device, &pipeline_layout_info, nullptr, &this->pipeline_layout) != VK_SUCCESS)
	{
		log("Create Pipeline Layout Failed.");

		vkDestroyShaderModule(this->device, vert_shader_module, nullptr);
		vkDestroyShaderModule(this->device, frag_shader_module, nullptr);

		return false;
	}

	// Dynamic States
	VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.dynamicStateCount = 2;
	dynamic_state_info.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo pipeline_create_info = {};
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.stageCount = 2;
	pipeline_create_info.pStages = shader_stages;
	pipeline_create_info.pVertexInputState = &vertexInputInfo;
	pipeline_create_info.pInputAssemblyState = &inputAssembly;
	pipeline_create_info.pViewportState = &viewportState;
	pipeline_create_info.pRasterizationState = &rasterizer;
	pipeline_create_info.pMultisampleState = &multisampling;
	pipeline_create_info.pColorBlendState = &colorBlending;
	pipeline_create_info.layout = this->pipeline_layout;
	pipeline_create_info.pDepthStencilState = nullptr;
	pipeline_create_info.pDynamicState = &dynamic_state_info;
	pipeline_create_info.renderPass = this->render_pass;
	pipeline_create_info.subpass = 0;

	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(
		this->device,
		VK_NULL_HANDLE,
		1,
		&pipeline_create_info,
		nullptr,
		&this->graphics_pipeline) != VK_SUCCESS)
	{
		log("Create Pipeline Failed.");

		free(shader_stages);
		vkDestroyShaderModule(this->device, vert_shader_module, nullptr);
		vkDestroyShaderModule(this->device, frag_shader_module, nullptr);

		return false;
	}

	vkDestroyShaderModule(this->device, vert_shader_module, nullptr);
	vkDestroyShaderModule(this->device, frag_shader_module, nullptr);

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_vertex_buffer()
{
	get_circle_model(30, &this->circle_model);
	const VkDeviceSize buffer_size = sizeof(vertex) * this->circle_model.vertices.size();

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory))
	{
		return false;
	}

	void* data;
	vkMapMemory(this->device, staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, this->circle_model.vertices.data(), (size_t)buffer_size);
	vkUnmapMemory(this->device, staging_buffer_memory);

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		this->vertex_buffer,
		this->vertex_buffer_memory))
	{
		return false;
	}

	helper::copy_buffer(this->device, this->command_pool, this->graphics_queue, staging_buffer, this->vertex_buffer, buffer_size);

	vkDestroyBuffer(this->device, staging_buffer, nullptr);
	vkFreeMemory(this->device, staging_buffer_memory, nullptr);

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_index_buffer()
{
	const VkDeviceSize buffer_size = sizeof(uint16_t) * this->circle_model.indices.size();

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory))
	{
		return false;
	}

	void* data;
	vkMapMemory(this->device, staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, this->circle_model.indices.data(), (size_t)buffer_size);
	vkUnmapMemory(this->device, staging_buffer_memory);

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		this->index_buffer,
		this->index_buffer_memory))
	{
		return false;
	}

	helper::copy_buffer(this->device, this->command_pool, this->graphics_queue, staging_buffer, this->index_buffer, buffer_size);

	vkDestroyBuffer(this->device, staging_buffer, nullptr);
	vkFreeMemory(this->device, staging_buffer_memory, nullptr);

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_instance_buffers()
{
	setup_circles();

	if (!create_colors_buffer())
		return false;
	if (!create_positions_buffer())
		return false;
	if (!create_scales_buffer())
		return false;

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_uniform_buffers()
{
	const auto buffer_size = sizeof(UniformBufferObject);

	const auto size = this->swap_chain_images.size();
	this->ubo_buffers.resize(size);
	this->ubo_buffers_memory.resize(size);

	for (size_t i = 0; i < size; ++i)
	{
		if (!helper::create_buffer(
			this->device,
			this->physical_device,
			buffer_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			this->ubo_buffers[i],
			this->ubo_buffers_memory[i]))
		{
			return false;
		}
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_descriptor_pool()
{
	VkDescriptorPoolSize pool_size = {};
	pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_size.descriptorCount = static_cast<uint32_t>(this->swap_chain_images.size());

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	pool_info.maxSets = static_cast<uint32_t>(this->swap_chain_images.size());

	if (vkCreateDescriptorPool(this->device, &pool_info, nullptr, &this->ubo_descriptor_pool) != VK_SUCCESS)
	{
		return false;
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_descriptor_sets()
{
	std::vector<VkDescriptorSetLayout> layouts(swap_chain_images.size(), ubo_descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pSetLayouts = layouts.data();
	alloc_info.descriptorPool = this->ubo_descriptor_pool;
	alloc_info.descriptorSetCount = static_cast<uint32_t>(this->swap_chain_images.size());

	this->ubo_descriptor_sets.resize(swap_chain_images.size());

	if (vkAllocateDescriptorSets(this->device, &alloc_info, this->ubo_descriptor_sets.data()) != VK_SUCCESS)
		return false;

	for (size_t i = 0; i < this->swap_chain_images.size(); ++i)
	{
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferObject);
		buffer_info.buffer = this->ubo_buffers[i];

		VkWriteDescriptorSet desc_write = {};
		desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc_write.dstBinding = 0;
		desc_write.descriptorCount = 1;
		desc_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		desc_write.dstSet = this->ubo_descriptor_sets[i];
		desc_write.pBufferInfo = &buffer_info;
		desc_write.dstArrayElement = 0;

		vkUpdateDescriptorSets(this->device, 1, &desc_write, 0, nullptr);
	}
}

bool CircleCollisionMultiThreadSIMD::create_frame_buffers()
{
	this->swap_chain_frame_buffers.resize(this->swap_chain_image_views.size());

	for (auto i = 0; i < this->swap_chain_frame_buffers.size(); ++i)
	{
		VkImageView attachments[] =
		{
			this->swap_chain_image_views[i]
		};

		VkFramebufferCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		create_info.attachmentCount = 1;
		create_info.width = this->swap_chain_extent.width;
		create_info.height = this->swap_chain_extent.height;
		create_info.pAttachments = attachments;
		create_info.renderPass = this->render_pass;
		create_info.layers = 1;

		if (vkCreateFramebuffer(this->device, &create_info, nullptr, &this->swap_chain_frame_buffers[i]) != VK_SUCCESS)
		{
			log("Couldn't Create Frame Buffer, " << i);
			return false;
		}
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_command_pool()
{
	VkCommandPoolCreateInfo create_info = {};

	create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	create_info.queueFamilyIndex = this->family_indices.graphics_family.value();
	create_info.flags = 0;

	if (vkCreateCommandPool(this->device, &create_info, nullptr, &this->command_pool) != VK_SUCCESS)
	{
		log("Coudn't Create Command Pool");
		return false;
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_command_buffers()
{
	this->command_buffers.resize(this->swap_chain_frame_buffers.size());

	VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {};
	cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_buffer_alloc_info.commandBufferCount = (uint32_t)this->command_buffers.size();
	cmd_buffer_alloc_info.commandPool = this->command_pool;
	cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	if (vkAllocateCommandBuffers(this->device, &cmd_buffer_alloc_info, this->command_buffers.data()) != VK_SUCCESS)
	{
		log("Couldn't Allocate Command Buffers");
		return false;
	}

	for (auto i = 0; i < this->command_buffers.size(); ++i)
	{
		VkCommandBufferBeginInfo command_buffer_begin_info = {};
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		command_buffer_begin_info.pInheritanceInfo = nullptr; // all are primary now

		if (vkBeginCommandBuffer(this->command_buffers[i], &command_buffer_begin_info) != VK_SUCCESS)
		{
			log("Coudn't Begin Command Buffer");
			return false;
		}

		VkRenderPassBeginInfo render_pass_begin_info = {};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.clearValueCount = 1;
		VkClearValue clear_color = { 0.01f, 0.01f, 0.01f, 1.0f };
		render_pass_begin_info.pClearValues = &clear_color;
		render_pass_begin_info.renderPass = this->render_pass;
		render_pass_begin_info.framebuffer = this->swap_chain_frame_buffers[i];
		render_pass_begin_info.renderArea.extent = this->swap_chain_extent;
		render_pass_begin_info.renderArea.offset = { 0, 0 };

		vkCmdBeginRenderPass(this->command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdBindPipeline(this->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphics_pipeline);
			vkCmdSetViewport(this->command_buffers[i], 0, 1, &this->viewport);
			vkCmdSetScissor(this->command_buffers[i], 0, 1, &this->scissor);

			VkBuffer vertex_buffers[] = { this->vertex_buffer };
			VkBuffer colors_buffers[] = { this->colors_buffer };
			VkBuffer x_positions_buffers[] = { this->x_positions_buffer };
			VkBuffer y_positions_buffers[] = { this->y_positions_buffer };
			VkBuffer scales_buffers[] = { this->scales_buffer };
			VkDeviceSize offsets[] = { 0 };

			// Circles
			vkCmdBindDescriptorSets(this->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline_layout, 0, 1, &this->ubo_descriptor_sets[i], 0, nullptr);

			vkCmdBindVertexBuffers(this->command_buffers[i], VERTEX_BUFFER_BIND_ID, 1, vertex_buffers, offsets);

			vkCmdBindVertexBuffers(this->command_buffers[i], COLOR_BUFFER_BIND_ID, 1, colors_buffers, offsets);

			vkCmdBindVertexBuffers(this->command_buffers[i], XPOSITIONS_BUFFER_BIND_ID, 1, x_positions_buffers, offsets);

			vkCmdBindVertexBuffers(this->command_buffers[i], YPOSITIONS_BUFFER_BIND_ID, 1, y_positions_buffers, offsets);

			vkCmdBindVertexBuffers(this->command_buffers[i], SCALE_BUFFER_BIND_ID, 1, scales_buffers, offsets);

			vkCmdBindIndexBuffer(this->command_buffers[i], this->index_buffer, 0, VK_INDEX_TYPE_UINT16);

			vkCmdDrawIndexed(this->command_buffers[i], static_cast<uint32_t>(this->circle_model.indices.size()), instance_count, 0, 0, 0);
		}
		vkCmdEndRenderPass(this->command_buffers[i]);

		if (vkEndCommandBuffer(this->command_buffers[i]) != VK_SUCCESS)
		{
			log("vkEndCommandBuffer Failed.");
			return false;
		}
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_sync_objects()
{
	this->num_frames = this->swap_chain_images.size();

	this->image_available_semaphore.resize(this->num_frames);
	this->render_finished_semaphore.resize(this->num_frames);
	this->draw_fences.resize(this->num_frames);

	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;


	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (auto i = 0; i < this->num_frames; ++i)
	{
		if (vkCreateSemaphore(this->device, &semaphore_info, nullptr, &this->image_available_semaphore[i]) != VK_SUCCESS
			|| vkCreateSemaphore(this->device, &semaphore_info, nullptr, &this->render_finished_semaphore[i]) != VK_SUCCESS
			|| vkCreateFence(this->device, &fence_info, nullptr, &this->draw_fences[i]) != VK_SUCCESS)
		{
			log("Couldn't Create Semaphores.");
			return false;
		}
	}

	return true;
}

bool CircleCollisionMultiThreadSIMD::cleanup_swap_chain()
{
	for (auto& frame_buffer : this->swap_chain_frame_buffers)
		vkDestroyFramebuffer(this->device, frame_buffer, nullptr);

	vkFreeCommandBuffers(this->device, this->command_pool, this->num_frames, this->command_buffers.data());

	vkDestroyRenderPass(this->device, this->render_pass, nullptr);

	for (auto& image_view : this->swap_chain_image_views)
		vkDestroyImageView(this->device, image_view, nullptr);

	vkDestroySwapchainKHR(this->device, this->swap_chain, nullptr);

	for (size_t i = 0; i < this->swap_chain_images.size(); ++i)
	{
		vkDestroyBuffer(this->device, this->ubo_buffers[i], nullptr);
		vkFreeMemory(this->device, this->ubo_buffers_memory[i], nullptr);
	}

	vkDestroyDescriptorPool(this->device, this->ubo_descriptor_pool, nullptr);

	return true;
}

bool CircleCollisionMultiThreadSIMD::recreate_swap_chain()
{
	vkDeviceWaitIdle(this->device);

	int width = 0, height = 0;
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(this->window, &width, &height);
		glfwWaitEvents();
	}

	if (!cleanup_swap_chain())
		return false;

	if (!create_swap_chain())
		return false;
	if (!create_image_views())
		return false;
	if (!create_renderpass())
		return false;
	if (!set_viewport_scissor())
		return false;
	if (!create_frame_buffers())
		return false;
	if (!create_uniform_buffers())
		return false;
	if (!create_descriptor_pool())
		return false;
	if (!create_descriptor_sets())
		return false;
	if (!create_command_buffers())
		return false;

	return true;
}

bool CircleCollisionMultiThreadSIMD::set_viewport_scissor()
{
	this->viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)this->swap_chain_extent.width;
	viewport.height = (float)this->swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	this->scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = this->swap_chain_extent;

	return true;
}

template <class V, class T>
inline void print_vec(V vec)
{
	T* vecf = reinterpret_cast<T*>(&vec);

	for (short i = 0; i < 8; ++i)
		std::cout << "(" << i << "): " << vecf[i] << std::endl;

	std::cout << std::endl;
}

void CircleCollisionMultiThreadSIMD::update(const uint32_t& current_image)
{
	static auto start_time = std::chrono::high_resolution_clock::now();

	auto now = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(now - start_time).count();

	void* data;

	bool draw = false;

	int mouse_state = glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_LEFT);

	glm::vec2 mouse_pos;

	if (mouse_state == GLFW_PRESS)
	{
		draw = true;
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		mouse_pos = glm::vec2(xpos, screen_height - ypos);
	}

	const auto right_wall = (mouse_bounding_enabled && draw) ? mouse_pos.x : screen_width;
	const auto bottom_wall = (mouse_bounding_enabled && draw) ? mouse_pos.y : screen_height;

	const __m256 frame_time_vec = _mm256_set1_ps(this->frame_timer);

	{
		std::unique_lock<std::mutex> lock(collided_mutex);
		collided.clear();
		permute.clear();
		collided_mask.clear();
	}

	// Signal Collision Detection Threads to Start
	{
		for (size_t i = 0; i < num_threads; ++i)
		{
			sync::should_start[i] = true;
		}
	}

	for (size_t i = 0; i < vectors_size; ++i)
	{
		this->circles.x_positions[i] = _mm256_add_ps(
			this->circles.x_positions[i],
			_mm256_mul_ps(this->circles.x_velocities[i], frame_time_vec));
		this->circles.y_positions[i] = _mm256_add_ps(
			this->circles.y_positions[i],
			_mm256_mul_ps(this->circles.y_velocities[i], frame_time_vec));
	}

	const auto t1 = std::chrono::high_resolution_clock::now();

	for (size_t i = 0; i < vectors_size; ++i)
	{
		// Left
		const __m256 left = _mm256_sub_ps(this->circles.x_positions[i], this->circles.scales[i]);
		__m256 cmp = _mm256_cmp_ps(left, zero, _CMP_LE_OQ);
		if (_mm_popcnt_u32(_mm256_movemask_ps(cmp)) > 0)
		{
			__m256 clamped = _mm256_min_ps(cmp, _mm256_set1_ps(1));
			__m256 inverse = _mm256_sub_ps(one, clamped);
			__m256 mul = _mm256_fmadd_ps(clamped, minus_two, one);
			this->circles.x_velocities[i] = _mm256_mul_ps(this->circles.x_velocities[i], mul);
			this->circles.x_positions[i] = _mm256_add_ps(_mm256_mul_ps(inverse, this->circles.x_positions[i]), _mm256_mul_ps(clamped, this->circles.scales[i]));
		}

		// Right
		const __m256 right = _mm256_add_ps(this->circles.x_positions[i], this->circles.scales[i]);
		cmp = _mm256_cmp_ps(right, width_vec, _CMP_GE_OQ);
		if (_mm_popcnt_u32(_mm256_movemask_ps(cmp)) > 0)
		{
			__m256 clamped = _mm256_min_ps(cmp, _mm256_set1_ps(1));
			__m256 inverse = _mm256_sub_ps(one, clamped);
			__m256 mul = _mm256_fmadd_ps(clamped, minus_two, one);
			this->circles.x_velocities[i] = _mm256_mul_ps(this->circles.x_velocities[i], mul);
			this->circles.x_positions[i] = _mm256_add_ps(_mm256_mul_ps(inverse, this->circles.x_positions[i]), _mm256_mul_ps(clamped, _mm256_sub_ps(width_vec, this->circles.scales[i])));
		}

		// Bottom
		const __m256 bottom = _mm256_sub_ps(this->circles.y_positions[i], this->circles.scales[i]);
		cmp = _mm256_cmp_ps(bottom, zero, _CMP_LE_OQ);
		if (_mm_popcnt_u32(_mm256_movemask_ps(cmp)) > 0)
		{
			__m256 clamped = _mm256_min_ps(cmp, _mm256_set1_ps(1));
			__m256 inverse = _mm256_sub_ps(one, clamped);
			__m256 mul = _mm256_fmadd_ps(clamped, minus_two, one);
			this->circles.y_velocities[i] = _mm256_mul_ps(this->circles.y_velocities[i], mul);
			this->circles.y_positions[i] = _mm256_add_ps(_mm256_mul_ps(inverse, this->circles.y_positions[i]), _mm256_mul_ps(clamped, this->circles.scales[i]));
		}

		// Top
		const __m256 top = _mm256_add_ps(this->circles.y_positions[i], this->circles.scales[i]);
		cmp = _mm256_cmp_ps(top, height_vec, _CMP_GE_OQ);
		if (_mm_popcnt_u32(_mm256_movemask_ps(cmp)) > 0)
		{
			__m256 clamped = _mm256_min_ps(cmp, _mm256_set1_ps(1));
			__m256 inverse = _mm256_sub_ps(one, clamped);
			__m256 mul = _mm256_fmadd_ps(clamped, minus_two, one);
			this->circles.y_velocities[i] = _mm256_mul_ps(this->circles.y_velocities[i], mul);
			this->circles.y_positions[i] = _mm256_add_ps(_mm256_mul_ps(inverse, this->circles.y_positions[i]), _mm256_mul_ps(clamped, _mm256_sub_ps(height_vec, this->circles.scales[i])));
		}
	}

	const auto t2 = std::chrono::high_resolution_clock::now();

	// Wait For Collision Detection Work
	while (sync::num_finished.load() < num_threads)
	{
		if (sync::should_close)
			break;
		//std::this_thread::sleep_for(1ms);
	}
	sync::num_finished = 0;

	// Handle Collision
	for (size m = 0; m < collided.size(); ++m)
	{
		const size i = collided[m].first;
		const size j = collided[m].second;

		if (i == j)
		{
			const short per = permute[m];
			const __m256 compare = collided_mask[m];

			__m256& x1 = circles.x_positions[i];
			const __m256 x2 = _mm256_permutevar8x32_ps(x1, self_check_rows[per]);
			const __m256 dx = _mm256_sub_ps(x2, x1);

			__m256& y1 = circles.y_positions[i];
			const __m256 y2 = _mm256_permutevar8x32_ps(y1, self_check_rows[per]);
			const __m256 dy = _mm256_sub_ps(y2, y1);

			__m256& s1 = circles.scales[i];
			const __m256 s2 = _mm256_permutevar8x32_ps(s1, self_check_rows[per]);

			const __m256 radii = _mm256_add_ps(s1, s2);

			// Move Away
			const __m256 dis = _mm256_sqrt_ps(_mm256_fmadd_ps(dy, dy, _mm256_mul_ps(dx, dx)));
			const __m256 covered_by_2 = _mm256_div_ps(_mm256_sub_ps(radii, dis), two);

			// Perp Vec
			const __m256 n_x = _mm256_div_ps(dx, dis);
			const __m256 n_y = _mm256_div_ps(dy, dis);

			const __m256 move_x = _mm256_blendv_ps(zero, _mm256_mul_ps(n_x, covered_by_2), compare);
			const __m256 move_y = _mm256_blendv_ps(zero, _mm256_mul_ps(n_y, covered_by_2), compare);

			circles.x_positions[i] = _mm256_sub_ps(circles.x_positions[i], move_x);
			circles.y_positions[i] = _mm256_sub_ps(circles.y_positions[i], move_y);
			circles.x_positions[i] = _mm256_add_ps(circles.x_positions[i], _mm256_permutevar8x32_ps(move_x, reverse_self_check_rows[per]));
			circles.y_positions[i] = _mm256_add_ps(circles.y_positions[i], _mm256_permutevar8x32_ps(move_y, reverse_self_check_rows[per]));

			// Change Velocity Direction
			for (short h = 0; h < 8; ++h)
			{
				const __m256 cmp = _mm256_blendv_ps(zero, compare, identity_rows[h]);

				if (_mm256_movemask_ps(cmp) == 0)
					continue;

				const __m256 v2_x = _mm256_permutevar8x32_ps(this->circles.x_velocities[i], self_check_rows[per]);
				const __m256 v2_y = _mm256_permutevar8x32_ps(this->circles.y_velocities[i], self_check_rows[per]);

				const __m256 m1 = _mm256_mul_ps(this->circles.scales[i], one);
				const __m256 m2 = _mm256_mul_ps(s2, one);
				const __m256 sum_mass = _mm256_add_ps(m1, m2);

				const __m256 dot1 = _mm256_fmadd_ps(n_x, this->circles.x_velocities[i], _mm256_mul_ps(n_y, this->circles.y_velocities[i]));
				const __m256 dot2 = _mm256_fmadd_ps(n_x, v2_x, _mm256_mul_ps(n_y, v2_y));

				const __m256 p = _mm256_div_ps(_mm256_mul_ps(_mm256_sub_ps(dot1, dot2), two), sum_mass);

				const __m256 mvel1_x = _mm256_blendv_ps(zero, _mm256_mul_ps(p, _mm256_mul_ps(m2, n_x)), cmp);
				const __m256 mvel1_y = _mm256_blendv_ps(zero, _mm256_mul_ps(p, _mm256_mul_ps(m2, n_y)), cmp);
				const __m256 mvel2_x = _mm256_blendv_ps(zero, _mm256_mul_ps(p, _mm256_mul_ps(m1, n_x)), cmp);
				const __m256 mvel2_y = _mm256_blendv_ps(zero, _mm256_mul_ps(p, _mm256_mul_ps(m1, n_y)), cmp);

				this->circles.x_velocities[i] = _mm256_sub_ps(this->circles.x_velocities[i], mvel1_x);
				this->circles.y_velocities[i] = _mm256_sub_ps(this->circles.y_velocities[i], mvel1_y);

				this->circles.x_velocities[i] = _mm256_add_ps(this->circles.x_velocities[i], _mm256_permutevar8x32_ps(mvel2_x, reverse_self_check_rows[per]));
				this->circles.y_velocities[i] = _mm256_add_ps(this->circles.y_velocities[i], _mm256_permutevar8x32_ps(mvel2_y, reverse_self_check_rows[per]));
			}
		}
		else
		{
			const short per = permute[m];
			const __m256 compare = collided_mask[m];

			const __m256 x1 = _mm256_permutevar8x32_ps(circles.x_positions[i], rows[per]);
			const __m256 y1 = _mm256_permutevar8x32_ps(circles.y_positions[i], rows[per]);
			const __m256 s1 = _mm256_permutevar8x32_ps(circles.scales[i], rows[per]);

			const __m256 dx = _mm256_sub_ps(x1, circles.x_positions[j]);
			const __m256 dy = _mm256_sub_ps(y1, circles.y_positions[j]);

			const __m256 radii = _mm256_add_ps(circles.scales[j], s1);

			// Move Away
			const __m256 dis = _mm256_sqrt_ps(_mm256_fmadd_ps(dy, dy, _mm256_mul_ps(dx, dx)));
			const __m256 covered_by_2 = _mm256_div_ps(_mm256_sub_ps(radii, dis), two);

			// Perp Vec
			const __m256 n_x = _mm256_div_ps(dx, dis);
			const __m256 n_y = _mm256_div_ps(dy, dis);

			const __m256 move_x = _mm256_blendv_ps(zero, _mm256_mul_ps(n_x, covered_by_2), compare);
			const __m256 move_y = _mm256_blendv_ps(zero, _mm256_mul_ps(n_y, covered_by_2), compare);

			circles.x_positions[j] = _mm256_sub_ps(circles.x_positions[j], move_x);
			circles.y_positions[j] = _mm256_sub_ps(circles.y_positions[j], move_y);
			circles.x_positions[i] = _mm256_add_ps(circles.x_positions[i], _mm256_permutevar8x32_ps(move_x, reverse_rows[per]));
			circles.y_positions[i] = _mm256_add_ps(circles.y_positions[i], _mm256_permutevar8x32_ps(move_y, reverse_rows[per]));

			// Change Velocity Direction
			const __m256 m1 = _mm256_mul_ps(s1, five);
			const __m256 m2 = _mm256_mul_ps(this->circles.scales[j], five);
			const __m256 sum_mass = _mm256_add_ps(m1, m2);

			const __m256 v1_x = _mm256_permutevar8x32_ps(this->circles.x_velocities[i], rows[per]);
			const __m256 v1_y = _mm256_permutevar8x32_ps(this->circles.y_velocities[i], rows[per]);

			const __m256 dot1_x = _mm256_mul_ps(n_x, v1_x);
			const __m256 dot1_y = _mm256_mul_ps(n_y, v1_y);
			const __m256 dot1 = _mm256_add_ps(dot1_x, dot1_y);

			const __m256 dot2_x = _mm256_mul_ps(n_x, this->circles.x_velocities[j]);
			const __m256 dot2_y = _mm256_mul_ps(n_y, this->circles.y_velocities[j]);
			const __m256 dot2 = _mm256_add_ps(dot2_x, dot2_y);

			const __m256 p = _mm256_mul_ps(_mm256_div_ps(two, sum_mass), _mm256_sub_ps(dot1, dot2));

			const __m256 mvel1_x = _mm256_blendv_ps(zero, _mm256_mul_ps(n_x, _mm256_mul_ps(m2, p)), compare);
			const __m256 mvel1_y = _mm256_blendv_ps(zero, _mm256_mul_ps(n_y, _mm256_mul_ps(m2, p)), compare);
			const __m256 mvel2_x = _mm256_blendv_ps(zero, _mm256_mul_ps(n_x, _mm256_mul_ps(m1, p)), compare);
			const __m256 mvel2_y = _mm256_blendv_ps(zero, _mm256_mul_ps(n_y, _mm256_mul_ps(m1, p)), compare);

			this->circles.x_velocities[i] = _mm256_sub_ps(this->circles.x_velocities[i], _mm256_permutevar8x32_ps(mvel1_x, reverse_rows[per]));
			this->circles.y_velocities[i] = _mm256_sub_ps(this->circles.y_velocities[i], _mm256_permutevar8x32_ps(mvel1_y, reverse_rows[per]));

			this->circles.x_velocities[j] = _mm256_add_ps(this->circles.x_velocities[j], mvel2_x);
			this->circles.y_velocities[j] = _mm256_add_ps(this->circles.y_velocities[j], mvel2_y);
		}
	}


	const auto t3 = std::chrono::high_resolution_clock::now();
	const auto detection = std::chrono::duration<double, std::milli>(t2 - t1).count();
	const auto handle = std::chrono::duration<double, std::milli>(t3 - t2).count();
	//sprintf_s(title, "Wall Handle: %.8f(ms), Collision Handle: %.8f(ms)", detection, handle);

	if (draw)
		draw = false;

	static const auto positions_update_size = sizeof(float) * instance_count;
	vkMapMemory(this->device, this->x_positions_buffer_memory, 0, positions_update_size, 0, &data);
	memcpy(data, this->circles.x_positions, positions_update_size);
	vkUnmapMemory(this->device, this->x_positions_buffer_memory);

	vkMapMemory(this->device, this->y_positions_buffer_memory, 0, positions_update_size, 0, &data);
	memcpy(data, this->circles.y_positions, positions_update_size);
	vkUnmapMemory(this->device, this->y_positions_buffer_memory);

	UniformBufferObject ubo = {};

	ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	ubo.proj = glm::ortho(0.0f, static_cast<float>(this->swap_chain_extent.width), static_cast<float>(this->swap_chain_extent.height), 0.0f, -1000.0f, 1000.0f);

	vkMapMemory(this->device, this->ubo_buffers_memory[current_image], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(this->device, this->ubo_buffers_memory[current_image]);
}

bool CircleCollisionMultiThreadSIMD::draw_frame()
{
	vkWaitForFences(this->device, 1, &this->draw_fences[this->current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	// image_index vs current_frame
	uint32_t image_index;

	const auto acq_image_result = vkAcquireNextImageKHR(
		this->device,
		this->swap_chain,
		std::numeric_limits<uint64_t>::max(),
		this->image_available_semaphore[this->current_frame],
		VK_NULL_HANDLE,
		&image_index);

	if (acq_image_result == VK_SUBOPTIMAL_KHR
		|| acq_image_result == VK_ERROR_OUT_OF_DATE_KHR
		|| this->should_recreate_swapchain)
	{
		if (recreate_swap_chain())
		{
			this->should_recreate_swapchain = false;
			log("SwapChain Recreate");
			return true;
		}
		else
		{
			log("Failed SwapChain Recreatation.");
		}
	}

	VkSemaphore wait_semaphores[] = { this->image_available_semaphore[this->current_frame] };
	VkSemaphore singnal_semaphores[] = { this->render_finished_semaphore[this->current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	// Update UBO
	update(image_index);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &this->command_buffers[image_index];
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = singnal_semaphores;

	vkResetFences(this->device, 1, &this->draw_fences[this->current_frame]);
	if (vkQueueSubmit(this->graphics_queue, 1, &submit_info, this->draw_fences[this->current_frame]) != VK_SUCCESS)
	{
		log("vkQueueSubmit Failed");
		return false;
	}

	VkPresentInfoKHR present_info = {};

	VkSwapchainKHR swap_chains[] = { this->swap_chain };
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pImageIndices = &image_index;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = singnal_semaphores;
	present_info.pResults = nullptr;
	present_info.pSwapchains = swap_chains;
	present_info.swapchainCount = 1;

	const auto present_result = vkQueuePresentKHR(this->present_queue, &present_info);
	if (present_result == VK_SUBOPTIMAL_KHR || present_result == VK_ERROR_OUT_OF_DATE_KHR || this->should_recreate_swapchain)
	{
		if (recreate_swap_chain())
		{
			this->should_recreate_swapchain = false;
			log("SwapChain Recreate");
			return true;
		}
		else
		{
			log("Failed SwapChain Recreatation.");
		}
	}

	this->current_frame = (this->current_frame + 1) % this->num_frames;

	return true;
}

bool CircleCollisionMultiThreadSIMD::main_loop()
{
	this->last_timestamp = std::chrono::high_resolution_clock::now();

	create_threads();

	while (!glfwWindowShouldClose(this->window))
	{
		const auto t_start = std::chrono::high_resolution_clock::now();

		if (!draw_frame())
			return false;

		const auto t_end = std::chrono::high_resolution_clock::now();
		const auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_start).count();

		this->frame_counter++;
		this->frame_timer = (float)t_diff;

		float fps_timer = std::chrono::duration<double, std::milli>(t_end - this->last_timestamp).count();

		if (fps_timer > 300.0f)
		{
			this->last_fps = static_cast<uint32_t>((float)frame_counter * (1000.0f / fps_timer));

			sprintf_s(title, "%d FPS in %.8f (ms)", this->last_fps, this->frame_timer);

			sum_time += fps_timer;
			count_frames += this->frame_counter;

			glfwSetWindowTitle(this->window, title);

			this->frame_counter = 0;
			this->last_timestamp = t_end;
		}

		glfwPollEvents();

		if (count_frames > 500)
		{
			std::cout << "Average Frame Time: " << (float)sum_time / count_frames << std::endl;
			sum_time = 0;
			count_frames = 0;
		}

	}

	sync::should_close = true;
	this->thread_pool.wait_for_threads();
	this->thread_pool.release();

	return true;
}

void CircleCollisionMultiThreadSIMD::create_threads()
{
	// Post Proccessing
	checks_i[0] = 0;
	checks_j[0] = 0;

	size_t all_sum = (vectors_size * (vectors_size - 1) / 2);
	for (size_t k = 1; k <= num_threads; k++)
	{
		const size_t to_find = ((k == num_threads) ? num_pairs : k * (num_pairs / num_threads)) - 1;
		size_t to_ff = num_pairs - to_find - 1;
		size_t batch = 0;

		for (size_t i = 1; i < vectors_size + 1; ++i)
		{
			int upper = ((i) * (i + 1)) / 2;

			if (to_ff < upper)
			{
				int lower = ((i) * (i - 1)) / 2;
				if (to_ff >= lower)
				{
					batch = vectors_size - i - 1;
					checks_i[k] = batch;
					checks_j[k] = (upper - to_ff) + batch;
					break;
				}
			}
		}
	}

	// Dividing Job between Threads
	this->thread_pool.resize(num_threads);

	for (size_t id = 0; id < num_threads; ++id)
	{
		thread_pool.add_job(id, [&, id](const size_t mini, const size_t maxi, const size_t minj, const size_t maxj)
			{
				std::vector<std::pair<size_t, size_t>>	collided_local;
				std::vector<__m256>	collided_mask_local;
				std::vector<short>	permute_local;

				while (!sync::should_close)
				{
					while (!sync::should_start[id].load())
					{
						if (sync::should_close)
							break;
						std::this_thread::sleep_for(1ms);
					}
					sync::should_start[id] = false;


					// Collision Checks
					for (size_t i = mini; i <= maxi; ++i)
					{
						const  size_t begin = (i == mini) ? minj + 1 : (i);
						const  size_t end = (i == maxi) ? maxj + 1 : vectors_size;
						for (size_t j = begin; j < end; ++j)
						{
							// Vector Self Checks
							if (i == j)
							{		
								// Vector Self Checks
								for (size k = 0; k < 4; ++k)
								{
									__m256& x1 = circles.x_positions[i];
									__m256& y1 = circles.y_positions[i];
									__m256& s1 = circles.scales[i];

									const __m256 x2 = _mm256_permutevar8x32_ps(x1, self_check_rows[k]);
									const __m256 y2 = _mm256_permutevar8x32_ps(y1, self_check_rows[k]);
									const __m256 s2 = _mm256_permutevar8x32_ps(s1, self_check_rows[k]);

									const __m256 dx = _mm256_sub_ps(x2, x1);
									const __m256 dy = _mm256_sub_ps(y2, y1);
									const __m256 ds = _mm256_sub_ps(s2, s1);

									const __m256 dis2 = _mm256_fmadd_ps(dy, dy, _mm256_mul_ps(dx, dx));
									const __m256 radii = _mm256_add_ps(s1, s2);
									const __m256 radii2 = _mm256_mul_ps(radii, radii);

									const __m256 compare = _mm256_cmp_ps(dis2, radii2, _CMP_LE_OQ);

									if (_mm256_movemask_ps(compare) > 0)
									{
										collided_local.push_back(std::pair<size, size>(i, i));
										collided_mask_local.push_back(compare);
										permute_local.push_back(k);
									}
								}
							}
							// Vector Other Checks
							else
							{
								for (size k = 0; k < 8; ++k)
								{
									const __m256 x1 = _mm256_permutevar8x32_ps(circles.x_positions[i], rows[k]);
									const __m256 y1 = _mm256_permutevar8x32_ps(circles.y_positions[i], rows[k]);
									const __m256 s1 = _mm256_permutevar8x32_ps(circles.scales[i], rows[k]);

									const __m256 dx = _mm256_sub_ps(x1, circles.x_positions[j]);
									const __m256 dy = _mm256_sub_ps(y1, circles.y_positions[j]);

									const __m256 dis2 = _mm256_fmadd_ps(dy, dy, _mm256_fmadd_ps(dx, dx, zero));
									const __m256 radii = _mm256_add_ps(circles.scales[j], s1);
									const __m256 radii2 = _mm256_mul_ps(radii, radii);

									const __m256 compare = _mm256_cmp_ps(dis2, radii2, _CMP_LE_OQ);

									if (_mm256_movemask_ps(compare) > 0)
									{
										collided_local.push_back(std::pair<size, size>(i, j));
										collided_mask_local.push_back(compare);
										permute_local.push_back(k);
									}
								}
							}
						}
					}

					// Add local colls to collisions vector
					if (!collided_local.empty())
					{
						std::lock_guard<std::mutex> lock(collided_mutex);
						collided.insert(collided.begin(), collided_local.begin(), collided_local.end());
						collided_mask.insert(collided_mask.begin(), collided_mask_local.begin(), collided_mask_local.end());
						permute.insert(permute.begin(), permute_local.begin(), permute_local.end());
					}

					collided_local.clear();
					collided_mask_local.clear();
					permute_local.clear();

					++sync::num_finished;
				}

			}, checks_i[id], checks_i[id + 1], checks_j[id], checks_j[id + 1]);
	}
}

bool CircleCollisionMultiThreadSIMD::release()
{
	if (is_released)
		return true;

	circles.release();

	vkDeviceWaitIdle(this->device);

#if defined (_DEBUG)
	auto DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (DestroyDebugUtilsMessengerEXT != nullptr)
	{
		DestroyDebugUtilsMessengerEXT(this->instance, this->debug_messenger, nullptr);
	}
#endif

	if (this->device)
	{
		vkDestroyBuffer(this->device, this->vertex_buffer, nullptr);
		vkFreeMemory(this->device, this->vertex_buffer_memory, nullptr);

		vkDestroyBuffer(this->device, this->index_buffer, nullptr);
		vkFreeMemory(this->device, this->index_buffer_memory, nullptr);

		vkDestroyBuffer(this->device, this->colors_buffer, nullptr);
		vkFreeMemory(this->device, this->colors_buffer_memory, nullptr);

		vkDestroyBuffer(this->device, this->x_positions_buffer, nullptr);
		vkFreeMemory(this->device, this->x_positions_buffer_memory, nullptr);

		vkDestroyBuffer(this->device, this->y_positions_buffer, nullptr);
		vkFreeMemory(this->device, this->y_positions_buffer_memory, nullptr);

		vkDestroyBuffer(this->device, this->scales_buffer, nullptr);
		vkFreeMemory(this->device, this->scales_buffer_memory, nullptr);

		cleanup_swap_chain();

		vkDestroyDescriptorSetLayout(this->device, this->ubo_descriptor_set_layout, nullptr);

		vkDestroyPipeline(this->device, this->graphics_pipeline, nullptr);
		vkDestroyPipelineLayout(this->device, this->pipeline_layout, nullptr);

		for (auto i = 0; i < this->num_frames; ++i)
		{
			vkDestroySemaphore(this->device, this->image_available_semaphore[i], nullptr);
			vkDestroySemaphore(this->device, this->render_finished_semaphore[i], nullptr);
			vkDestroyFence(this->device, this->draw_fences[i], nullptr);
		}

		vkDestroyCommandPool(this->device, this->command_pool, nullptr);

		// Destroy Device
		vkDestroyDevice(this->device, nullptr);
	}

	if (this->instance)
	{
		vkDestroySurfaceKHR(this->instance, this->surface, nullptr);

		// Destroy Instance
		vkDestroyInstance(this->instance, nullptr);
	}

	glfwDestroyWindow(this->window);
	glfwTerminate();

	is_released = true;
	return true;
}

void CircleCollisionMultiThreadSIMD::window_resize()
{
	this->should_recreate_swapchain = true;
}

bool CircleCollisionMultiThreadSIMD::create_colors_buffer()
{
	const VkDeviceSize buffer_size = sizeof(glm::vec3) * instance_count;

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		this->colors_buffer,
		this->colors_buffer_memory))
	{
		return false;
	}

	void* data = nullptr;
	vkMapMemory(this->device, this->colors_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, this->circles.colors, buffer_size);
	vkUnmapMemory(this->device, this->colors_buffer_memory);

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_positions_buffer()
{
	const VkDeviceSize buffer_size = sizeof(float) * instance_count;

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		this->x_positions_buffer,
		this->x_positions_buffer_memory))
	{
		return false;
	}

	void* data = nullptr;
	vkMapMemory(this->device, this->x_positions_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, this->circles.x_positions, buffer_size);
	vkUnmapMemory(this->device, this->x_positions_buffer_memory);

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		this->y_positions_buffer,
		this->y_positions_buffer_memory))
	{
		return false;
	}

	vkMapMemory(this->device, this->y_positions_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, this->circles.y_positions, buffer_size);
	vkUnmapMemory(this->device, this->y_positions_buffer_memory);

	return true;
}

bool CircleCollisionMultiThreadSIMD::create_scales_buffer()
{
	const VkDeviceSize buffer_size = sizeof(float) * instance_count;

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory))
	{
		return false;
	}

	void* data = nullptr;
	vkMapMemory(this->device, staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, this->circles.scales, buffer_size);
	vkUnmapMemory(this->device, staging_buffer_memory);

	if (!helper::create_buffer(
		this->device,
		this->physical_device,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		this->scales_buffer,
		this->scales_buffer_memory))
	{
		return false;
	}

	helper::copy_buffer(this->device, this->command_pool, this->graphics_queue, staging_buffer, this->scales_buffer, buffer_size);

	vkDestroyBuffer(this->device, staging_buffer, nullptr);
	vkFreeMemory(this->device, staging_buffer_memory, nullptr);

	return true;
}

void CircleCollisionMultiThreadSIMD::setup_circles()
{
	this->circles.resize();

	for (size_t i = 0; i < instance_count; ++i)
	{
		this->circles.colors[i] = glm::vec3((rand() % 255) / 255.0f, (rand() % 255) / 255.0f, (rand() % 255) / 255.0f);
	}

	for (size_t i = 0; i < vectors_size; ++i)
	{
		this->circles.scales[i] = rand_scale_vec();

		this->circles.x_velocities[i] = rand_vel_vec(this->circles.scales[i]);
		this->circles.y_velocities[i] = rand_vel_vec(this->circles.scales[i]);

		this->circles.x_positions[i] = rand_pos_vec(this->circles.scales[i], screen_width);
		this->circles.y_positions[i] = rand_pos_vec(this->circles.scales[i], screen_height);
	}

	float mulf[8];
	const int ones = instance_count % 8;
	for (int i = 0; i < ones; ++i)
		mulf[i] = 1;
	for (int i = ones; i < 8; ++i)
		mulf[i] = 0;

	generate_rows();
	this->circles.scales[vectors_size - 1] = _mm256_mul_ps(this->circles.scales[vectors_size - 1], _mm256_loadu_ps(mulf));
	this->circles.x_positions[vectors_size - 1] = _mm256_div_ps(this->circles.x_positions[vectors_size - 1], _mm256_loadu_ps(mulf));
	this->circles.y_positions[vectors_size - 1] = _mm256_div_ps(this->circles.y_positions[vectors_size - 1], _mm256_loadu_ps(mulf));

	const size padding_count = dividible_size - instance_count;
}