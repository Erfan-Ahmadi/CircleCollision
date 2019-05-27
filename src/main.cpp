#include "Renderer\vulkan_renderer.h"

int main()
{
	VulkanRenderer app;

	if (!app.run())
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}