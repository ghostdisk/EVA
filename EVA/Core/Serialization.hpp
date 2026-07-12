#pragma once
#include <EVA/Core/Basic.hpp>
#include <stdio.h>
#include <vector>

/**
 ** Example usage - serialize an object to { "foo": { "a": 3, "b": 5, "c": [100,200,300] } }
 **
 ** serializer.BeginObject();
 **     serializer.Key("foo");
 **     serializer.BeginObject();
 **         serializer.Key("a");
 **         serializer.SerializeU32(3)
 **         serializer.Key("b");
 **         serializer.SerializeU32(5)
 **     serializer.EndObject();
 **     serializer.Key("c");
 **     serializer.BeginArray(3);
 **         serializer.SerializeU32(100);
 **         serializer.SerializeU32(200);
 **         serializer.SerializeU32(300);
 **     serializer.EndArray();
 ** serializer.EndObject();
 **
 ** NOTE: Keys are only for debugging and visualization purposes - the binary serializer completely ignores them, so
 **       property order is very important.
 **/

class Serializer {
public:
	virtual void BeginObject() = 0;
	virtual void EndObject() = 0;
	virtual void Key(String key) = 0;

	virtual void BeginArray(U32 size) = 0;
	virtual void EndArray() = 0;

	virtual void BeginFixedSizeArray(U32 size) = 0; 
	virtual void EndFixedSizeArray() = 0;

	virtual void SerializeU8     (U8 value) = 0;
	virtual void SerializeU16    (U16 value) = 0;
	virtual void SerializeU32    (U32 value) = 0;
	virtual void SerializeU64    (U64 value) = 0;
	virtual void SerializeI8     (I8 value) = 0;
	virtual void SerializeI16    (I16 value) = 0;
	virtual void SerializeI32    (I32 value) = 0;
	virtual void SerializeI64    (I64 value) = 0;
	virtual void SerializeF32    (F32 value) = 0;
	virtual void SerializeF64    (F64 value) = 0;
	virtual void SerializeString (String value) = 0;
	virtual void SerializeBool   (bool value) = 0;
	virtual void SerializeNull   () = 0;
	virtual void SerializeBytes  (U8* bytes, size_t size) = 0;
};

class Deserializer {
public:
	Result res = {};

	void SetError(Result err) {
		res = err;
	}

	/**
	 ** Parses either an object or null. Returns true on object and false on null.
	 ** In case of null, EndObject must not be called.
	 **/
	virtual bool BeginObject() = 0;

	virtual void EndObject() = 0;
	virtual void Key(String key) = 0;

	virtual U32 BeginArray() = 0;
	virtual void EndArray() = 0;

	virtual void BeginFixedSizeArray(U32 size) = 0;
	virtual void EndFixedSizeArray() = 0;

	virtual U8       DeserializeU8     () = 0;
	virtual U16      DeserializeU16    () = 0;
	virtual U32      DeserializeU32    () = 0;
	virtual U64      DeserializeU64    () = 0;
	virtual I8       DeserializeI8     () = 0;
	virtual I16      DeserializeI16    () = 0;
	virtual I32      DeserializeI32    () = 0;
	virtual I64      DeserializeI64    () = 0;
	virtual F32      DeserializeF32    () = 0;
	virtual F64      DeserializeF64    () = 0;
	virtual ZTString DeserializeString () = 0;
	virtual bool     DeserializeBool   () = 0;
	virtual size_t   DeserializeBytes1 () = 0;
	virtual void     DeserializeBytes2 (U8* out_bytes) = 0;
};

class TextSerializer : public Serializer {

	struct StackEntry {
		char type = '\0';
		bool oneline = false;
		bool has_some = false;
	};

	std::vector<StackEntry> stack;
	FILE* out = nullptr;

	void WriteNewLine();
	void NextValue();

public:
	TextSerializer(FILE* out);

	virtual void BeginObject() override;
	virtual void EndObject() override;
	virtual void Key(String key) override;

	virtual void BeginArray(U32 size) override;
	virtual void EndArray() override;

	virtual void BeginFixedSizeArray(U32 size) override;
	virtual void EndFixedSizeArray() override;

