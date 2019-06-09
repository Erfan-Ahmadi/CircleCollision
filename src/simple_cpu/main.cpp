#include "simple_cpu.h"

int main()
{
	CircleCollisionSimple app;

	app.initialize();

	if (!app.run())
		return EXIT_FAILURE;

	app.release();

	return EXIT_SUCCESS;
}