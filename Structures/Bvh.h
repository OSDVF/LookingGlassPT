/**
* Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/BVHBuilder.h
*/
#pragma once

#include "../PrecompiledHeaders.hpp"
#include <GL/glew.h>
#include <vector>
#include "./SceneObjects.h"

struct BVHNode
{
	static const GLuint LeafMask = 0x80000000;
	static const GLuint InvalidMask = 0xFFFFFFFF;

	glm::vec3 bboxMin;
	GLuint triangleIndex = InvalidMask;

	glm::vec3 bboxMax;
	GLuint next = InvalidMask;

	bool isLeaf() const { return triangleIndex != InvalidMask; }
};

struct BVHPackedNode
{
	GLuint a, b, c, d;
};

struct BVHBuilder
{
	std::vector<BVHNode> m_nodes;
	std::vector<BVHPackedNode> m_packedNodes;
	void build(std::vector<FastTriangle> triangles);
};