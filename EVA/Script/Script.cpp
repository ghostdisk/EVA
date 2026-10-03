#include <EVA/Script/Script.hpp>
#include <EVA/Script/Script_IR.hpp>
#include <EVA/Core/Panic.hpp>
#include <string.h>

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
	type->scope->kind = ScopeKind::ENUM;
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

static Type* InstantiateArray(Context& context, GenericInstance* instance, Typer* typer);

static Generic* DefineGeneric(Context& context, Scope* scope, StringView name, std::initializer_list<GenericParam> params,
	InstantiateFn instantiate)
{
	Generic* generic = context.arena->New<Generic>();
	generic->name = GetAtom(name);
	GenericParam* copy = (GenericParam*)context.arena->Allocate(params.size() * sizeof(GenericParam), alignof(GenericParam));
	uint32 count = 0;
	for (const GenericParam& param : params)
		copy[count++] = param;
	generic->params = Slice<GenericParam>(copy, count);
	generic->instantiate = instantiate;
	Define(context, scope, generic->name, generic);
	return generic;
}

// The scope above every module, naming the built-ins.
static Scope* CreateGlobalScope(Context& context, ContextKind kind)
{
	Scope* scope = context.arena->New<Scope>();
	scope->kind = ScopeKind::GLOBAL;

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

	context.array_generic = DefineGeneric(context, scope, "Array",
		{
			{ .kind = GenericParamKind::TYPE, .what = "array element" },
			{ .kind = GenericParamKind::CONSTANT, .what = "array size", .type = context.uint_type },
		},
		InstantiateArray);

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

// Generics

bool GenericInstanceKey::operator==(const GenericInstanceKey& other) const
{
	const GenericInstance& a = *instance;
	const GenericInstance& b = *other.instance;
	if (a.generic != b.generic || a.args.count != b.args.count)
		return false;
	for (uint32 i = 0; i < a.args.count; ++i)
	{
		const GenericArg& x = a.args[i];
		const GenericArg& y = b.args[i];
		if (x.type != y.type || !x.constant != !y.constant)
			return false;
		if (x.constant && (x.constant->type != y.constant->type || x.constant->bytes.count != y.constant->bytes.count ||
							  memcmp(x.constant->bytes.data, y.constant->bytes.data, x.constant->bytes.count) != 0))
			return false;
	}
	return true;
}

size_t GenericInstanceHash::operator()(const GenericInstanceKey& key) const
{
	// FNV-1a over the generic, and the arguments' types and constants' bytes.
	uint64 hash = 14695981039346656037ull;
	auto mix = [&](const void* data, size_t size) {
		for (size_t i = 0; i < size; ++i)
			hash = (hash ^ ((const uint8*)data)[i]) * 1099511628211ull;
	};
	mix(&key.instance->generic, sizeof(Generic*));
	for (uint32 i = 0; i < key.instance->args.count; ++i)
	{
		const GenericArg& arg = key.instance->args[i];
		mix(&arg.type, sizeof(Type*));
		if (arg.constant)
		{
			mix(&arg.constant->type, sizeof(Type*));
			mix(arg.constant->bytes.data, arg.constant->bytes.count);
		}
	}
	return (size_t)hash;
}

Type* Instantiate(Context& context, Generic* generic, Slice<GenericArg> args, Typer* typer)
{
	assert(args.count == generic->params.count);
	GenericInstance lookup = { .generic = generic, .args = args };
	auto found = context.instances.find({ &lookup });
	if (found != context.instances.end())
		return found->second;

	// The arguments can be the caller's temporaries, so the instance gets its own copies.
	GenericArg* copies = (GenericArg*)context.arena->Allocate(args.count * sizeof(GenericArg), alignof(GenericArg));
	for (uint32 i = 0; i < args.count; ++i)
	{
		copies[i] = args[i];
		if (Constant* constant = args[i].constant)
		{
			Constant* copy = context.arena->New<Constant>();
			copy->type = constant->type;
			uint8* bytes = (uint8*)context.arena->Allocate(constant->bytes.count, 4);
			memcpy(bytes, constant->bytes.data, constant->bytes.count);
			copy->bytes = Slice<uint8>(bytes, constant->bytes.count);
			copies[i].constant = copy;
		}
	}
	GenericInstance* instance = context.arena->New<GenericInstance>();
	instance->generic = generic;
	instance->args = Slice<GenericArg>(copies, args.count);

	Type* type = generic->instantiate(context, instance, typer);
	if (type)
		context.instances[{ instance }] = type;
	return type;
}

static Type* InstantiateArray(Context& context, GenericInstance* instance, Typer* typer)
{
	Type* element = instance->args[0].type;
	uint32 length;
	memcpy(&length, instance->args[1].constant->bytes.data, 4);
	if (element->type_kind == TypeKind::PRIMITIVE && ((PrimitiveType*)element)->primitive_kind == PrimitiveKind::VOID)
	{
		if (typer)
			EmitError(*typer, "can't make an array of void");
		return nullptr;
	}
	if (length < 1)
	{
		if (typer)
			EmitError(*typer, "array size must be at least 1, got %u", length);
		return nullptr;
	}
	uint64 stride = ((uint64)element->size + element->alignment - 1) / element->alignment * element->alignment;
	if (stride * length > UINT32_MAX)
	{
		if (typer)
			EmitError(*typer, "[%u]%s is too large", length, TypeToString(element, typer->arena).CString());
		return nullptr;
	}

	ArrayType* type = context.arena->New<ArrayType>();
	type->element = element;
	type->length = length;
	type->stride = (uint32)stride;
	type->size = (uint32)(stride * length);
	type->alignment = element->alignment;
	type->instance = instance;
	return type;
}

ArrayType* GetArrayType(Context& context, Type* element, uint32 length)
{
	Constant constant;
	constant.type = context.uint_type;
	constant.bytes = Slice<uint8>((uint8*)&length, 4);
	GenericArg args[] = { { .type = element }, { .constant = &constant } };
	Type* type = Instantiate(context, context.array_generic, Slice<GenericArg>(args, 2), nullptr);
	assert(type);
	return (ArrayType*)type;
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

CompileShaderResult CompileShader(const CompileShaderOptions& options)
{
	using GPU::Backend;
	using GPU::CompiledEntryPoint;
	Arena* arena = options.arena;
	ZTStringView source = options.source;
	if (options.backend == Backend::NONE)
	{
		ScriptError* error = arena->New<ScriptError>();
		error->message = InternString(arena, StringView("no backend to compile the shader for"));
		return { .errors = ToSlice(arena, { error }) };
	}

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

	IRModule ir;
	InitIRModule(ir, &context, intermediate_arena);
	GenerateIR(ir, module, &shader_interface);
	ClampIndices(ir);

	// The wrappers, one per entry point, come in the same order as the entry points.
	uint32 count = shader_interface.entry_points.count;
	CompiledEntryPoint* entry_points = (CompiledEntryPoint*)arena->Allocate(count * sizeof(CompiledEntryPoint), alignof(CompiledEntryPoint));
	uint32 index = 0;
	std::vector<ScriptError*> errors; // limits of the target, see EmitSPIRV, EmitHLSL and EmitMSL
	for (IRRef function = ir.first_function; function; function = ir[function].function.info->next)
	{
		EntryPoint* entry_point = ir[function].function.info->entry_point;
		if (!entry_point)
			continue;
		CompiledEntryPoint& compiled = entry_points[index++];
		compiled = { .stage = entry_point->stage, .name = entry_point->function->name };
		if (options.backend == Backend::VULKAN)
		{
			Slice<uint32> words = EmitSPIRV(ir, function, arena, errors);
			compiled.code = Slice<uint8>((uint8*)words.data, words.count * 4);
		}
		else
		{
			ZTStringView text = options.backend == Backend::D3D11 ? EmitHLSL(ir, function, arena, errors)
																  : EmitMSL(ir, function, arena, errors);
			compiled.code = Slice<uint8>(text.data, (uint32)text.length);
		}
	}
	assert(index == count);
	if (!errors.empty())
		return { .errors = ToSlice(arena, errors) };
	return { .entry_points = Slice<CompiledEntryPoint>(entry_points, count) };
}

}
