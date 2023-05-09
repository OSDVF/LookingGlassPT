#pragma once
#include "../PrecompiledHeaders.hpp"
#include <cstdint>
#include <bitset>
#include <type_traits>
#include <sstream>

inline std::string debugArray(const char* data, size_t len, bool fl) {
	std::stringstream out;
	if (fl)
	{
		for (size_t i = 0; i < len; i += sizeof(float))
		{
			out << *(float*)&data[i] << ' ';
		}
	}
	else
	{
		for (size_t i = 0; i < len; ++i)
		{
			out << std::uppercase << std::hex << std::setfill('0') <<
				std::setw(2) << (((int)data[i]) & 0xFF) << " ";
		}
	}
	return out.str();
}

using color = glm::vec4;
std::ostream& operator<<(std::ostream& os, const color& c);
using textureHandle = uint64_t;

template <
	typename albedoT,
	typename roughnessT,
	typename metallnessT,
	typename emmisionT,
	typename occlussionT
>
struct DynamicMaterial {
	const uint32_t textureTypesMask =
		(
			((std::is_same<albedoT, color>::value ? 0 : 1) << 0)
			|
			((std::is_same<roughnessT, color>::value ? 0 : 1) << 1)
			|
			((std::is_same<metallnessT, color>::value ? 0 : 1) << 2)
			|
			((std::is_same<emmisionT, color>::value ? 0 : 1) << 3)
			|
			((std::is_same<occlussionT, color>::value ? 0 : 1) << 4)
			);
	albedoT albedo;
	roughnessT roughness;
	metallnessT metallness;
	emmisionT emmision;
	occlussionT occlussion;
	DynamicMaterial(albedoT albedo,
		roughnessT roughness,
		metallnessT metallness,
		emmisionT emmision,
		occlussionT occlussion) :
		albedo(albedo),
		roughness(roughness),
		metallness(metallness),
		emmision(emmision),
		occlussion(occlussion)
	{

	}


	std::stringstream to_print() {
		std::stringstream s;
		s << "A: " << albedo << std::endl;
		s << "R: " << roughness << std::endl;
		s << "M: " << metallness << std::endl;
		s << "E: " << emmision << std::endl;
		s << "O: " << occlussion << std::endl;
		s << "T: " << std::bitset<5>(textureTypesMask) << std::endl;
		return s;
	}
};

struct Material {
	uint32_t isTexture;
	glm::vec3 colorOrHandle;
	glm::vec3 emissive = glm::vec3(0.);
	float transparency = 1;

	Material()
	{}

	Material(glm::uint64 handle)
	{
		isTexture = 1u;
		this->colorOrHandle = glm::vec3(
			glm::uintBitsToFloat((handle >> 32u) & 0xFFFFFFFFu),
			glm::uintBitsToFloat(handle & 0xFFFFFFFFu), 0.f
		);
	}

	Material(glm::vec3 color)
	{
		isTexture = 0u;
		this->colorOrHandle = color;
	}

	void setEmissive(glm::vec3 e)
	{
		isTexture &= ~(1u << 1u);
		emissive = e;
	}
	void setEmissive(glm::uint64 handle)
	{
		isTexture |= 1u << 1u;
		this->emissive = glm::vec3(
			glm::uintBitsToFloat((handle >> 32u) & 0xFFFFFFFFu),
			glm::uintBitsToFloat(handle & 0xFFFFFFFFu), 0.f
		);
	}
};

template<uint32_t... Items>
struct StaticLinkedList;

template<uint32_t val>
struct StaticLinkedList<val> {
	uint32_t value = val;
};

template<uint32_t val, uint32_t... Rest>
struct StaticLinkedList<val, Rest...> {
	uint32_t value = val;
	StaticLinkedList<Rest...> next;
};

template <uint32_t... VertexAttrs>
struct DynamicSceneObject {
	using AttrList = StaticLinkedList<VertexAttrs...>;
	uint32_t materialNumber;
	uint32_t vboPosition;
	uint32_t iboPosition;
	uint32_t indicesCount;

	uint32_t vertexAttributeCount;
	uint32_t totalVertexSize = 0;
	AttrList attributeSizes;

	DynamicSceneObject(uint32_t materialNumber,
		uint32_t vboPosition,
		uint32_t iboPosition,
		uint32_t indicesCount)
		:
		materialNumber(materialNumber),
		vboPosition(vboPosition),
		iboPosition(iboPosition),
		indicesCount(indicesCount),
		vertexAttributeCount(sizeof...(VertexAttrs)),
		totalVertexSize((VertexAttrs + ...))
	{
	}

	std::stringstream to_print() {
		std::stringstream s;
		s << "MN: " << materialNumber << std::endl;
		s << "V: " << vboPosition << std::endl;
		s << "I: " << iboPosition << std::endl;
		s << "IC: " << indicesCount << std::endl;
		s << "A: " << vertexAttributeCount << std::endl;
		s << "T: " << totalVertexSize << std::endl;

		((s << VertexAttrs << ' '), ...);
		s << std::endl;
		return s;
	}
};

struct SceneObject {
	uint32_t material;
	uint32_t attrBufferPointer;
	uint32_t vertexAttrsMask;
	uint32_t totalAttrSize;

