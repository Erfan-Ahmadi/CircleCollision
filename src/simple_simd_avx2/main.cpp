#include "simple_simd_avx2.h"

int main()
{
	CircleCollisionSIMD app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}