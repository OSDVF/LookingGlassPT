#pragma once
#include <cstdint>
#include <bitset>
#include <type_traits>
#include <glm/glm.hpp>
#include <sstream>

inline std::string debugArray(const char* data, size_t len) {
	std::stringstream out;
	for (size_t i = 0; i < len; ++i)
		out << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (((int)data[i]) & 0xFF) << " ";
	out << std::endl;
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
struct SceneObject {
	using AttrList = StaticLinkedList<VertexAttrs...>;
	uint32_t materialNumber;
	uint32_t vboPosition;
	uint32_t modelBaseIndex;
	uint32_t indicesCount;

	uint32_t vertexAttributeCount;
	uint32_t totalVertexSize = 0;
	AttrList attributeSizes;

	SceneObject(uint32_t materialNumber,
		uint32_t vboPosition,
		uint32_t modelBaseIndex,
		uint32_t indicesCount)
		:
		materialNumber(materialNumber),
		vboPosition(vboPosition),
		modelBaseIndex(modelBaseIndex),
		indicesCount(indicesCount),
		vertexAttributeCount(sizeof...(VertexAttrs)),
		totalVertexSize((VertexAttrs + ...))
	{
	}

	std::stringstream to_print() {
		std::stringstream s;
		s << "MN: " << materialNumber << std::endl;
		s << "I: " << modelBaseIndex << std::endl;
		s << "IC: " << indicesCount << std::endl;
		s << "A: " << vertexAttributeCount << std::endl;

		((s << VertexAttrs << ' '), ...);
		s << std::endl;
		return s;
	}
};

class anySized
{
public:
	std::byte* data;
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
	~anySized()
	{
		delete[] data;
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
	template <typename T>
	void push(const T&& item) {
		auto s = sizeof(std::decay_t<T>);
		totalSize += s;
		types.push_back({ s, typeid(T) });
		buffer.write(
			reinterpret_cast<const char*>(item.data),
			item.size
		);
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
	std::stringstream debug()
	{
		std::stringstream s;
		std::size_t i = 0;
		std::size_t offset = 0;
		for (auto& type : types)
		{
			s << i << ": " << type.type.name() << std::endl;
			s << debugArray(buffer.view().data(), buffer.view().size()) << std::endl;
			//s << ((Printable*)buffer.view().substr(offset, type.size).data())->to_print().rdbuf();
			s << "(" << type.size << " bytes)" << std::endl;
			i++;
		}
		s << std::endl;
		return s;
	}
};