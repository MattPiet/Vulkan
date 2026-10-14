// Umer Noor 2022
// Original code from/taught by Dr. Scott Fielder in Game Engine 4 class. Lucky for me I was in that class.

#ifndef CAMERA_ACTOR_H
#define CAMERA_ACTOR_H


#include <SDL3/SDL_events.h>

#include "Actor.h"
#include <core/Trackball.h>
#include <Quaternion.h>
#include <DQMath.h>
#include <Plane.h>
#include <PMath.h>
#include <core/CoreStructs.h>

#include "render/VulkanRenderer.h"
using namespace MATH;

enum class FrustumCollision {
	INSIDE,
	CULLED_NEAR,
	CULLED_FAR,
	CULLED_LEFT,
	CULLED_RIGHT,
	CULLED_BOTTOM,
	CULLED_TOP
};

struct Frustum
{
	MATHEX::Plane left_plane;
	MATHEX::Plane right_plane;
	MATHEX::Plane front_plane;
	MATHEX::Plane back_plane;
	MATHEX::Plane top_plane;
	MATHEX::Plane bottom_plane;
};

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
	
	Frustum frustum;
	
	float cullRadius = 2.0f;
	
	std::vector<BufferMemory> cameraUBO;
	DescriptorSetInfo CameraUBOinfo;
	PipelineInfo skyboxPipelineInfo;
	
public:
	CameraActor(std::weak_ptr<Component> parent_, float fovy, float aspectRatio, float near, float far);
	~CameraActor();

	// we will override the base class one (new in C++ 11)
	// put override so that if you spell the method wrong it give a error
	bool OnCreate(VulkanRenderer* vRenderer) override;
	void OnDestroy() override;

	std::vector<BufferMemory> GetCameraUBO() const { return cameraUBO; }
	DescriptorSetInfo GetCameraUBODescriptorSetInfo() const { return CameraUBOinfo; }
	DescriptorSetInfo& GetCameraUBODescriptorSetInfoRef() { return CameraUBOinfo; }
	void SetCameraUBODescriptorSetInfo(const DescriptorSetInfo& info) { CameraUBOinfo = info; }
	PipelineInfo GetSkyboxPipelineInfo() const { return skyboxPipelineInfo; }
	
	Matrix4 GetProjectionMatrix() const { return projectionMatrix; }
	Matrix4 GetViewMatrix() const { return MMath::toMatrix4(position_orientation_Quat); }
	
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
		return data;
	}
	
	void CreateSkyboxPipeline(VulkanRenderer* vRenderer);
	
	void setFrustumCullRadius(float frustumCullRadius){cullRadius = frustumCullRadius;}
	
	Frustum GetFrustum() const { return frustum; } 
	void UpdateFrustum();
	FrustumCollision CheckFrustum(Matrix4 modelMatrix);
	
};




#endif
