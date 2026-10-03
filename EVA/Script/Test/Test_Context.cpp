#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>

using namespace EVA;
using namespace EVA::Script;

static Type* FindGlobalType(Context& context, const char* name)
{
	for (Definition* definition = context.global_scope->first; definition; definition = definition->next)
	{
		if (definition->element->kind == ElementKind::TYPE && definition->name == GetAtom(name))
			return (Type*)definition->element;
	}
	return nullptr;
}

TEST(Context, GlobalScopeHoldsTheBuiltInTypes)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SCRIPT);
	REQUIRE(context.global_scope);
	CHECK(context.global_scope->parent == nullptr);

	uint32 types = 0;
	uint32 generics = 0;
	uint32 functions = 0;
	for (Definition* definition = context.global_scope->first; definition; definition = definition->next)
	{
		if (definition->element->kind == ElementKind::GENERIC)
		{
			CHECK_EQ(definition->name, ((Generic*)definition->element)->name);
			generics++;
			continue;
		}
		if (definition->element->kind == ElementKind::INTRINSIC)
		{
			Intrinsic* intrinsic = (Intrinsic*)definition->element;
			CHECK_EQ(definition->name, intrinsic->name);
			CHECK(IsBuiltinFunction(intrinsic->intrinsic_kind)); // scripts have no attributes
			CHECK(intrinsic->argument_scope == nullptr);
			functions++;
			continue;
		}
		CHECK_EQ(definition->element->kind, ElementKind::TYPE);
		CHECK_EQ(definition->name, ((Type*)definition->element)->name); // named by the type's own name
		types++;
	}
	CHECK_EQ(types, 16u); // void, int, uint, float, float2 to float4, float2x2 to float4x4
	CHECK_EQ(generics, 3u);
	CHECK_EQ(functions, 6u); // mul, min, max, dot, length, normalize
	CHECK(context.array_generic);
	CHECK(context.vector_generic);
	CHECK(context.matrix_generic);
}

TEST(Context, GenericInstancesAreUnique)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SCRIPT);
	Type* float_type = FindGlobalType(context, "float");

	ArrayType* a = GetArrayType(context, float_type, 3);
	REQUIRE(a);
	CHECK_EQ(a->type_kind, TypeKind::ARRAY);
	CHECK(a->element == float_type);
	CHECK_EQ(a->length, 3u);
	CHECK(GetArrayType(context, float_type, 3) == a);
	CHECK(GetArrayType(context, float_type, 4) != a);
	CHECK(GetArrayType(context, context.int_type, 3) != a);

	// The instance remembers its generic and arguments, copied into the context.
	REQUIRE(a->instance);
	CHECK(a->instance->generic == context.array_generic);
	REQUIRE(a->instance->args.count == 2);
	CHECK(a->instance->args[0].type == float_type);
	REQUIRE(a->instance->args[1].constant);
	CHECK(a->instance->args[1].constant->type == context.uint_type);
	CHECK_EQ(ConstantToInteger(a->instance->args[1].constant), 3);

	// Arrays of arrays go through the same cache.
	ArrayType* nested = GetArrayType(context, a, 2);
	CHECK(GetArrayType(context, GetArrayType(context, float_type, 3), 2) == nested);
	CHECK_EQ(nested->size, 24u);
}

TEST(Context, RejectedInstancesAreNotCached)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SCRIPT);
	Typer typer = { .context = context, .arena = test.arena };

	uint32 length = 0;
	Constant constant;
	constant.type = context.uint_type;
	constant.bytes = Slice<uint8>((uint8*)&length, 4);
	GenericArg args[] = { { .type = context.float_type }, { .constant = &constant } };
	size_t cached = context.instances.size(); // the named vectors and matrices
	CHECK(!Instantiate(context, context.array_generic, Slice<GenericArg>(args, 2), &typer));
	CHECK(!Instantiate(context, context.array_generic, Slice<GenericArg>(args, 2), &typer));
	REQUIRE(context.errors.size() == 2);
	CHECK(context.errors[0]->message == "array size must be at least 1, got 0");
	CHECK_EQ(context.instances.size(), cached);
}

TEST(Context, ShaderContextAddsTheShaderIntrinsics)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);

	struct Expected
	{
		const char* name;
		IntrinsicKind kind;
	};
	Expected expected[] = {
		{ "semantic", IntrinsicKind::SEMANTIC },
		{ "location", IntrinsicKind::LOCATION },
		{ "entry", IntrinsicKind::ENTRY },
	};
	for (const Expected& e : expected)
	{
		Definition* found = nullptr;
		for (Definition* definition = context.global_scope->first; definition; definition = definition->next)
		{
			if (definition->name == GetAtom(e.name))
				found = definition;
		}
		REQUIRE(found);
		REQUIRE(found->element->kind == ElementKind::INTRINSIC);
		Intrinsic* intrinsic = (Intrinsic*)found->element;
		CHECK_EQ(intrinsic->intrinsic_kind, e.kind);
		CHECK_EQ(intrinsic->name, GetAtom(e.name));
		CHECK_EQ(intrinsic->argument_scope != nullptr, e.kind != IntrinsicKind::LOCATION);
	}

	CHECK(FindGlobalType(context, "float4"));
}

