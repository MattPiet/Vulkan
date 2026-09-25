#include <SDL3/SDL.h>
#include <core/Trackball.h>
#include <VMath.h>
#include <QMath.h>
#include <MMath.h>

#define M_PI 3.14159265358979323846f

Trackball::Trackball() {
	mouseDown = false;
}

Trackball::~Trackball() {}

void Trackball::HandleEvents(const SDL_Event& sdlEvent) {
	if (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (sdlEvent.button.button == SDL_BUTTON_RIGHT) {
			SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(sdlEvent.button.windowID), true);
			onRightMouseDown();
		}
	}
	else if (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		if (sdlEvent.button.button == SDL_BUTTON_RIGHT) {
			SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(sdlEvent.button.windowID), false);
			onRightMouseUp();
		}
	}
	else if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(SDL_GetWindowFromID(sdlEvent.motion.windowID))) {
			onMouseMove(static_cast<int>(sdlEvent.motion.xrel), static_cast<int>(sdlEvent.motion.yrel));
		}
	}
}

void Trackball::onMouseMove(int xrel, int yrel) {
	if (!mouseDown) return;
	
	m_Yaw += -static_cast<float>(xrel) * m_Sensitivity;
	m_Pitch += -static_cast<float>(yrel) * m_Sensitivity;

	if (m_Pitch > 89.0f)  m_Pitch = 89.0f;
	if (m_Pitch < -89.0f) m_Pitch = -89.0f;

	Quaternion qYaw = QMath::angleAxisRotation(m_Yaw, Vec3(0.0f, 1.0f, 0.0f));
	Quaternion qPitch = QMath::angleAxisRotation(m_Pitch, Vec3(1.0f, 0.0f, 0.0f));
	mouseRotationQuat = qYaw * qPitch;
}

void Trackball::onRightMouseDown() {
	mouseDown = true;
}

void Trackball::onRightMouseUp() {
	mouseDown = false;
}
#undef M_PI