// Umer Noor 2022
// Original code from/taught by Dr. Scott Fielder in Game Engine 4 class. Lucky for me I was in that class.

#ifndef CAMERA_ACTOR_H
#define CAMERA_ACTOR_H


#include <SDL3/SDL_events.h>

#include "Actor.h"
#include <core/Trackball.h>
#include <Quaternion.h>
#include <DQMath.h>
#include <core/CoreStructs.h>
using namespace MATH;
class CameraActor:public Actor {
private:
	// The camera is nothing but a projection matrix and a view matrix
	// These are not pointers, so never delete them! They are created on the stack
	// The automatic constructor will be the identity
	Matrix4 projectionMatrix;
	Matrix4 viewMatrix;

	MATHEX::DualQuat position_orientation_Quat;
	
	float CameraSpeed = 20.0f;

	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;
	float m_Sensitivity = 60.0f;
	float c_Sensitivity = 160.0f;
	Trackball trackball;
	CameraData data;
public:
	CameraActor(std::weak_ptr<Component> parent_, float fovy, float aspectRatio, float near, float far);
	~CameraActor();

	// we will override the base class one (new in C++ 11)
	// put override so that if you spell the method wrong it give a error
	bool OnCreate() override;
	void OnDestroy() override;

	Matrix4 GetProjectionMatrix() const { return projectionMatrix; }
	Matrix4 GetViewMatrix() const { return viewMatrix; }

	// fovy (field of view along y) is roughly 40 degrees
	// need aspectRatio of window, near and far clipping planes
	// these are the standard arguments for generating the perspective matrix in OpenGL
	void UpdateProjectionMatrix(const float fovy, const float aspectRatio, const float near, const float far);
	void UpdateViewMatrix();	
	float GetCameraSpeed() const { return CameraSpeed; }
	//void SetSensitivity(float sens) { trackball.SetSensitivity(sens); }
	
	Vec3 freeCameraMovement(Vec3 direction);
	void CameraMovement(float deltaTime, SDL_Gamepad* gamepad);
	void SetView(const Quaternion& orientation_, const Vec3& position_);
	void SetQuat(const SDL_Event &sdlEvent);
	
	CameraData GetCameraData()
	{
		data.viewMatrix = MMath::toMatrix4(position_orientation_Quat);
		data.projectionMatrix = projectionMatrix;
		data.projectionMatrix[5] *= -1;
		return data;
	}
};
#endif