struct ExpectedEnumValue
{
	const char* name;
	int64 value;
};

// The intrinsic's arguments resolve in a scope of its own holding exactly the expected enum values.
static void CheckEnumArgumentScope(EVA::Test::Context& test, Context& context, const char* intrinsic_name, Slice<ExpectedEnumValue> expected)
{
	Intrinsic* intrinsic = nullptr;
	for (Definition* definition = context.global_scope->first; definition; definition = definition->next)
	{
		if (definition->name == GetAtom(intrinsic_name))
			intrinsic = (Intrinsic*)definition->element;
	}
	REQUIRE(intrinsic);
	Scope* scope = intrinsic->argument_scope;
	REQUIRE(scope);
	CHECK(scope->parent == nullptr);

	uint32 count = 0;
	for (Definition* definition = scope->first; definition; definition = definition->next)
		count++;
	CHECK_EQ(count, expected.count);
	for (uint32 i = 0; i < expected.count; ++i)
	{
		const ExpectedEnumValue& e = expected[i];
		Definition* found = nullptr;
		for (Definition* definition = scope->first; definition; definition = definition->next)
		{
			if (definition->name == GetAtom(e.name))
				found = definition;
		}
		REQUIRE(found);
		REQUIRE(found->element->kind == ElementKind::NODE);
		Node* node = (Node*)found->element;
		CHECK_EQ(node->node_type, NodeType::ENUM_VALUE);
		CHECK_EQ(node->name, GetAtom(e.name));
		CHECK_EQ(node->enum_value, e.value);
	}
}

TEST(Context, SemanticArgumentsAreTheSemanticEnumValues)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);
	ExpectedEnumValue expected[] = {
		{ "vertex_index", (int64)Semantic::VERTEX_INDEX },
		{ "position", (int64)Semantic::POSITION },
	};
	CheckEnumArgumentScope(test, context, "semantic", Slice<ExpectedEnumValue>(expected, 2));
}

TEST(Context, EntryArgumentsAreTheShaderStageEnumValues)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);
	ExpectedEnumValue expected[] = {
		{ "vertex", (int64)ShaderStage::VERTEX },
		{ "fragment", (int64)ShaderStage::FRAGMENT },
	};
	CheckEnumArgumentScope(test, context, "entry", Slice<ExpectedEnumValue>(expected, 2));
}

TEST(Context, Primitives)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SCRIPT);

	PrimitiveType* void_type = (PrimitiveType*)FindGlobalType(context, "void");
	REQUIRE(void_type);
	CHECK_EQ(void_type->type_kind, TypeKind::PRIMITIVE);
	CHECK_EQ(void_type->primitive_kind, PrimitiveKind::VOID);
	CHECK_EQ(void_type->size, 0u);

	struct Expected
	{
		const char* name;
		PrimitiveKind primitive_kind;
	};
	Expected expected[] = {
		{ "int", PrimitiveKind::SIGNED },
		{ "uint", PrimitiveKind::UNSIGNED },
		{ "float", PrimitiveKind::FLOAT },
	};
	for (const Expected& e : expected)
	{
		PrimitiveType* type = (PrimitiveType*)FindGlobalType(context, e.name);
		REQUIRE(type);
		CHECK_EQ(type->type_kind, TypeKind::PRIMITIVE);
		CHECK_EQ(type->primitive_kind, e.primitive_kind);
		CHECK_EQ(type->size, 4u);
		CHECK_EQ(type->alignment, 4u);
		CHECK_EQ(type->name, GetAtom(e.name));
	}
}

TEST(Context, FloatVectors)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SCRIPT);
	Type* float_type = FindGlobalType(context, "float");

	const char* names[] = { "float2", "float3", "float4" };
	for (uint32 i = 0; i < 3; ++i)
	{
		VectorType* vector = (VectorType*)FindGlobalType(context, names[i]);
		REQUIRE(vector);
		CHECK_EQ(vector->type_kind, TypeKind::VECTOR);
		CHECK_EQ(vector->name, GetAtom(names[i]));
		CHECK_EQ(vector->count, i + 2);
		CHECK_EQ(vector->size, 4 * (i + 2));
		CHECK_EQ(vector->alignment, 4u);
		CHECK(vector->element == float_type); // types are unique, so this is the same float as the global one
	}
}

TEST(Context, EachContextHasItsOwnTypes)
{
	Context a;
	Context b;
	InitContext(a, test.arena, ContextKind::SCRIPT);
	InitContext(b, test.arena, ContextKind::SCRIPT);
	CHECK(FindGlobalType(a, "float4") != FindGlobalType(b, "float4"));
}
