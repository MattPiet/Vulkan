#pragma once
#include <Core/Component.h>
#include "Matrix.h"
#include "QMath.h"
#include "Euler.h"
#include "MMath.h"
using namespace MATH;
class TransformComponent : public Component {
private:
	Vec3 pos;
	Vec3 scale;
	Quaternion orientation;
 
public:
	TransformComponent(std::weak_ptr<Component> parent_);
	TransformComponent(std::weak_ptr<Component> parent_,Vec3 pos_, Quaternion orientation_, Vec3 scale_ = Vec3(1.0f, 1.0f, 1.0f));
	~TransformComponent();

	bool OnCreate() override;
	void OnDestroy() override;
	void Update(const float deltaTime_) override;
	void Render() const override;

	Vec3 GetPosition() { return pos; }
	Vec3 GetScale() { return scale; }
	Quaternion GetQuaternion() { return orientation; }
	Matrix4 GetTransformMatrix() const;
	Matrix4 GetMoveMatrix() const {
		return MMath::translate(pos);
	}
	Matrix4 GetTranslate_Rotate_Matrix() const {
		return MMath::translate(pos)
			* MMath::toMatrix4(orientation);
	}
	void SetTransform(Vec3 pos_, Quaternion orientation_, Vec3 scale_ = Vec3(1.0f, 1.0f, 1.0f) ) {
		pos = pos_;
		orientation = orientation_;
		scale = scale_;
	}
	void SetPosition(const Vec3& pos_) { pos = pos_; }
	void SetScale(const Vec3& scale_) { scale = scale_; }
	void SetOrientation(const Quaternion& orientation_) { orientation = orientation_; }

};

