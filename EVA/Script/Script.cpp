#include <EVA/Script/Script.hpp>
#include <EVA/Core/Panic.hpp>

namespace EVA::Script
{

static PrimitiveType* NewPrimitiveType(Context& context, StringView name, PrimitiveKind primitive_kind, uint32 size)
{
	PrimitiveType* type = context.arena->New<PrimitiveType>();
	type->name = GetAtom(name);
	type->primitive_kind = primitive_kind;
	type->size = size;
	type->alignment = size ? size : 1;
	return type;
}

static VectorType* NewVectorType(Context& context, StringView name, PrimitiveType* element, uint32 count)
{
	VectorType* type = context.arena->New<VectorType>();
	type->name = GetAtom(name);
	type->element = element;
	type->count = count;
	type->size = element->size * count;
	type->alignment = element->alignment;
	return type;
}

static void Define(Context& context, Scope* scope, Atom name, Element* element)
{
	Definition* definition = context.arena->New<Definition>();
	definition->name = name;
	definition->element = element;
	definition->next = scope->first;
	scope->first = definition;
}

// Adds a built-in type to the scope, under its name.
static void DefineType(Context& context, Scope* scope, Type* type)
{
	Define(context, scope, type->name, type);
}

static Intrinsic* DefineIntrinsic(Context& context, Scope* scope, StringView name, IntrinsicKind kind)
{
	Intrinsic* intrinsic = context.arena->New<Intrinsic>();
	intrinsic->intrinsic_kind = kind;
	intrinsic->name = GetAtom(name);
	Define(context, scope, intrinsic->name, intrinsic);
	return intrinsic;
}

static EnumType* NewEnumType(Context& context, StringView name)
{
	EnumType* type = context.arena->New<EnumType>();
	type->name = GetAtom(name);
	type->scope = context.arena->New<Scope>();
	return type;
}

static void DefineEnumValue(Context& context, EnumType* type, StringView name, int64 value)
{
	Node* node = context.arena->New<Node>();
	node->node_type = NodeType::ENUM_VALUE;
	node->name = GetAtom(name);
	node->enum_value = value;
	node->type = type;
	Define(context, type->scope, node->name, node);
}

// The scope above every module, naming the built-ins.
static Scope* CreateGlobalScope(Context& context, ContextKind kind)
{
	Scope* scope = context.arena->New<Scope>();

	context.void_type = NewPrimitiveType(context, "void", PrimitiveKind::VOID, 0);
	context.int_type = NewPrimitiveType(context, "int", PrimitiveKind::SIGNED, 4);
	context.uint_type = NewPrimitiveType(context, "uint", PrimitiveKind::UNSIGNED, 4);
	context.float_type = NewPrimitiveType(context, "float", PrimitiveKind::FLOAT, 4);
	DefineType(context, scope, context.void_type);
	DefineType(context, scope, context.int_type);
	DefineType(context, scope, context.uint_type);
	DefineType(context, scope, context.float_type);
	DefineType(context, scope, NewVectorType(context, "float2", context.float_type, 2));
	DefineType(context, scope, NewVectorType(context, "float3", context.float_type, 3));
	DefineType(context, scope, NewVectorType(context, "float4", context.float_type, 4));

	if (kind == ContextKind::SHADER)
	{
		EnumType* builtin_type = NewEnumType(context, "Builtin");
		context.builtin_type = builtin_type;
		DefineEnumValue(context, builtin_type, "vertex_index", (int64)Builtin::VERTEX_INDEX);
		DefineEnumValue(context, builtin_type, "instance_index", (int64)Builtin::INSTANCE_INDEX);
		DefineEnumValue(context, builtin_type, "position", (int64)Builtin::POSITION);
		DefineEnumValue(context, builtin_type, "front_facing", (int64)Builtin::FRONT_FACING);
		DefineEnumValue(context, builtin_type, "frag_depth", (int64)Builtin::FRAG_DEPTH);
		DefineIntrinsic(context, scope, "builtin", IntrinsicKind::BUILTIN)->argument_scope = builtin_type->scope;

		DefineIntrinsic(context, scope, "location", IntrinsicKind::LOCATION);
		DefineIntrinsic(context, scope, "vertex", IntrinsicKind::VERTEX);
		DefineIntrinsic(context, scope, "fragment", IntrinsicKind::FRAGMENT);
	}
	return scope;
}

void InitContext(Context& context, Arena* arena, ContextKind kind)
{
	context.arena = arena;
	context.global_scope = CreateGlobalScope(context, kind);
}

ArrayType* GetArrayType(Context& context, Type* element, uint32 length)
{
	for (ArrayType* type : context.array_types)
	{
		if (type->element == element && type->length == length)
			return type;
	}

	uint64 stride = ((uint64)element->size + element->alignment - 1) / element->alignment * element->alignment;
	if (stride * length > UINT32_MAX)
		return nullptr;

	ArrayType* type = context.arena->New<ArrayType>();
	type->element = element;
	type->length = length;
	type->stride = (uint32)stride;
	type->size = (uint32)(stride * length);
	type->alignment = element->alignment;
	context.array_types.push_back(type);
	return type;
}

// Copies the list of errors into arena, where the errors themselves already are.
static Slice<ScriptError*> ToSlice(Arena* arena, const std::vector<ScriptError*>& errors)
{
	ScriptError** data = (ScriptError**)arena->Allocate(errors.size() * sizeof(ScriptError*), alignof(ScriptError*));
	for (size_t i = 0; i < errors.size(); ++i)
		data[i] = errors[i];
	return Slice<ScriptError*>(data, (uint32)errors.size());
}

CompileShaderResult CompileShader(Arena* arena, ZTStringView source)
{
	Arena* intermediate_arena = CreateArena(1024 * 1024);
	DEFER(DestroyArena(intermediate_arena));

	// The shader is only converted, never run, so the context can go with the rest of the intermediate data.
	Context context;
	InitContext(context, intermediate_arena, ContextKind::SHADER);

	Parser parser = {
		.source = (char*)source.CString(),
		.head = (char*)source.CString(),
		.arena = intermediate_arena,
		.error_arena = arena,
	};
	Node* module = nullptr;
	if (!Parse(parser, &module))
		return { .errors = ToSlice(arena, parser.errors) };

	Resolver resolver = { .context = &context, .arena = intermediate_arena, .error_arena = arena };
	if (!Resolve(resolver, module))
		return { .errors = ToSlice(arena, resolver.errors) };

	Typer typer = { .context = &context, .arena = intermediate_arena, .error_arena = arena };
	if (!TypeCheck(typer, module))
		return { .errors = ToSlice(arena, typer.errors) };

	DumpNode(module, intermediate_arena);
	printf("\n");
	Panic("CompileShader: code generation is not implemented yet");
}

}
