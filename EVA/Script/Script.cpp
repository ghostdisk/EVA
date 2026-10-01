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

// Adds a built-in type to the scope, under its name.
static void DefineType(Context& context, Scope* scope, Type* type)
{
	Definition* definition = context.arena->New<Definition>();
	definition->name = type->name;
	definition->type = type;
	definition->next = scope->first;
	scope->first = definition;
}

// The scope above every module, naming the built-in types.
static Scope* CreateGlobalScope(Context& context)
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
	return scope;
}

void InitContext(Context& context, Arena* arena)
{
	context.arena = arena;
	context.global_scope = CreateGlobalScope(context);
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
	InitContext(context, intermediate_arena);

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
