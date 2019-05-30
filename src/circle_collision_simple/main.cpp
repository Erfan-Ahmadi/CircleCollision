#include "vulkan_renderer.h"

int main()
{
	circle_collision_simple app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}