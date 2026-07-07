#pragma once
#include <EVA/Common.hpp>
#include <EVA/String.hpp>
#include <EVA/Result.hpp>

class Object;
class Serializer;
class Serializer;
class Deserializer;

class Type {
public:
	Type*    parent_type = nullptr;
	ZTString name        = {};
	Object* (*Instantiate)()   = nullptr;
};

#ifdef EVAGEN
#define ECLASS(...) [[clang::annotate("eclass " #__VA_ARGS__)]]
#define EPROPERTY(...) [[clang::annotate("eproperty " #__VA_ARGS__)]]
#else
#define ECLASS(...)
#define EPROPERTY(...)
#endif

#define ECLASS_COMMON() \
	static Type* StaticClass(); \
	virtual Type* GetClass() override; \
	virtual void Serialize(Serializer& serializer) override; \
	virtual Result Deserialize(Deserializer& serializer) override; 

class ECLASS() Object {
public:
	static Type* StaticClass();
	virtual Type* GetClass();
	virtual ~Object() {}
	virtual void Serialize(Serializer& serializer) {}
	virtual Result Deserialize(Deserializer& serializer) { return Success(); }
};