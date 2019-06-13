#include "simple_multithread_stl.h"

int main()
{
	CircleCollisionMultiThreaded app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}