	virtual void SerializeU8     (U8 value) override;
	virtual void SerializeU16    (U16 value) override;
	virtual void SerializeU32    (U32 value) override;
	virtual void SerializeU64    (U64 value) override;
	virtual void SerializeI8     (I8 value) override;
	virtual void SerializeI16    (I16 value) override;
	virtual void SerializeI32    (I32 value) override;
	virtual void SerializeI64    (I64 value) override;
	virtual void SerializeF32    (F32 value) override;
	virtual void SerializeF64    (F64 value) override;
	virtual void SerializeString (String value) override;
	virtual void SerializeBool   (bool value) override;
	virtual void SerializeNull   () override;
	virtual void SerializeBytes  (U8* bytes, size_t size) override;
};

class TextDeserializer : public Deserializer {
	Result result = {};
	FILE* in = nullptr;

	struct StackEntry {
		char type = '\0';
	};

	std::vector<StackEntry> stack;

	void NextValue();

public:
	void SkipWhitespace();
	TextDeserializer(FILE* in);

	virtual bool     BeginObject() override;
	virtual void     EndObject() override;
	virtual void     Key(String key) override;
	virtual U32      BeginArray() override;
	virtual void     EndArray() override;
	virtual void     BeginFixedSizeArray(U32 size) override;
	virtual void     EndFixedSizeArray() override;
	virtual U8       DeserializeU8() override;
	virtual U16      DeserializeU16() override;
	virtual U32      DeserializeU32() override;
	virtual U64      DeserializeU64() override;
	virtual I8       DeserializeI8() override;
	virtual I16      DeserializeI16() override;
	virtual I32      DeserializeI32() override;
	virtual I64      DeserializeI64() override;
	virtual F32      DeserializeF32() override;
	virtual F64      DeserializeF64() override;
	virtual ZTString DeserializeString() override;
	virtual bool     DeserializeBool() override;
	virtual size_t   DeserializeBytes1() override;
	virtual void     DeserializeBytes2(U8* out_bytes) override;
};

inline void Serialize(Serializer& s, const U8&     value)  { s.SerializeU8     (value); }
inline void Serialize(Serializer& s, const U16&    value)  { s.SerializeU16    (value); }
inline void Serialize(Serializer& s, const U32&    value)  { s.SerializeU32    (value); }
inline void Serialize(Serializer& s, const U64&    value)  { s.SerializeU64    (value); }
inline void Serialize(Serializer& s, const I8&     value)  { s.SerializeI8     (value); }
inline void Serialize(Serializer& s, const I16&    value)  { s.SerializeI16    (value); }
inline void Serialize(Serializer& s, const I32&    value)  { s.SerializeI32    (value); }
inline void Serialize(Serializer& s, const I64&    value)  { s.SerializeI64    (value); }
inline void Serialize(Serializer& s, const F32&    value)  { s.SerializeF32    (value); }
inline void Serialize(Serializer& s, const F64&    value)  { s.SerializeF64    (value); }
inline void Serialize(Serializer& s, const String& value)  { s.SerializeString (value); }
inline void Serialize(Serializer& s, const bool&   value)  { s.SerializeBool   (value); }

inline void Deserialize(Deserializer& s, U8&     out_value)  { out_value = s.DeserializeU8     ();  }
inline void Deserialize(Deserializer& s, U16&    out_value)  { out_value = s.DeserializeU16    ();  }
inline void Deserialize(Deserializer& s, U32&    out_value)  { out_value = s.DeserializeU32    ();  }
inline void Deserialize(Deserializer& s, U64&    out_value)  { out_value = s.DeserializeU64    ();  }
inline void Deserialize(Deserializer& s, I8&     out_value)  { out_value = s.DeserializeI8     ();  }
inline void Deserialize(Deserializer& s, I16&    out_value)  { out_value = s.DeserializeI16    ();  }
inline void Deserialize(Deserializer& s, I32&    out_value)  { out_value = s.DeserializeI32    ();  }
inline void Deserialize(Deserializer& s, I64&    out_value)  { out_value = s.DeserializeI64    ();  }
inline void Deserialize(Deserializer& s, F32&    out_value)  { out_value = s.DeserializeF32    ();  }
inline void Deserialize(Deserializer& s, F64&    out_value)  { out_value = s.DeserializeF64    ();  }
inline void Deserialize(Deserializer& s, String& out_value)  { out_value = s.DeserializeString ();  }
inline void Deserialize(Deserializer& s, bool&   out_value)  { out_value = s.DeserializeBool   ();  }