#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/String.hpp>
#include <EVA/Core/Result.hpp>
#include <EVA/Core/Vector.hpp>

struct Allocator;
class Object;
class Serializer;
class Deserializer;

template<typename T>
T FromString(String str);

class Type {
public:
	ZTString           name          = {};
	Type*              parent_type   = nullptr;
	Vector<Type*>      subclasses    = {};
	void*              defaultObject = {};

	void* (*Instantiate)(Allocator allocator) = nullptr;
	static Type* Find(String name);
};

struct EnumValue {
	ZTString string = {};
	I64      value  = 0;
};

class EnumType : public Type {
public:
	Vector<EnumValue> values = {};

	EnumType(ZTString name, Vector<EnumValue> values) : values(values) {
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

// TODO: The fact that these has to be outside the NS is fucking stupid.

#define EAUTO_SERIALIZE(T) \
	EAUTO void Serialize(Serializer& s, const T& value); \
	EAUTO void Deserialize(Deserializer& d, T& value); \

#define EAUTO_ENUM(T) \
	EAUTO_SERIALIZE(T) \
	EAUTO ZTString ToString(T& t); \
	template<> EAUTO T FromString<T>(String str); \

#define ECLASS_COMMON(T) \
	static Type* StaticClass(); \
	static T* StaticDefaultObject() { return (T*)StaticClass()->defaultObject; } \
	virtual Type* GetClass() override; \

class Object {
public:
	static Type* StaticClass();
	static Object* StaticDefaultObject() { return (Object*)StaticClass()->defaultObject; }
	virtual Type* GetClass();
	virtual ~Object() {}
};
