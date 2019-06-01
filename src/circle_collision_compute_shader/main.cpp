#include "circle_collision_compute_shader.h"

int main()
{
	CircleCollisionComputeShader app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}