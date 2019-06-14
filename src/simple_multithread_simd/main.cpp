#include "simple_multithread_simd.h"

int main()
{
	CircleCollisionMultiThreadSIMD app;
	
	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}