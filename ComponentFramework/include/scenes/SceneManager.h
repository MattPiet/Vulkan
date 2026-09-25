#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <string>
#include "render/Renderer.h"
#include <mutex>
class SceneManager  {\
public:
	
	SceneManager();
	~SceneManager();
	void Run();
	bool Initialize(std::string name_, int width_, int height_);
	void GetEvents();
	
	
private:
	
	enum SCENE_NUMBER {
		SCENE0 = 0,
		SCENE1,
		SCENE2,
		SCENE3,
		SCENE4,
		SCENE5,
		SCENE6
	};
	
	std::atomic<bool> isSceneReady{false};

	enum class RendererType rendererType;
	class Scene* currentScene;
	class Scene* stagedScene;
	class Timer* timer;

	Renderer* renderer;
	unsigned int fps;
	bool isRunning;
	bool BuildScene(SCENE_NUMBER scene_, bool isInitialBoot = false);
	
	void SwapScene();
	
	bool ThreadStagedScene(int SceneNumber);
	
};

extern bool swapscene;
extern bool loadStagedScene;
extern int scene_number;
extern std::mutex vulkanMutex;
#endif // SCENEMANAGER_H