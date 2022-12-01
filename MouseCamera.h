#pragma once
// Code reused from
// https://github.com/openblack/openblack/blob/5cda3b2f584701331a875cdb40b717f5a0241cab/src/3D/Camera.h
/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SDL2/SDL.h>
#include <chrono>
#include <memory>

class MouseCamera
{

public:
	MouseCamera(glm::vec3 position, glm::vec3 rotation)
		: _position(position)
		, _rotation(glm::radians(rotation))
		, _projectionMatrix(1.0f)
		, _previousMouseState()
	{
	}
	MouseCamera()
		: MouseCamera(glm::vec3(0.0f), glm::vec3(0.0f))
	{
	}
	float Sensitivity = 1.0f;
	float Aspect = 1.0f;

	virtual ~MouseCamera() = default;

	[[nodiscard]] virtual glm::mat4 GetViewMatrix() const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix() const { return _projectionMatrix; }
	[[nodiscard]] virtual glm::mat4 GetViewProjectionMatrix() const;

	[[nodiscard]] glm::vec3 GetPosition() const { return _position; }
	[[nodiscard]] glm::vec3 GetRotation() const { return glm::degrees(_rotation); }

	void SetPosition(const glm::vec3& position) { _position = position; }
	void SetRotation(const glm::vec3& eulerDegrees) { _rotation = glm::radians(eulerDegrees); }

	void SetProjectionMatrixPerspective(float fov, float aspect, float nearclip, float farclip);
	void SetProjectionMatrix(const glm::mat4x4& projection) { _projectionMatrix = projection; }

	[[nodiscard]] glm::vec3 GetForward() const;
	[[nodiscard]] glm::vec3 GetRight() const;
	[[nodiscard]] glm::vec3 GetUp() const;


	void DeprojectScreenToWorld(const glm::ivec2 screenPosition, const glm::ivec2 screenSize, glm::vec3& out_worldOrigin,
		glm::vec3& out_worldDirection);
	bool ProjectWorldToScreen(const glm::vec3 worldPosition, const glm::vec4 viewport, glm::vec3& out_screenPosition) const;

	void handleMouseInput(bool mouseLocked, float deltaTime, SDL_MouseMotionEvent mouseState);

	[[nodiscard]] glm::mat4 getRotationMatrix() const;

	void SetCenter(glm::vec2 center)
	{
		_previousMouseState.x = center.x;
		_previousMouseState.y = center.y;
	}

protected:
	glm::vec3 _position;
	glm::vec3 _rotation;

	glm::mat4 _projectionMatrix;

	SDL_MouseMotionEvent _previousMouseState;
	bool _wasLocked = false;
};