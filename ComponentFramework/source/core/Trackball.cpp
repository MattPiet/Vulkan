#include <SDL.h>
#include <core/Trackball.h>
#include <VMath.h>
#include <QMath.h>
#include <MMath.h>

#define M_PI 3.14159265358979323846f

///https://www.khronos.org/opengl/wiki/Object_Mouse_Trackball

Trackball::Trackball() {
	mouseDown = false;
}

Trackball::~Trackball() {}

void Trackball::HandleEvents(const SDL_Event& sdlEvent) {
	if (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (sdlEvent.button.button == SDL_BUTTON_RIGHT) {
			SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(sdlEvent.button.windowID), true);
			onRightMouseDown(static_cast<int>(sdlEvent.button.x), static_cast<int>(sdlEvent.button.y));
		}
	}
	else if (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		if (sdlEvent.button.button == SDL_BUTTON_RIGHT) {
			SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(sdlEvent.button.windowID), false);
			onRightMouseUp(static_cast<int>(sdlEvent.button.x), static_cast<int>(sdlEvent.button.y));
		}
	}
	else if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(SDL_GetWindowFromID(sdlEvent.button.windowID))) {
			onMouseMove(static_cast<int>(sdlEvent.motion.xrel), static_cast<int>(sdlEvent.motion.yrel));
		}
	}
}

void Trackball::onRightMouseDown(int x, int y) {
	mouseDown = true;
	lastMouseX = x;
	lastMouseY = y;

}

void Trackball::onRightMouseUp(int x, int y) {
	mouseDown = false;
}

void Trackball::onMouseMove(int x, int y) {
	if (!mouseDown) return;

	// Use the parameters directly as deltas
	m_Yaw += -static_cast<float>(x) * m_Sensitivity;
	m_Pitch += -static_cast<float>(y) * m_Sensitivity;

	// Clamp Pitch to prevent flipping
	if (m_Pitch > 89.0f)  m_Pitch = 89.0f;
	if (m_Pitch < -89.0f) m_Pitch = 89.0f;

	Quaternion qYaw = QMath::angleAxisRotation(m_Yaw, Vec3(0.0f, 1.0f, 0.0f));
	Quaternion qPitch = QMath::angleAxisRotation(m_Pitch, Vec3(1.0f, 0.0f, 0.0f));
	mouseRotationQuat = qYaw * qPitch;
}
#undef M_PI