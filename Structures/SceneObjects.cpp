#include "SceneObjects.h"

float FastTriangle::calculateArea() const {
	auto classic = toClassic();
	return glm::length(glm::cross(classic[2] - classic[0], classic[1] - classic[0])) * 0.5f;
}

std::array<glm::vec3, 3>FastTriangle::toClassic() const
{
	return {
		v0,
		v0 - edgeA,
		v0 + edgeB
	};
}

FastTriangle toFast(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::uvec3 indices)
{
	glm::vec3 edgeA = v0 - v1;
	glm::vec3 edgeB = v2 - v0;

	return FastTriangle(
		v0,
		edgeA,
		edgeB,
		glm::cross(edgeA, edgeB),
		glm::vec4(indices, 0)
	);
}

std::ostream& operator<<(std::ostream& os, const color& c)
{
	os << c.r << ',' << c.g << ',' << c.b << ',' << c.a;
	return os;
}