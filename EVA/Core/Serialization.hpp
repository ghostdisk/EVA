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
	virtual bool IsText() const = 0;

	virtual void BeginObject() = 0;
	virtual void EndObject() = 0;
	virtual void Key(String key) = 0;

	virtual void BeginArray(U32 size) = 0;
	virtual void EndArray() = 0;

	virtual void BeginFixedSizeArray(U32 size) = 0; 
	virtual void EndFixedSizeArray() = 0;

	virtual void SerializeU8        (U8 value) = 0;
	virtual void SerializeU16       (U16 value) = 0;
	virtual void SerializeU32       (U32 value) = 0;
	virtual void SerializeU64       (U64 value) = 0;
	virtual void SerializeI8        (I8 value) = 0;
	virtual void SerializeI16       (I16 value) = 0;
	virtual void SerializeI32       (I32 value) = 0;
	virtual void SerializeI64       (I64 value) = 0;
	virtual void SerializeF32       (F32 value) = 0;
	virtual void SerializeF64       (F64 value) = 0;
	virtual void SerializeString    (String value) = 0;
	virtual void SerializeBool      (bool value) = 0;
	virtual void SerializeNull      () = 0;
	virtual void SerializeBytes     (const void* bytes, size_t size) = 0;
};

class Deserializer {
public:
	Arena* arena = nullptr;
	Result res = {};
	virtual bool IsText() const = 0;

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

	virtual U8       DeserializeU8        () = 0;
	virtual U16      DeserializeU16       () = 0;
	virtual U32      DeserializeU32       () = 0;
	virtual U64      DeserializeU64       () = 0;
	virtual I8       DeserializeI8        () = 0;
	virtual I16      DeserializeI16       () = 0;
	virtual I32      DeserializeI32       () = 0;
	virtual I64      DeserializeI64       () = 0;
	virtual F32      DeserializeF32       () = 0;
	virtual F64      DeserializeF64       () = 0;
	virtual ZTString DeserializeString    () = 0;
	virtual bool     DeserializeBool      () = 0;
	virtual void     DeserializeBytes     (Allocator allocator, size_t* out_size, void** out_data) = 0;
};

class TextSerializer : public Serializer {

	struct StackEntry {
		char type = '\0';
		bool oneline = false;
		bool has_some = false;
	};

	std::vector<StackEntry> stack;
	FILE* m_out = nullptr;

	void WriteNewLine();
	void NextValue();

public:
	TextSerializer(FILE* out);
	virtual bool IsText() const override { return true; }

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
	virtual void SerializeBytes  (const void* bytes, size_t size) override;
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
	TextDeserializer(FILE* in, Arena* arena = nullptr);
	virtual bool IsText() const override { return true; }

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
	virtual void     DeserializeBytes(Allocator allocator, size_t* out_size, void** out_data) override;
};

struct SerializableBytes {
	void* data = nullptr;
	size_t size = 0;
};


// Having Serialize(s, const T&) and Deserialize(d, T&) versions for the primitive types  makes automatic code generation
// simpler. This way, when [de]serializing fields of a struct, we don't have to care about their type, and can just
// emit Serialize(s, theStruct.field)
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

inline void Deserialize(Deserializer& d, U8&     out_value)  { out_value = d.DeserializeU8     ();  }
inline void Deserialize(Deserializer& d, U16&    out_value)  { out_value = d.DeserializeU16    ();  }
inline void Deserialize(Deserializer& d, U32&    out_value)  { out_value = d.DeserializeU32    ();  }
inline void Deserialize(Deserializer& d, U64&    out_value)  { out_value = d.DeserializeU64    ();  }
inline void Deserialize(Deserializer& d, I8&     out_value)  { out_value = d.DeserializeI8     ();  }
inline void Deserialize(Deserializer& d, I16&    out_value)  { out_value = d.DeserializeI16    ();  }
inline void Deserialize(Deserializer& d, I32&    out_value)  { out_value = d.DeserializeI32    ();  }
inline void Deserialize(Deserializer& d, I64&    out_value)  { out_value = d.DeserializeI64    ();  }
inline void Deserialize(Deserializer& d, F32&    out_value)  { out_value = d.DeserializeF32    ();  }
inline void Deserialize(Deserializer& d, F64&    out_value)  { out_value = d.DeserializeF64    ();  }
inline void Deserialize(Deserializer& d, String& out_value)  { out_value = d.DeserializeString ();  }
inline void Deserialize(Deserializer& d, bool&   out_value)  { out_value = d.DeserializeBool   ();  }

template <typename T>
inline void Serialize(Serializer& s, const Vector<T>& vec) {
	s.BeginArray(vec.m_size);
	for (const T& value : vec) {
		Serialize(s, value);
	}
	s.EndArray();
}

template <typename T>
inline void Deserialize(Deserializer& d, Vector<T>& out_vec) {
	U32 size = d.BeginArray();
	out_vec.resize(size);
	for (U32 i = 0; i < size; i++) {
		Deserialize(d, out_vec[i]);
	}
	d.EndArray();
}

void Serialize(Serializer& s, const SerializableBytes& bytes);
void Deserialize(Deserializer& d, SerializableBytes& bytes);
