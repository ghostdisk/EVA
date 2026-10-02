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

VectorType* GetVectorType(Context& context, PrimitiveType* element, uint32 count)
{
	assert(count >= 2 && count <= 4);
	for (VectorType* type : context.vector_types)
	{
		if (type->element == element && type->count == count)
			return type;
	}
	VectorType* type = context.arena->New<VectorType>();
	type->name = GetAtom(aprintf(context.arena, "%s%u", GetAtomString(element->name, context.arena).CString(), count));
	type->element = element;
	type->count = count;
	type->size = element->size * count;
	type->alignment = element->alignment;
	context.vector_types.push_back(type);
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
	context.bool_type = NewPrimitiveType(context, "bool", PrimitiveKind::BOOL, 4);
	context.int_type = NewPrimitiveType(context, "int", PrimitiveKind::SIGNED, 4);
	context.uint_type = NewPrimitiveType(context, "uint", PrimitiveKind::UNSIGNED, 4);
	context.float_type = NewPrimitiveType(context, "float", PrimitiveKind::FLOAT, 4);
	DefineType(context, scope, context.void_type);
	DefineType(context, scope, context.int_type);
	DefineType(context, scope, context.uint_type);
	DefineType(context, scope, context.float_type);
	DefineType(context, scope, GetVectorType(context, context.float_type, 2));
	DefineType(context, scope, GetVectorType(context, context.float_type, 3));
	DefineType(context, scope, GetVectorType(context, context.float_type, 4));

	if (kind == ContextKind::SHADER)
	{
		EnumType* semantic_type = NewEnumType(context, "Semantic");
		context.semantic_type = semantic_type;
		for (Semantic semantic : { Semantic::VERTEX_INDEX, Semantic::POSITION })
			DefineEnumValue(context, semantic_type, SemanticToString(semantic), (int64)semantic);
		DefineIntrinsic(context, scope, "semantic", IntrinsicKind::SEMANTIC)->argument_scope = semantic_type->scope;

		DefineIntrinsic(context, scope, "location", IntrinsicKind::LOCATION);

		EnumType* stage_type = NewEnumType(context, "ShaderStage");
		context.stage_type = stage_type;
		for (ShaderStage stage : { ShaderStage::VERTEX, ShaderStage::FRAGMENT })
			DefineEnumValue(context, stage_type, ShaderStageToString(stage), (int64)stage);
		DefineIntrinsic(context, scope, "entry", IntrinsicKind::ENTRY)->argument_scope = stage_type->scope;
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

PointerType* GetPointerType(Context& context, AddressSpace space, Type* pointee)
{
	for (PointerType* type : context.pointer_types)
	{
		if (type->space == space && type->pointee == pointee)
			return type;
	}
	PointerType* type = context.arena->New<PointerType>();
	type->space = space;
	type->pointee = pointee;
	type->size = 4; // only memory pointers can be stored, as 32-bit offsets
	type->alignment = 4;
	context.pointer_types.push_back(type);
	return type;
}

FunctionType* GetFunctionType(Context& context, Type* return_type, Slice<Type*> parameters)
{
	for (FunctionType* type : context.function_types)
	{
		if (type->return_type != return_type || type->parameters.count != parameters.count)
			continue;
		bool same = true;
		for (uint32 i = 0; i < parameters.count && same; ++i)
			same = type->parameters[i] == parameters[i];
		if (same)
			return type;
	}
	FunctionType* type = context.arena->New<FunctionType>();
	type->return_type = return_type;
	Type** copy = (Type**)context.arena->Allocate(parameters.count * sizeof(Type*), alignof(Type*));
	for (uint32 i = 0; i < parameters.count; ++i)
		copy[i] = parameters[i];
	type->parameters = Slice<Type*>(copy, parameters.count);
	type->size = 0;
	context.function_types.push_back(type);
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
	Arena* intermediate_arena = CreateArena();
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

	ShaderInterfaceBuilder interface_builder = { .arena = intermediate_arena, .error_arena = arena };
	ShaderInterface shader_interface;
	if (!BuildShaderInterface(interface_builder, module, &shader_interface))
		return { .errors = ToSlice(arena, interface_builder.errors) };

	DumpNode(module, intermediate_arena);
	printf("\n%s", ShaderInterfaceToString(shader_interface, intermediate_arena).CString());
	Panic("CompileShader: code generation is not implemented yet");
}

}
