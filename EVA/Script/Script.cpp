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

static Definition* NewDefinition(Context& context, Scope* scope, DefinitionKind kind, Atom name)
{
	Definition* definition = context.arena->New<Definition>();
	definition->kind = kind;
	definition->name = name;
	definition->next = scope->first;
	scope->first = definition;
	return definition;
}

// Adds a built-in type to the scope, under its name.
static void DefineType(Context& context, Scope* scope, Type* type)
{
	NewDefinition(context, scope, DefinitionKind::TYPE, type->name)->type = type;
}

static void DefineIntrinsic(Context& context, Scope* scope, StringView name, IntrinsicKind kind)
{
	Intrinsic* intrinsic = context.arena->New<Intrinsic>();
	intrinsic->kind = kind;
	intrinsic->name = GetAtom(name);
	NewDefinition(context, scope, DefinitionKind::INTRINSIC, intrinsic->name)->intrinsic = intrinsic;
}

// The scope above every module, naming the built-ins.
static Scope* CreateGlobalScope(Context& context, ContextKind kind)
{
	Scope* scope = context.arena->New<Scope>();

	DefineType(context, scope, NewPrimitiveType(context, "void", PrimitiveKind::VOID, 0));
	DefineType(context, scope, NewPrimitiveType(context, "int", PrimitiveKind::SIGNED, 4));
	DefineType(context, scope, NewPrimitiveType(context, "uint", PrimitiveKind::UNSIGNED, 4));

	PrimitiveType* float_type = NewPrimitiveType(context, "float", PrimitiveKind::FLOAT, 4);
	DefineType(context, scope, float_type);
	DefineType(context, scope, NewVectorType(context, "float2", float_type, 2));
	DefineType(context, scope, NewVectorType(context, "float3", float_type, 3));
	DefineType(context, scope, NewVectorType(context, "float4", float_type, 4));

	if (kind == ContextKind::SHADER)
	{
		DefineIntrinsic(context, scope, "builtin", IntrinsicKind::BUILTIN);
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

	DumpNode(module, intermediate_arena);
	printf("\n");
	Panic("CompileShader: code generation is not implemented yet");
}

}
