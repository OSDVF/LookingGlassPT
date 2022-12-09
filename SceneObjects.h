#pragma once
#include <cstdint>
#include <bitset>
#include <type_traits>
#include <glm/glm.hpp>
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

struct color {
	float r, g, b;
};
std::ostream& operator<<(std::ostream& os, const color& c)
{
	os << c.r << ',' << c.g << ',' << c.b;
	return os;
}
typedef uint64_t textureHandle;

template <
	typename albedoT,
	typename roughnessT,
	typename metallnessT,
	typename emmisionT,
	typename occlussionT
>
struct Material {
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
	Material(albedoT albedo,
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
	uint32_t vboStartIndex;
	uint32_t indexIndex;
	uint32_t triNumber;
	glm::uvec4 attrSizes;
};

class anySized
{
public:
	void* data = nullptr;
	std::size_t size = 0;
	const std::type_info& type;
	template <typename T>
	anySized(T&& item) :
		size{ sizeof(std::decay_t<T>) },
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
};

/// https://github.com/embree/embree/blob/master/kernels/geometry/triangle.h
struct FastTriangle {
	glm::vec3 v0;
	glm::vec3 edgeA;
	glm::vec3 edgeB;
	glm::vec3 normal;
	glm::uvec4 indices;//Indices of other vertex attributes
};

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