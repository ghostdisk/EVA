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

	uint32 count = 0;
	for (Definition* definition = context.global_scope->first; definition; definition = definition->next)
	{
		CHECK_EQ(definition->element->kind, ElementKind::TYPE);
		CHECK_EQ(definition->name, ((Type*)definition->element)->name); // named by the type's own name
		count++;
	}
	CHECK_EQ(count, 7u);
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
