#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/String.hpp>
#include <EVA/Core/Result.hpp>
#include <utility>
#include <vector>

struct Allocator;
class Object;
class Serializer;
class Deserializer;

template<typename T>
T FromString(String str);

class Type {
public:
	ZTString           name        = {};
	Type*              parent_type = nullptr;
	std::vector<Type*> subclasses  = {};

	void* (*Instantiate)(Allocator allocator) = nullptr;
	static Type* Find(String name);
};

struct EnumValue {
	ZTString string = {};
	I64      value  = 0;
};

class EnumType : public Type {
public:
	std::vector<EnumValue> values = {};

	EnumType(ZTString name, std::vector<EnumValue> values)
		: values(std::move(values)) {
		this->name = name;
	}
};

#ifdef EVAGEN
#	define EAUTO          [[clang::annotate("eauto")]]
#	define EVERSION(...)  [[clang::annotate("eversion, " #__VA_ARGS__)]]
#else
#	define EVERSION(...)
#	define EAUTO
#endif

#define EAUTO_SERIALIZE(T) \
	EAUTO void Serialize(Serializer& s, const T& value); \
	EAUTO void Deserialize(Deserializer& d, T& value); \

#define EAUTO_ENUM(T) \
	EAUTO_SERIALIZE(T) \
	EAUTO ZTString ToString(T& t); \
	template<> EAUTO T FromString<T>(String str); \

#define ECLASS_COMMON() \
	static Type* StaticClass(); \
	virtual Type* GetClass() override;

class Object {
public:
	static Type* StaticClass();
	virtual Type* GetClass();
	virtual ~Object() {}
};
