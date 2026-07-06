#pragma once
#include <EVA/Common.hpp>

class Type {
public:
	Type*       parent_type = nullptr;
	const char* name        = {};
};

#ifdef EVAGEN
#define ECLASS(...) [[clang::annotate("eclass " #__VA_ARGS__)]]
#else
#define ECLASS(...)
#endif
#define EPROPERTY(...)

#define ECLASS_COMMON() \
	static Type g_type; \
	virtual const Type& GetType() override;

class ECLASS() Object {
public:
	static Type g_type;
	virtual const Type& GetType();
	virtual ~Object() {}
};