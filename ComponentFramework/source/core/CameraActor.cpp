#include "core/CameraActor.h"
#include <MMath.h>
#include <SDL3/SDL.h>
#include <QMath.h>
#include "Core/Debug.h"
#include <assert.h>
#include "Physics/TransformComponent.h"
using namespace MATH;
CameraActor::CameraActor(std::weak_ptr<Component> parent_, float fovy, float aspectRatio, float near, float far) : Actor(parent_)
{
	projectionMatrix = MMath::perspective(fovy, aspectRatio, near, far);
	viewMatrix.loadIdentity();
}

CameraActor::~CameraActor() {
	OnDestroy();
}

bool CameraActor::OnCreate()
{
	// This'll stop us from creating something twice and generating memory leaks
	// This will save us sweating when the code gets even more complicated
	if (isCreated) return true;

	isCreated = true;
	return isCreated;
}

void CameraActor::UpdateProjectionMatrix(const float fovy, const float aspectRatio, const float near, const float far)
{
	projectionMatrix = MMath::perspective(fovy, aspectRatio, near, far);

}

void CameraActor::UpdateViewMatrix()
{
	// let's hope we have transform component!
	Ref<TransformComponent> transformComponent = GetComponent<TransformComponent>();
	if (transformComponent == nullptr) {
		// we have no transform component! use default view. takes 3 vectors
		// this sets up where is the camera, what's it looking at, and what's up
		// we are moving the universe, not a camera
		// the eye moves the camera back towards us (positive z)
		// but what really happens is we push the whole universe negative 5 along z
		viewMatrix = MMath::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
		
	}
	else {
		Quaternion orientation = transformComponent->GetQuaternion();
		Vec3 position = transformComponent->GetPosition();
		// Ok we have the position and orientation of the camera in world space
		// But the view matrix takes us from world space to camera space (camera at origin looking down -z)
		// So we need to translate back to origin first, then rotate the camera back to the -z axis
		Matrix4 T_inverse = MMath::translate(-position);
		Matrix4 R_inverse = MMath::toMatrix4(QMath::conjugate(orientation));

		viewMatrix = R_inverse * T_inverse;
		// forget using the scale matrix above. What does that even mean for a camera?
	}
}

void CameraActor::OnDestroy()
{
	isCreated = false;
}

Vec3 CameraActor::freeCameraMovement(Vec3 direction)
{
	Matrix4 worldToCamera = this->GetViewMatrix();
	Matrix4 cameraToWorld = MMath::inverse(worldToCamera);
	Vec3 rotated_forward_in_cam_space = cameraToWorld * direction;
	return rotated_forward_in_cam_space;
}

void CameraActor::CameraMovement(float deltaTime, SDL_Gamepad* gamepad)
{

    Vec3 inputVelocity(0.0f, 0.0f, 0.0f);
    const float deadzone = 0.2f; 
    const int16_t triggerDeadzone = 8000; 

    if (gamepad && SDL_GamepadConnected(gamepad)) {
       float stickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
       float stickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

       if (SDL_fabsf(stickX) > deadzone) inputVelocity.x += stickX * 2;
       if (SDL_fabsf(stickY) > deadzone) inputVelocity.z += stickY * 2;

       int16_t leftTrigger  = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
       int16_t rightTrigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

       if (leftTrigger > triggerDeadzone)  inputVelocity.y -= 1.0f;
       if (rightTrigger > triggerDeadzone) inputVelocity.y += 1.0f;
    }
	
    const bool* keyboardState = SDL_GetKeyboardState(NULL);
    if (keyboardState[SDL_SCANCODE_W])      inputVelocity.z -= 1.0f;
    if (keyboardState[SDL_SCANCODE_S])      inputVelocity.z += 1.0f;
    if (keyboardState[SDL_SCANCODE_A])      inputVelocity.x -= 1.0f;
    if (keyboardState[SDL_SCANCODE_D])      inputVelocity.x += 1.0f;
    if (keyboardState[SDL_SCANCODE_SPACE])  inputVelocity.y += 1.0f;
    if (keyboardState[SDL_SCANCODE_LSHIFT]) inputVelocity.y -= 1.0f;

    Ref<TransformComponent> transform = GetComponent<TransformComponent>();
    Quaternion currentOrientation = transform->GetQuaternion();

    if (gamepad && SDL_GamepadConnected(gamepad)) {
       float rightStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
       float rightStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;

       if (SDL_fabsf(rightStickX) > deadzone || SDL_fabsf(rightStickY) > deadzone) {
           m_Yaw += -rightStickX * c_Sensitivity * deltaTime;
           m_Pitch += -rightStickY * c_Sensitivity * deltaTime;

          if (m_Pitch > 89.0f)  m_Pitch = 89.0f;
          if (m_Pitch < -89.0f) m_Pitch = -89.0f;

          Quaternion qYaw = QMath::angleAxisRotation(m_Yaw, Vec3(0.0f, 1.0f, 0.0f));
          Quaternion qPitch = QMath::angleAxisRotation(m_Pitch, Vec3(1.0f, 0.0f, 0.0f));
          currentOrientation = qYaw * qPitch;
       }
    }
	
    if (VMath::mag(inputVelocity) > 0.0f) {
       // Cap the magnitude at 1.0f so moving diagonally isn't faster, 
       // but we preserve analog stick sensitivity (values < 1.0)
       if (VMath::mag(inputVelocity) > 1.0f) {
           inputVelocity = VMath::normalize(inputVelocity);
       }
       
       Vec3 displacement = inputVelocity * GetCameraSpeed() * deltaTime;
       SetView(currentOrientation, freeCameraMovement(displacement));
    } else {
       // Even if we aren't moving, we still need to apply rotation if the right stick moved!
       SetView(currentOrientation, transform->GetPosition());
    }

    // Update the UBO
    UpdateViewMatrix();
}
void CameraActor::SetView(const Quaternion& orientation_, const Vec3& position_) {
	GetComponent<TransformComponent>()->SetOrientation(orientation_ );
	GetComponent<TransformComponent>()->SetPosition(position_);
}

void CameraActor::SetQuat(const SDL_Event& sdlEvent)
{
	if (sdlEvent.type == SDL_EVENT_MOUSE_WHEEL)
	{
		if (SDL_GetGlobalMouseState(NULL, NULL) & SDL_BUTTON_RMASK) {
			CameraSpeed += sdlEvent.wheel.y * 2.0f; 
		}
		if (CameraSpeed < 0.0f) CameraSpeed = 0.0f;
		if (CameraSpeed > 100.0f) CameraSpeed = 100.0f;
	}
	// Only let the trackball update if it's a mouse event
	if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION ||
		sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
		sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		trackball.HandleEvents(sdlEvent);
		GetComponent<TransformComponent>()->SetOrientation(trackball.getQuat()); // Sync camera to trackball
		}
}
