#pragma once
#define _CRTDBG_MAP_ALLOC  
#include <iostream> // Or any other standard library headers
#include <stdlib.h>
#include <crtdbg.h>
#include "core/Debug.h"
#include <string>
#ifdef _DEBUG
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif


#include "scenes/SceneManager.h"


#include "MMath.h"
using namespace MATH;
 
int main(int argc, char* args[]) {
	/// Look for memory leaks
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	
	
#pragma warning(disable : 4996) 
	/*if (const char* env_p = std::getenv("PATH")) {
		std::cout << "Your PATH is: " << env_p << '\n';
	}*/

	static_assert(sizeof(int*) == 8, "Vulkan no longer supports x86 builds");

	Debug::DebugInit("Vulkan Graphics Engine Log");
	Debug::Info("Starting the GameSceneManager", __FILE__, __LINE__);
	SceneManager* gsm = new SceneManager();
	if (gsm->Initialize("Vulkan Graphics Engine", 1920, 1080) ==  true) {
		gsm->Run();
	} 
	delete gsm;
	exit(0);

}