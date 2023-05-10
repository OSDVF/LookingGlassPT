/**
* Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/BVHBuilder.cpp
*/

#include "Bvh.h"
#include "Box3.h"

#include <algorithm>
#include <xmmintrin.h>

namespace
{
	struct TempNode : BVHNode
	{
		GLuint visitOrder = InvalidMask;
		GLuint parent = InvalidMask;

		GLuint left;
		GLuint right;

		glm::vec3 bboxCenter;

		float primArea = 0.0f;

		float surfaceAreaLeft = 0.0f;
		float surfaceAreaRight = 0.0f;
	};

	inline float bboxSurfaceArea(const glm::vec3& bboxMin, const glm::vec3& bboxMax)
	{
		glm::vec3 extents = bboxMax - bboxMin;
		return (extents.x * extents.y + extents.y * extents.z + extents.z * extents.x) * 2.0f;
	}

	inline float bboxSurfaceArea(const Box3& bbox)
	{
		return bboxSurfaceArea(bbox.m_min, bbox.m_max);
	}

	inline void setBounds(BVHNode& node, const glm::vec3& min, const glm::vec3& max)
	{
		node.bboxMin[0] = min.x;
		node.bboxMin[1] = min.y;
		node.bboxMin[2] = min.z;

		node.bboxMax[0] = max.x;
		node.bboxMax[1] = max.y;
		node.bboxMax[2] = max.z;
	}

	inline glm::vec3 extractVec3(__m128 v)
	{
		alignas(16) float temp[4];
		_mm_store_ps(temp, v);
		return glm::vec3(temp[0], temp[1], temp[2]);
	}

	Box3 calculateBounds(std::vector<TempNode>& nodes, GLuint begin, GLuint end)
	{
		Box3 bounds;
		if (begin == end)
		{
			bounds.m_min = glm::vec3(0.0f);
			bounds.m_max = glm::vec3(0.0f);
		}
		else
		{
			__m128 bboxMin = _mm_set1_ps(FLT_MAX);
			__m128 bboxMax = _mm_set1_ps(-FLT_MAX);
			for (GLuint i = begin; i < end; ++i)
			{
				__m128 nodeBoundsMin = _mm_loadu_ps(&nodes[i].bboxMin.x);
				__m128 nodeBoundsMax = _mm_loadu_ps(&nodes[i].bboxMax.x);
				bboxMin = _mm_min_ps(bboxMin, nodeBoundsMin);
				bboxMax = _mm_max_ps(bboxMax, nodeBoundsMax);
			}
			bounds.m_min = extractVec3(bboxMin);
			bounds.m_max = extractVec3(bboxMax);
		}
		return bounds;
	}

