#include "simple_cpu_grid_partitioning.h"

int main()
{
	CircleCollisionGridPartitioning app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}