	SceneObject(
		uint32_t material,
		uint32_t vboStartIndex,
		uint32_t indexIndex,
		uint32_t triNumber,
		bool colors, bool normals, bool uvs) :
		material(material),
		attrBufferPointer(vboStartIndex)
	{
		totalAttrSize = 0;
		vertexAttrsMask = 0;
		if (colors)
		{
			vertexAttrsMask |= 1;
			totalAttrSize += 4;
		}
		if (normals)
		{
			vertexAttrsMask |= 2;
			totalAttrSize += 3;
		}
		if (uvs)
		{
			vertexAttrsMask |= 4;
			totalAttrSize += 2;
		}
	}
};

class anySized
{
public:
	std::byte* data = nullptr;
	std::size_t size = 0;
	const std::type_info& type;
	template <typename T>
	anySized(T&& item) :
		size(sizeof(T)),
		type(typeid(T))
	{
		data = new std::byte[size];
		std::memcpy(data, &item, size);
	}
	anySized(const anySized& other) = delete;
	/* :
		type(other.type)
	{
		size = other.size;;
		data = new std::byte[size];
		std::memcpy(data, other.data, size);
	}*/
	~anySized()
	{
		if (data != nullptr)
		{
			delete[] data;
		}
	}
};

// https://stackoverflow.com/a/49730915
class anyVector
{
	struct typeInfo
	{
		std::size_t size;
		const std::type_info& type;
	};
public:
	std::vector<typeInfo> types;
	std::size_t totalSize = 0;
	std::stringstream buffer;

public:
	void push(const anySized&& item) {
		totalSize += item.size;
		types.push_back({ item.size, item.type });
		buffer.write(
			reinterpret_cast<const char*>(item.data),
			item.size
		);
	}

	void push(const anySized& item) {
		totalSize += item.size;
		types.push_back({ item.size, item.type });
		buffer.write(
			reinterpret_cast<const char*>(item.data),
			item.size
		);
	}

	void push(const std::initializer_list<anySized>&& items)
	{
		for (auto&& item : items)
		{
			this->push(item);
		}
	}

	/**
	* Pushes a value into the buffer and increnets a counter by the value's size
	*/
	template<typename CounterT>
	void push(const anySized&& item, CounterT& counter) {
		totalSize += item.size;
		counter += item.size;
		types.push_back({ item.size, item.type });
		buffer.write(
			reinterpret_cast<const char*>(item.data),
			item.size
		);
	}

	void clear()
	{
		types.clear();
		buffer.str("");
		buffer.clear();
		totalSize = 0;
	}

	anyVector(std::initializer_list<anySized>&& items)
	{
		for (auto&& item : items)
		{
			totalSize += item.size;
			types.push_back({ item.size, item.type });
			buffer.write(
				reinterpret_cast<const char*>(item.data),
				item.size
			);
		}
	}
	anyVector()
	{

	}

	std::stringstream debug(bool fl = false)
	{
		std::stringstream s;
		std::size_t i = 0;
		std::size_t offset = 0;
		for (auto& type : types)
		{
			s << i << ": " << type.type.name() << std::endl;
			s << debugArray(buffer.view().data() + offset, type.size, fl) << std::endl;
			//s << ((Printable*)buffer.view().substr(offset, type.size).data())->to_print().rdbuf();
			s << "(" << type.size << " bytes)" << std::endl;
			i++;
			offset += type.size;
		}
		s << std::endl;
		return s;
	}

	std::stringstream debug(std::initializer_list<std::pair<std::size_t, bool>> subSizes)
	{
		std::stringstream s;
		std::size_t i = 0;
		std::size_t offset = 0;
		for (auto& type : types)
		{
			s << i << ": " << type.type.name() << std::endl;
			std::size_t subOffset = 0;
			for (auto& subItem : subSizes)
			{
				s << "subitem at " << subOffset << " of size " << subItem.first << std::endl;
				s << debugArray(buffer.view().data() + offset + subOffset, subItem.first, subItem.second) << std::endl;
				subOffset += subItem.first;
			}
			if (subOffset != type.size)
			{
				s << "remaining:\n";
			}
			s << debugArray(buffer.view().data() + offset + subOffset, type.size - subOffset, false) << std::endl;
			s << "(" << type.size << " bytes)" << std::endl;
			i++;
			offset += type.size;
		}
		s << std::endl;
		return s;
	}
};

struct FastTriangleFirstHalf {
	glm::vec3 v0;
	glm::vec3 edgeA;
};

struct FastTriangleSecondHalf {
	glm::vec3 edgeB;
	uint32_t padding;
	glm::uvec3 indices;
	uint32_t objectIndex;
};

/// https://github.com/embree/embree/blob/master/kernels/geometry/triangle.h combined with BVH purposes
struct FastTriangle {
	glm::vec3 v0;
	glm::vec3 edgeA;
	glm::vec3 edgeB;
	glm::uvec3 indices;//Indices of other vertex attributes
	uint32_t objectIndex;

	std::array<glm::vec3, 3> toClassic() const;

	float calculateArea() const;
	FastTriangleFirstHalf firstHalf() const;
	FastTriangleSecondHalf secondHalf() const;
};

FastTriangle toFast(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::uvec3 indices, uint32_t objectIndex);
FastTriangle toFast(FastTriangleFirstHalf first, FastTriangleSecondHalf second);

struct Light {
	glm::vec3 position;
	float size; //Dimension in both axes
	glm::vec3 normal;
	float area; // size squared
	glm::vec4 emission;
	uint32_t object;
};