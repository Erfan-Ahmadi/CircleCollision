#include "grid_partitioning_multithread_stl.h"

int main()
{
	CircleCollisionGridPartitioningMultiThreaded app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}