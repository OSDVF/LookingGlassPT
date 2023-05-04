#include "Box3.h"

void Box3::expand(const Box3& rhs)
{
	m_min.x = std::min(m_min.x, rhs.m_min.x);
	m_min.y = std::min(m_min.y, rhs.m_min.y);
	m_min.z = std::min(m_min.z, rhs.m_min.z);

	m_max.x = std::max(m_max.x, rhs.m_max.x);
	m_max.y = std::max(m_max.y, rhs.m_max.y);
	m_max.z = std::max(m_max.z, rhs.m_max.z);
}

void Box3::expand(const glm::vec3& rhs)
{
	m_min.x = std::min(m_min.x, rhs.x);
	m_min.y = std::min(m_min.y, rhs.y);
	m_min.z = std::min(m_min.z, rhs.z);

	m_max.x = std::max(m_max.x, rhs.x);
	m_max.y = std::max(m_max.y, rhs.y);
	m_max.z = std::max(m_max.z, rhs.z);
}

void Box3::expandInit()
{
	m_min = glm::vec3(FLT_MAX);
	m_max = glm::vec3(-FLT_MAX);
}

glm::vec3 Box3::dimensions() const
{
	return m_max - m_min;
}

glm::vec3 Box3::center() const
{
	return (m_max + m_min) * 0.5f;
}