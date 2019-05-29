#include "Renderer\vulkan_renderer.h"

int main()
{
	VulkanRenderer app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}