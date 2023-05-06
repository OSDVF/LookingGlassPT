#include "SceneObjects.h"
#include <glm/packing.hpp>

float FastTriangle::calculateArea() const {
	auto classic = toClassic();
	return glm::length(glm::cross(classic[2] - classic[0], classic[1] - classic[0])) * 0.5f;
}

FastTriangleFirstHalf FastTriangle::firstHalf() const
{
	return {
		v0,
		edgeA,
	};
}
FastTriangleSecondHalf FastTriangle::secondHalf() const
{
	/*union
	{
		u_char components[4];
		uint32_t asUint;
	};

	// Pack normal
	components[3] = normal.x * 255;
	components[2] = normal.y * 255;
	components[1] = normal.z * 255;*/

	return {
		edgeB,
		0,
		indices,
		objectIndex
	};
}

std::array<glm::vec3, 3>FastTriangle::toClassic() const
{
	return {
		v0,
		v0 - edgeA,
		v0 + edgeB
	};
}

FastTriangle toFast(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::uvec3 indices, uint32_t objectIndex)
{
	glm::vec3 edgeA = v0 - v1;
	glm::vec3 edgeB = v2 - v0;
	glm::vec3 normal = glm::cross(edgeA, edgeB);

	return {
		v0,
		edgeA,
		edgeB,
		indices,
		objectIndex,
	};
}

FastTriangle toFast(FastTriangleFirstHalf first, FastTriangleSecondHalf second) {
	return {
		first.v0,
		first.edgeA,
		second.edgeB,
		second.indices,
		second.objectIndex,
	};
}

std::ostream& operator<<(std::ostream& os, const color& c)
{
	os << c.r << ',' << c.g << ',' << c.b << ',' << c.a;
	return os;
}