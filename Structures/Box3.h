#pragma once
#include "../PrecompiledHeaders.hpp"

class Box3 {
public:
	glm::vec3 m_min;
	glm::vec3 m_max;

	void expand(const Box3& rhs);

	/// Expands this bounding box to contain the vector
	void expand(const glm::vec3& rhs);

	glm::vec3 dimensions() const;

	glm::vec3 center() const;

	/// <summary>
	///  Initializes the bounding box to the minimum possible size (-INFINITY)
	/// </summary>
	void expandInit();
};