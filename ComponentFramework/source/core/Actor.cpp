#include <Core/Actor.h>
#include <Core/Debug.h>
#include <physics/TransformComponent.h>
Actor::Actor(std::weak_ptr<Component> parent_):Component(parent_) {}

bool Actor::OnCreate() {
	if (isCreated) return true;
	Debug::Info("Loading assets for Actor: ", __FILE__, __LINE__);
	for (auto component : components) {
		if (component->OnCreate() == false) {
			Debug::Error("Component failed OnCreate: " + std::string(typeid(*component).name()), __FILE__, __LINE__);
			isCreated = false;
			return isCreated;
		}
	}
	isCreated = true;
	return isCreated;
}

Actor::~Actor() {
	OnDestroy();
}

void Actor::OnDestroy() {
	Debug::Info("Deleting assets for Actor: ", __FILE__, __LINE__);
	RemoveAllComponents();
	isCreated = false;
}



void Actor::Update(const float deltaTime) {
	std::cout << "Hello from Update\n";
}

void Actor::Render()const {}


void Actor::RemoveAllComponents() {
	components.clear();
}

void Actor::ListComponents() const {
	std::cout << typeid(*this).name() << " contains the following components:\n";
	for (const auto& component : components) {
		std::cout << typeid(*component).name() << std::endl;
	}
	std::cout << '\n';
}


MATH::Matrix4 Actor::GetModelMatrix() {

	MATH::Matrix4 modelMatrix;
	Ref<TransformComponent> transform = GetComponent<TransformComponent>();

	if (transform) {
		modelMatrix = transform->GetTransformMatrix();
	}
	else {
		modelMatrix.loadIdentity();
	}

	if (parent.lock()) {
		// Use regular dynamic_cast for raw pointers, not shared_ptr casts
		Actor* parentActor = dynamic_cast<Actor*>(parent.lock().get());
		if (parentActor) {
			auto parentTransform = parentActor->GetComponent<TransformComponent>();
			if (parentTransform) {
				modelMatrix = parentTransform->GetTranslate_Rotate_Matrix() * modelMatrix;
			}
		}
	}
	return modelMatrix;
}
