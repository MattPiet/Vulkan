#pragma once
#include "render/VulkanRenderer.h"
#include <DualQuat.h>
#include <MMath.h>
using namespace MATH;
using namespace MATHEX;
class Camera {
private:
    DualQuat orientationPosition;
public:
    Matrix4 getViewMatrix() { return MMath::toMatrix4(orientationPosition); }

};