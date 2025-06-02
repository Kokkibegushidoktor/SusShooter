#include "game.h"
#include <stdbool.h>
#include <string.h>

int main(int argc, char* argv[])
{
	bool drawDebugRays = false;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "debug")) {
			drawDebugRays = true;
		}
	}
	
	startGame(drawDebugRays);
}