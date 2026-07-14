#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/String.hpp>
#include <EVA/Core/Result.hpp>
#include <vector>

struct Allocator;
class Object;
class Serializer;
class Deserializer;

class Type {
public:
	ZTString           name        = {};
	Type*              parent_type = nullptr;
	std::vector<Type*> subclasses  = {};

	void* (*Instantiate)(Allocator allocator) = nullptr;
	static Type* Find(String name);
};

#ifdef EVAGEN
#	define EAUTO          [[clang::annotate("eauto")]]
#	define EVERSION(...)  [[clang::annotate("eversion " #__VA_ARGS__)]]
#else
#	define EVERSION(...)
#	define EAUTO
#endif

#define ECLASS_COMMON() \
	static Type* StaticClass(); \
	virtual Type* GetClass() override;

class Object {
public:
	static Type* StaticClass();
	virtual Type* GetClass();
	virtual ~Object() {}
};
