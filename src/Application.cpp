#include "Engine.h"

int main()
{
	Engine* engine = new Engine();
	engine->Run();
	delete engine;

	return 0;
}