	GLuint split(std::vector<TempNode>& nodes, GLuint begin, GLuint end, const Box3& nodeBounds, const unsigned int sahThreshold)
	{
		GLuint count = end - begin;
		GLuint bestSplit = begin;

		if (count <= sahThreshold)
		{
			// Use Surface Area Heuristic
			GLuint bestAxis = 0;
			GLuint globalBestSplit = begin;
			float globalBestCost = FLT_MAX;

			for (GLuint axis = 0; axis < 3; ++axis)
			{
				// TODO: just sort into N buckets
				std::sort(nodes.begin() + begin, nodes.begin() + end,
					[&](const TempNode& a, const TempNode& b)
					{
						return a.bboxCenter[axis] < b.bboxCenter[axis];
					});

				Box3 boundsLeft;
				boundsLeft.expandInit();

				Box3 boundsRight;
				boundsRight.expandInit();

				for (GLuint indexLeft = 0; indexLeft < count; ++indexLeft)
				{
					GLuint indexRight = count - indexLeft - 1;

					boundsLeft.expand(nodes[begin + indexLeft].bboxMin);
					boundsLeft.expand(nodes[begin + indexLeft].bboxMax);

					boundsRight.expand(nodes[begin + indexRight].bboxMin);
					boundsRight.expand(nodes[begin + indexRight].bboxMax);

					float surfaceAreaLeft = bboxSurfaceArea(boundsLeft);
					float surfaceAreaRight = bboxSurfaceArea(boundsRight);

					nodes[begin + indexLeft].surfaceAreaLeft = surfaceAreaLeft;
					nodes[begin + indexRight].surfaceAreaRight = surfaceAreaRight;
				}

				float bestCost = FLT_MAX;
				for (GLuint mid = begin + 1; mid < end; ++mid)
				{
					float surfaceAreaLeft = nodes[mid - 1].surfaceAreaLeft;
					float surfaceAreaRight = nodes[mid].surfaceAreaRight;

					GLuint countLeft = mid - begin;
					GLuint countRight = end - mid;

					float costLeft = surfaceAreaLeft * (float)countLeft;
					float costRight = surfaceAreaRight * (float)countRight;

					float cost = costLeft + costRight;
					if (cost < bestCost)
					{
						bestSplit = mid;
						bestCost = cost;
					}
				}

				if (bestCost < globalBestCost)
				{
					globalBestSplit = bestSplit;
					globalBestCost = bestCost;
					bestAxis = axis;
				}
			}

			std::sort(nodes.begin() + begin, nodes.begin() + end,
				[&](const TempNode& a, const TempNode& b)
				{
					return a.bboxCenter[bestAxis] < b.bboxCenter[bestAxis];
				});

			return globalBestSplit;
		}
		else
		{
			// Use Median Split
			glm::vec3 extents = nodeBounds.dimensions();
			int majorAxis = (int)std::distance(&extents.x, std::max_element(&extents.x, &extents.z));

			std::sort(nodes.begin() + begin, nodes.begin() + end,
				[&](const TempNode& a, const TempNode& b)
				{
					return a.bboxCenter[majorAxis] < b.bboxCenter[majorAxis];
				});

			float splitPos = (nodeBounds.m_min[majorAxis] + nodeBounds.m_max[majorAxis]) * 0.5f;
			for (uint32_t mid = begin + 1; mid < end; ++mid)
			{
				if (nodes[mid].bboxCenter[majorAxis] >= splitPos)
				{
					return mid;
				}
			}

			return end - 1;
		};
	}

	GLuint buildNodeHierarchy(std::vector<TempNode>& nodes, GLuint begin, GLuint end, const unsigned int sahThreshold)
	{
		GLuint count = end - begin;

		if (count == 1)
		{
			return begin;
		}

		Box3 bounds = calculateBounds(nodes, begin, end);

		GLuint mid = split(nodes, begin, end, bounds, sahThreshold);

		GLuint nodeId = (GLuint)nodes.size();
		nodes.push_back(TempNode());

		TempNode node;

		node.left = buildNodeHierarchy(nodes, begin, mid, sahThreshold);
		node.right = buildNodeHierarchy(nodes, mid, end, sahThreshold);

		float surfaceAreaLeft = bboxSurfaceArea(nodes[node.left].bboxMin, nodes[node.left].bboxMax);
		float surfaceAreaRight = bboxSurfaceArea(nodes[node.right].bboxMin, nodes[node.right].bboxMax);

		if (surfaceAreaRight > surfaceAreaLeft)
		{
			std::swap(node.left, node.right);
		}

		setBounds(node, bounds.m_min, bounds.m_max);
		node.bboxCenter = bounds.center();
		node.triangleIndex = BVHNode::InvalidMask;

		nodes[node.left].parent = nodeId;
		nodes[node.right].parent = nodeId;

		nodes[nodeId] = node;

		return nodeId;
	}

	void setDepthFirstVisitOrder(std::vector<TempNode>& nodes, GLuint nodeId, GLuint nextId, GLuint& order)
	{
		TempNode& node = nodes[nodeId];

		node.visitOrder = order++;
		node.next = nextId;

		if (node.left != BVHNode::InvalidMask)
		{
			setDepthFirstVisitOrder(nodes, node.left, node.right, order);
		}

		if (node.right != BVHNode::InvalidMask)
		{
			setDepthFirstVisitOrder(nodes, node.right, nextId, order);
		}
	}

