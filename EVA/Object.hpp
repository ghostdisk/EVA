#pragma once
#include <EVA/Common.hpp>

class Object;

class Type {
public:
	Type*       parent_type = nullptr;
	const char* name        = {};
	Object* (*Instantiate)()   = nullptr;
};

#ifdef EVAGEN
#define ECLASS(...) [[clang::annotate("eclass " #__VA_ARGS__)]]
#else
#define ECLASS(...)
#endif
#define EPROPERTY(...)

#define ECLASS_COMMON() \
	static Type* StaticClass(); \
	virtual Type* GetClass() override;

class ECLASS() Object {
public:
	static Type* StaticClass();
	virtual Type* GetClass();
	virtual ~Object() {}
};