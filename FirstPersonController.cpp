#include "FirstPersonController.h"
#include <SDL2/SDL_events.h>

void FirstPersonController::Update(float deltaTime, bool mouseLocked, SDL_Event event) {
	if (event.type == SDL_MOUSEMOTION)
	{
		this->Camera.handleMouseInput(mouseLocked, deltaTime, event.motion);
	}

	auto keyboardState = SDL_GetKeyboardState(nullptr);
	//Speed Modifier
	running = keyboardState[SDL_SCANCODE_LSHIFT];
	if (running == true) {
		speed = RunSpeed;
	}
	else {
		speed = WalkSpeed / AirModifier;
	}

	jump = keyboardState[SDL_SCANCODE_SPACE];
	bool up = keyboardState[SDL_SCANCODE_W];
	bool left = keyboardState[SDL_SCANCODE_A];
	bool right = keyboardState[SDL_SCANCODE_D];
	bool down = keyboardState[SDL_SCANCODE_S];
	bool crouch = keyboardState[SDL_SCANCODE_C];

	auto rightForce = this->Camera.GetRight() * (float)(right - left);
	auto upForce = this->Camera.GetUp() * (float)(jump - crouch);
	auto forwardForce = (this->Camera.GetForward() * (float)(up - down));
	auto moveForce = glm::normalize(rightForce + upForce + forwardForce);

	if (glm::length(moveForce) > 0) {
		isMoving = true;
	}
	else {
		isMoving = false;
		moveForce = glm::vec3(0, 0, 0);
	}

	//Limit Speed
	if (glm::length(velocity) > speed) {
		velocity = normalize(velocity) * speed;
	}

	//
	// Movement
	//

	//Apply Friction
	velocity *= Friction;

	velocity += moveForce * speed;

	this->Camera.SetPosition(this->Camera.GetPosition() + glm::vec3(velocity.x, velocity.y, velocity.z) * deltaTime);
}