#pragma once
#include <EVA/Common.hpp>

enum S2CMessageType : U8
{
	S2CMessageType_None,
	S2CMessageType_EntityCreate,
	S2CMessageType_EntityAttachPhysicsBody,
	S2CMessageType_EntitySetTransform,
};