	void setDepthFirstVisitOrder(std::vector<TempNode>& nodes, GLuint root)
	{
		GLuint order = 0;
		setDepthFirstVisitOrder(nodes, root, BVHNode::InvalidMask, order);
	}

}

void BVHBuilder::build(std::vector<FastTriangleFirstHalf> trianglesFirst, std::vector<FastTriangleSecondHalf> trianglesSecond)
{
	auto primCount = trianglesFirst.size();
	m_nodes.clear();
	m_nodes.reserve(primCount * 2 - 1);

	std::vector<TempNode> tempNodes;
	tempNodes.reserve(primCount * 2 - 1);

	// Build leaf nodes
	for (GLuint triangleIndex = 0; triangleIndex < primCount; ++triangleIndex)
	{
		TempNode node;
		Box3 box;
		box.expandInit();

		auto fast = toFast(trianglesFirst[triangleIndex],trianglesSecond[triangleIndex]);
		auto triangle = fast.toClassic();

		box.expand(triangle[0]);
		box.expand(triangle[1]);
		box.expand(triangle[2]);

		node.primArea = fast.calculateArea();

		setBounds(node, box.m_min, box.m_max);

		node.bboxCenter = box.center();
		node.triangleIndex = triangleIndex;
		node.left = BVHNode::InvalidMask;
		node.right = BVHNode::InvalidMask;
		tempNodes.push_back(node);
	}

	const GLuint rootIndex = buildNodeHierarchy(tempNodes, 0, (GLuint)tempNodes.size(), this->sahThreshold);

	//
	// Node reordering to ensure cache optimisation
	//

	setDepthFirstVisitOrder(tempNodes, rootIndex);

	m_nodes.resize(tempNodes.size());

	for (GLuint oldIndex = 0; oldIndex < (GLuint)tempNodes.size(); ++oldIndex)
	{
		const TempNode& oldNode = tempNodes[oldIndex];

		BVHNode& newNode = m_nodes[oldNode.visitOrder];

		glm::vec3 bboxMin(oldNode.bboxMin);
		glm::vec3 bboxMax(oldNode.bboxMax);
		setBounds(newNode, bboxMin, bboxMax);

		newNode.triangleIndex = oldNode.triangleIndex;
		newNode.next = oldNode.next == BVHNode::InvalidMask
			? BVHNode::InvalidMask
			: tempNodes[oldNode.next].visitOrder;
	}

	//
	// Node packing
	//
	m_packedNodes.clear();
	m_packedNodes.reserve(m_nodes.size() + primCount);

	for (GLuint i = 0; i < (GLuint)tempNodes.size(); ++i)
	{
		const BVHNode& node = m_nodes[i];

		if (node.isLeaf())
		{
			struct TriangleHalf {
				glm::vec3 v0;
				GLuint triIndex;
				glm::vec3 edgeA;
				GLuint next;
			};

			TriangleHalf triangleToPacked;

			auto triangleSource = trianglesFirst[node.triangleIndex];
			triangleToPacked.v0 = triangleSource.v0;
			triangleToPacked.triIndex = node.triangleIndex;
			triangleToPacked.edgeA = triangleSource.edgeA;
			triangleToPacked.next = node.next;

			// There are 2 packed nodes per leaf node
			BVHPackedNode data0, data1;
			memcpy(&data0, &triangleToPacked.v0, sizeof(BVHPackedNode));
			memcpy(&data1, &triangleToPacked.edgeA, sizeof(BVHPackedNode));

			m_packedNodes.push_back(data0);
			m_packedNodes.push_back(data1);
		}
		else
		{
			BVHNode packedNode;

			packedNode.bboxMin = node.bboxMin;
			packedNode.triangleIndex = node.triangleIndex;
			packedNode.bboxMax = node.bboxMax;
			packedNode.next = node.next;

			BVHPackedNode data0, data1;
			memcpy(&data0, &packedNode.bboxMin, sizeof(BVHPackedNode));
			memcpy(&data1, &packedNode.bboxMax, sizeof(BVHPackedNode));

			m_packedNodes.push_back(data0);
			m_packedNodes.push_back(data1);
		}
	}
}