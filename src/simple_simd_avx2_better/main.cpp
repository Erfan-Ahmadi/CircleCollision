#include "simple_simd_avx2_better.h"

int main()
{
	CircleCollisionMultiThreadSIMD app;
	
	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}