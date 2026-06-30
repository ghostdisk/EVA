#pragma once
#include <EVA/Common.hpp>

typedef U16 ObjectType;

enum ObjectFlagBits : U16
{
	ObjectFlags_None  = 0x0000,
	ObjectFlags_Alive = 0x0001,
};
typedef U16 ObjectFlags;

struct Object
{
	ObjectType  type;
	ObjectFlags flags;
};