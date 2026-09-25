#include <SDL3/SDL.h>
#include "scenes/SceneManager.h"
#include "render/VulkanRenderer.h"
#include "render/OpenGLRenderer.h"
#include "core/Timer.h"
#include "scenes/Scene0.h"
#include "scenes/Scene1.h"
#include "core/Debug.h"
#include <thread>

bool swapscene = false;
bool loadStagedScene = false;
int scene_number = 0;
std::mutex vulkanMutex;

SceneManager::SceneManager(): 
	currentScene(nullptr), stagedScene(nullptr), timer(nullptr),
	fps(60), isRunning(false), rendererType(RendererType::VULKAN),
	renderer(nullptr) {}

SceneManager::~SceneManager() {
	if (currentScene) {
		currentScene->OnDestroy();
		delete currentScene;
		currentScene = nullptr;
	}
	if (stagedScene)
	{
		stagedScene->OnDestroy();
		delete stagedScene;
		stagedScene = nullptr;
	}
	if (timer) {
		delete timer;
		timer = nullptr;
	}
	renderer->OnDestroy();
	delete renderer;
	Debug::Info("Deleting the GameSceneManager", __FILE__, __LINE__);

}

bool SceneManager::Initialize(std::string name_, int width_, int height_) {
	switch(rendererType){
	case RendererType::OPENGL:
		renderer = new OpenGLRenderer();
		renderer->setRendererType(RendererType::OPENGL);
		renderer->CreateWindow(name_, width_, height_);
		renderer->OnCreate();
		break;

	case RendererType::VULKAN:
		renderer = new VulkanRenderer();
		renderer->setRendererType(RendererType::VULKAN);
		renderer->CreateWindow(name_, width_, height_);
		renderer->OnCreate();
		break;

	case RendererType::DIRECTX11:
	case RendererType::DIRECTX12:
		Debug::FatalError("Renderer not yet supported", __FILE__, __LINE__);
		return false;
		break;
	}

	timer = new Timer();
	if (timer == nullptr) {
		Debug::FatalError("Failed to initialize Timer object", __FILE__, __LINE__);
		return false;
	}
	
	BuildScene(SCENE0, true);
	
	return true;
}


void SceneManager::Run() {
	SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
	timer->Start();
	isRunning = true;
	while (isRunning) {
	
		timer->UpdateFrameTicks();
		currentScene->Update(timer->GetDeltaTime());
		currentScene->Render();
		GetEvents();
		SDL_Delay(timer->GetSleepTime(fps));	
		if (loadStagedScene)
		{
				std::thread th1([=]
				{
					bool loadedScene = ThreadStagedScene(scene_number);
					if (loadedScene) {
						isSceneReady = true;
					}
				});    
				th1.detach();
				loadStagedScene = false;
		}
		if (isSceneReady)
		{
			SwapScene();
			isSceneReady = false;
		}		
	}
}

void SceneManager::GetEvents() {
	SDL_Event sdlEvent;
	while (SDL_PollEvent(&sdlEvent)) {
		if (sdlEvent.type == SDL_EventType::SDL_EVENT_QUIT) {
			isRunning = false;
			return;
		}
		else if (sdlEvent.type == SDL_EVENT_KEY_DOWN) {
			switch (sdlEvent.key.scancode) {
			case SDL_SCANCODE_ESCAPE:
			case SDL_SCANCODE_Q:
				isRunning = false;
				return;

			case SDL_SCANCODE_F1:
				BuildScene(SCENE0);
				break;
			case SDL_SCANCODE_F2:
				BuildScene(SCENE1);
				
				break;
			case SDL_SCANCODE_F3:
				///BuildScene(SCENE2);
				break;

			case SDL_SCANCODE_F4:
				///BuildScene(SCENE3);
				break;

			case SDL_SCANCODE_F5:
				///BuildScene(SCENE4);
				break;

			case SDL_SCANCODE_F6:
				///BuildScene(SCENE5);
				break;

			case SDL_SCANCODE_F7:
				///BuildScene(SCENE6);
				break;

			default:
				break;
			}
		}
		if (currentScene == nullptr) {
			Debug::FatalError("Failed to initialize Scene", __FILE__, __LINE__);
			isRunning = false;
			return;
		}
		
		currentScene->HandleEvents(sdlEvent);
	}
}

bool SceneManager::BuildScene(SCENE_NUMBER scene_, bool isInitialBoot) {
	bool status = false; 
	Scene* tempScene = nullptr; // Load into a safe local pointer first

	switch (scene_) {
	case SCENE0:  
		tempScene = new Scene0(renderer);
		status = tempScene->OnCreate();
		break;

	case SCENE1:
		tempScene = new Scene1(renderer); // Just use the main renderer
		status = tempScene->OnCreate();
		break;
		
	case SCENE2:
		///currentScene = new Scene2();
		status = currentScene->OnCreate();
		break;
	case SCENE3:
		///currentScene = new Scene3();
		status = currentScene->OnCreate();
		break;
	case SCENE4:
		///currentScene = new Scene4();
		//status = currentScene->OnCreate();
		break;
	case SCENE5:
		///currentScene = new Scene5();
		status = currentScene->OnCreate();
		break;
	case SCENE6:
		///currentScene = new Scene6();
		status = currentScene->OnCreate();

		break;

	default:
		Debug::Error("Incorrect scene number", __FILE__, __LINE__);
		return false;
	}  

	// Safely route the newly loaded scene based on engine state
	if (status) {
		if (isInitialBoot) {
			currentScene = tempScene;
		} else {
			stagedScene = tempScene; // Background threads safely store it here
		}
	} else {
		delete tempScene; 
	}

	return status;
}

void SceneManager::SwapScene()
{
	if (currentScene != nullptr) {
		currentScene->OnDestroy();
		delete currentScene;
		currentScene = nullptr;
	}
    
	currentScene = stagedScene;
	stagedScene = nullptr; 
    
	swapscene = false;
	std::cout << "Swapped Scene" << std::endl;
}


bool SceneManager::ThreadStagedScene(int scene_number)
{
	SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
	switch (scene_number)
	{
	case 0:
		if (BuildScene(SCENE0))
		{
			loadStagedScene = false;
			std::cout << "Loaded Scene" << std::endl;
			return true;
		}
		std::cout << "Failed to load scene" << std::endl;
		return false;
		break;
		case 1:
		if (BuildScene(SCENE1))
		{
			loadStagedScene = false;
			std::cout << "Loaded Scene" << std::endl;
			return true;
		}
		std::cout << "Failed to load scene" << std::endl;
		return false;
	}
	return false;
	
}




