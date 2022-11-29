#pragma once
#include "MouseCamera.h"
#include <glm/glm.hpp>

class FirstPersonController
{
public:
	// Virtual camera which is used in the raytracer
	MouseCamera Camera;
	bool CanJump = true;
	float WalkSpeed = 0.00002;
	float RunSpeed = 0.0001;
	float AirModifier = 1.5f;
	float Friction = 0.86f;
	float JumpForce = 2;

	bool jump = false;
	bool running = false;
	// Currently choosen speed
	float speed = 0;
	// Current physical velocity of the controller
	glm::vec3 velocity;
	bool isMoving = true;
	bool isJumping = false;

	void Update(float deltaTime, bool mouseLocked, SDL_Event event);
};
