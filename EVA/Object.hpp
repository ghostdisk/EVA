#pragma once
#include <EVA/Common.hpp>
#include <EVA/String.hpp>
#include <EVA/Result.hpp>

class Object;
class Serializer;
class Deserializer;

class Type {
public:
	Type*    parent_type = nullptr;
	ZTString name        = {};
	Object* (*Instantiate)()   = nullptr;

	static Type* Find(String name);
};

#ifdef EVAGEN
#	define ECLASS(...)    [[clang::annotate("eclass " #__VA_ARGS__)]]
#	define EPROPERTY(...) [[clang::annotate("eproperty " #__VA_ARGS__)]]
#	define ESERIALIZABLE  [[clang::annotate("eserializable")]]
#else
#	define ECLASS(...) 
#	define EPROPERTY(...)
#	define ESERIALIZABLE
#endif

#define ECLASS_COMMON() \
	static Type* StaticClass(); \
	virtual Type* GetClass() override;

class ECLASS() Object {
public:
	static Type* StaticClass();
	virtual Type* GetClass();
	virtual ~Object() {}
};
