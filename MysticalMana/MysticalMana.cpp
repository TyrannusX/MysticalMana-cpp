#define PLATFORM_WIN32 1

#include <iostream>
#include <memory>
#include "Application.h"

int main()
{
	std::unique_ptr<Application> game(new Application());
	game->Run();
	return 0;
}
