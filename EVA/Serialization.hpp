#pragma once
#include <EVA/Common.hpp>
#include <EVA/String.hpp>
#include <EVA/Result.hpp>
#include <EVA/Arena.hpp>

// example usage - serialize to { "foo": { "a": 3, "b": 5, "c": [100,200,300] } }
// serializer.BeginObject();
// serializer.Key("foo");
// serializer.BeginObject();
// serializer.Key("a");
// serializer.SerializeU32(3)
// serializer.Key("b");
// serializer.SerializeU32(5)
// serializer.EndObject();
// serializer.Key("c");
// serializer.BeginArray(3);
// serializer.SerializeU32(100);
// serializer.SerializeU32(200);
// serializer.SerializeU32(300);
// serializer.EndArray();
// serializer.EndObject();
// NOTE: Keys are only for debugging and visualization purposes - the binary serializer completely ignores them, so
//       property order is very important.

struct Serializer {
	virtual void BeginObject() = 0;
	virtual void EndObject() = 0;
	virtual void Key(String key) = 0;

	virtual void BeginArray(U32 size) = 0;
	virtual void EndArray() = 0;

	virtual void SerializeU8     (U8 value) = 0;
	virtual void SerializeU16    (U16 value) = 0;
	virtual void SerializeU32    (U32 value) = 0;
	virtual void SerializeU64    (U64 value) = 0;
	virtual void SerializeI8     (U8 value) = 0;
	virtual void SerializeI16    (I16 value) = 0;
	virtual void SerializeI32    (I32 value) = 0;
	virtual void SerializeI64    (I64 value) = 0;
	virtual void SerializeF32    (F32 value) = 0;
	virtual void SerializeF64    (F64 value) = 0;
	virtual void SerializeString (String value) = 0;
};

struct Deserializer {
	virtual Result GetResult();

	virtual void BeginObject() = 0;
	virtual void EndObject() = 0;
	virtual void Key(String key) = 0;

	virtual void BeginArray(U32 size) = 0;
	virtual void EndArray() = 0;

	virtual U8       DeserializeU8     () = 0;
	virtual U16      DeserializeU16    () = 0;
	virtual U32      DeserializeU32    () = 0;
	virtual U64      DeserializeU64    () = 0;
	virtual U8       DeserializeI8     () = 0;
	virtual I16      DeserializeI16    () = 0;
	virtual I32      DeserializeI32    () = 0;
	virtual I64      DeserializeI64    () = 0;
	virtual F32      DeserializeF32    () = 0;
	virtual F64      DeserializeF64    () = 0;
	virtual ZTString DeserializeString () = 0;
};
