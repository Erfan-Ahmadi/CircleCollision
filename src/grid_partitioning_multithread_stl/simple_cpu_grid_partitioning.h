#pragma once

#include "../common.hpp"
#include "grid.hpp"
#include <chrono>

struct circles_strcut
{
	std::vector<glm::vec2> positions;
	std::vector<glm::vec2> velocities;
	std::vector<glm::vec3> colors;
	std::vector<float> scales; // = radius

	inline void resize(const size_t& size)
	{
		positions.resize(size);
		velocities.resize(size);
		colors.resize(size);
		scales.resize(size);
	}
};
 
struct CircleCollisionGridPartitioningMultiThreaded
{
public:
	void initialize();

	bool run();

	bool release();

	void window_resize();

private:

	bool setup_window();
	bool setup_vulkan();

	bool create_instance();
	bool set_up_debug_messenger();
	bool pick_physical_device();

	bool check_device_extensions_support();
	bool create_logical_device();
	bool create_surface();
	bool create_swap_chain();
	bool create_image_views();
	bool create_renderpass();
	bool create_descriptor_set_layout();
	bool create_graphics_pipeline();
	bool create_vertex_buffer(); //
	bool create_index_buffer(); //
	bool create_instance_buffers(); //
	bool create_uniform_buffers(); //
	bool create_descriptor_pool();
	bool create_descriptor_sets();
	bool create_frame_buffers();
	bool create_command_pool();
	bool create_command_buffers();
	bool create_sync_objects();
	
	bool create_colors_buffer(); //
	bool create_positions_buffer(); //
	bool create_scales_buffer(); // 

	bool cleanup_swap_chain();
	bool recreate_swap_chain();
	bool set_viewport_scissor();

	void update(const uint32_t& current_image);

	bool draw_frame();

	bool main_loop();
	
	// Sample ----------------

	renderer::model circle_model;

	void setup_circles();
	circles_strcut circles;

	grid grid;
		
	std::vector<VkBuffer> ubo_buffers;
	std::vector<VkDeviceMemory> ubo_buffers_memory;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;

	VkBuffer colors_buffer;
	VkDeviceMemory colors_buffer_memory;

	VkBuffer scales_buffer;
	VkDeviceMemory scales_buffer_memory;
	
	VkBuffer positions_buffer;
	VkDeviceMemory positions_buffer_memory;

	VkDescriptorPool ubo_descriptor_pool;
	std::vector<VkDescriptorSet> ubo_descriptor_sets;
	VkDescriptorSetLayout ubo_descriptor_set_layout;

	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	
	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;

	// Sample ----------------


	//	Vulkan
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice  device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	renderer::helper::QueueFamilyIndices family_indices;

	VkRenderPass render_pass;

	VkQueue graphics_queue;
	VkQueue present_queue;

	bool should_recreate_swapchain;

	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swap_chain_images;
	std::vector<VkImageView> swap_chain_image_views;
	std::vector<VkFramebuffer> swap_chain_frame_buffers;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;

	VkViewport viewport;
	VkRect2D scissor;

	std::vector<VkSemaphore> image_available_semaphore;
	std::vector<VkSemaphore> render_finished_semaphore;
	std::vector<VkFence> draw_fences;

	std::chrono::time_point<std::chrono::high_resolution_clock> last_timestamp;
	size_t frame_counter;
	float frame_timer = 1.0f;
	uint32_t last_fps = 0;

	size_t num_frames;
	size_t current_frame = 0;

	bool validation_layers_enabled;

#ifdef _DEBUG
	VkDebugUtilsMessengerEXT debug_messenger;
#endif
	//	Window
	GLFWwindow* window;

	bool is_released;
};