#include <EVA/Script/Script.hpp>
#include <EVA/Script/Script_IR.hpp>
#include <EVA/Core/Panic.hpp>
#include <stdarg.h>
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
static Type* InstantiateVector(Context& context, GenericInstance* instance, Typer* typer);
static Type* InstantiateMatrix(Context& context, GenericInstance* instance, Typer* typer);

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

	context.array_generic = DefineGeneric(context, scope, "Array",
		{
			{ .kind = GenericParamKind::TYPE, .what = "array element" },
			{ .kind = GenericParamKind::CONSTANT, .what = "array size", .type = context.uint_type },
		},
		InstantiateArray);
	context.vector_generic = DefineGeneric(context, scope, "Vector",
		{
			{ .kind = GenericParamKind::TYPE, .what = "vector element" },
			{ .kind = GenericParamKind::CONSTANT, .what = "vector size", .type = context.uint_type },
		},
		InstantiateVector);
	context.matrix_generic = DefineGeneric(context, scope, "Matrix",
		{
			{ .kind = GenericParamKind::TYPE, .what = "matrix element" },
			{ .kind = GenericParamKind::CONSTANT, .what = "matrix columns", .type = context.uint_type },
			{ .kind = GenericParamKind::CONSTANT, .what = "matrix rows", .type = context.uint_type },
		},
		InstantiateMatrix);

	// Named instances: float2 is Vector(float, 2), float3x4 is Matrix(float, 3, 4).
	for (uint32 count = 2; count <= 4; ++count)
		DefineType(context, scope, GetVectorType(context, context.float_type, count));
	for (uint32 columns = 2; columns <= 4; ++columns)
	{
		for (uint32 rows = 2; rows <= 4; ++rows)
			DefineType(context, scope, GetMatrixType(context, context.float_type, columns, rows));
	}

	DefineIntrinsic(context, scope, "mul", IntrinsicKind::MUL);
	DefineIntrinsic(context, scope, "min", IntrinsicKind::MIN);
	DefineIntrinsic(context, scope, "max", IntrinsicKind::MAX);
	DefineIntrinsic(context, scope, "dot", IntrinsicKind::DOT);
	DefineIntrinsic(context, scope, "length", IntrinsicKind::LENGTH);
	DefineIntrinsic(context, scope, "normalize", IntrinsicKind::NORMALIZE);

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

		DefineIntrinsic(context, scope, "bind_group", IntrinsicKind::BIND_GROUP);
	}
	return scope;
}

void InitContext(Context& context, Arena* arena, ContextKind kind)
{
	context.arena = arena;
	context.error_arena = arena;
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
			EmitError(context, "can't make an array of void");
		return nullptr;
	}
	if (length < 1)
	{
		if (typer)
			EmitError(context, "array size must be at least 1, got %u", length);
		return nullptr;
	}
	uint64 stride = ((uint64)element->size + element->alignment - 1) / element->alignment * element->alignment;
	if (stride * length > UINT32_MAX)
	{
		if (typer)
			EmitError(context, "[%u]%s is too large", length, GetTypeNameCString(context, element));
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

static uint32 GetUintArg(GenericInstance* instance, uint32 index)
{
	uint32 value;
	memcpy(&value, instance->args[index].constant->bytes.data, 4);
	return value;
}

static Type* InstantiateVector(Context& context, GenericInstance* instance, Typer* typer)
{
	Type* element = instance->args[0].type;
	uint32 count = GetUintArg(instance, 1);
	PrimitiveKind kind = element->type_kind == TypeKind::PRIMITIVE ? ((PrimitiveType*)element)->primitive_kind : PrimitiveKind::VOID;
	if (kind == PrimitiveKind::VOID) // not a primitive, or void
	{
		if (typer)
			EmitError(context, "can't make a vector of %s", GetTypeNameCString(context, element));
		return nullptr;
	}
	if (count < 1 || count > 4)
	{
		if (typer)
			EmitError(context, "vector size must be 1 to 4, got %u", count);
		return nullptr;
	}
	if (count == 1)
		return element; // SPIR-V and MSL have no 1-component vectors

	VectorType* type = context.arena->New<VectorType>();
	type->name = GetAtom(aprintf(context.arena, "%s%u", GetAtomString(element->name, context.arena).CString(), count));
	type->element = (PrimitiveType*)element;
	type->count = count;
	type->size = element->size * count;
	type->alignment = element->alignment;
	type->instance = instance;
	return type;
}

static Type* InstantiateMatrix(Context& context, GenericInstance* instance, Typer* typer)
{
	Type* element = instance->args[0].type;
	uint32 columns = GetUintArg(instance, 1);
	uint32 rows = GetUintArg(instance, 2);
	// SPIR-V's matrix columns are float vectors, and MSL only has float and half matrices.
	if (element != context.float_type)
	{
		if (typer)
			EmitError(context, "can't make a matrix of %s, only of float", GetTypeNameCString(context, element));
		return nullptr;
	}
	// Neither SPIR-V nor MSL has matrices with a single column or row.
	if (columns < 2 || columns > 4 || rows < 2 || rows > 4)
	{
		if (typer)
			EmitError(context, "matrix columns and rows must be 2 to 4, got %u and %u", columns, rows);
		return nullptr;
	}

	// Column-major like the IR: columns vectors of rows floats.
	MatrixType* type = context.arena->New<MatrixType>();
	type->name = GetAtom(aprintf(context.arena, "%s%ux%u", GetAtomString(element->name, context.arena).CString(), columns, rows));
	type->element = (PrimitiveType*)element;
	type->columns = columns;
	type->rows = rows;
	type->size = element->size * rows * columns;
	type->alignment = element->alignment;
	type->instance = instance;
	return type;
}

// An instance the internal callers know is valid.
static Type* InstantiateValid(Context& context, Generic* generic, std::initializer_list<GenericArg> args)
{
	Type* type = Instantiate(context, generic, Slice<GenericArg>((GenericArg*)args.begin(), (uint32)args.size()), nullptr);
	assert(type);
	return type;
}

// A uint constant for a generic argument, in the caller's storage.
static Constant* GetUintConstant(Context& context, Constant* storage, uint32* value)
{
	storage->type = context.uint_type;
	storage->bytes = Slice<uint8>((uint8*)value, 4);
	return storage;
}

ArrayType* GetArrayType(Context& context, Type* element, uint32 length)
{
	Constant constant;
	return (ArrayType*)InstantiateValid(context, context.array_generic,
		{ { .type = element }, { .constant = GetUintConstant(context, &constant, &length) } });
}

VectorType* GetVectorType(Context& context, PrimitiveType* element, uint32 count)
{
	assert(count >= 2 && count <= 4);
	Constant constant;
	return (VectorType*)InstantiateValid(context, context.vector_generic,
		{ { .type = element }, { .constant = GetUintConstant(context, &constant, &count) } });
}

MatrixType* GetMatrixType(Context& context, PrimitiveType* element, uint32 columns, uint32 rows)
{
	Constant columns_constant;
	Constant rows_constant;
	return (MatrixType*)InstantiateValid(context, context.matrix_generic,
		{ { .type = element }, { .constant = GetUintConstant(context, &columns_constant, &columns) },
			{ .constant = GetUintConstant(context, &rows_constant, &rows) } });
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

ScriptError* EmitError(Context& context, const char* format, ...)
{
	ScriptError* error = context.error_arena->New<ScriptError>();
	va_list args;
	va_start(args, format);
	error->message = avprintf(context.error_arena, format, args);
	va_end(args);
	context.errors.push_back(error);
	return error;
}

const char* GetAtomNameCString(Context& context, Atom atom)
{
	return GetAtomString(atom, context.arena).CString();
}

const char* GetTypeNameCString(Context& context, Type* type)
{
	return TypeToString(type, context.arena).CString();
}

PrimitiveType* GetComponentType(Type* type)
{
	if (type->type_kind == TypeKind::PRIMITIVE)
		return (PrimitiveType*)type;
	if (type->type_kind == TypeKind::VECTOR)
		return ((VectorType*)type)->element;
	return nullptr;
}

uint32 GetComponentCount(Type* type)
{
	return type->type_kind == TypeKind::VECTOR ? ((VectorType*)type)->count : 1;
}

PrimitiveKind GetScalarKind(Type* type)
{
	if (type->type_kind == TypeKind::MATRIX)
		return ((MatrixType*)type)->element->primitive_kind;
	return GetComponentType(type)->primitive_kind;
}

const char* GetScalarName(PrimitiveKind kind)
{
	switch (kind)
	{
	case PrimitiveKind::VOID: return "void";
	case PrimitiveKind::BOOL: return "bool";
	case PrimitiveKind::SIGNED: return "int";
	case PrimitiveKind::UNSIGNED: return "uint";
	case PrimitiveKind::FLOAT: return "float";
	}
	return "?";
}

bool IsNumeric(PrimitiveType* type)
{
	return type->primitive_kind == PrimitiveKind::SIGNED || type->primitive_kind == PrimitiveKind::UNSIGNED ||
		   type->primitive_kind == PrimitiveKind::FLOAT;
}

bool IsInteger(PrimitiveType* type)
{
	return type->primitive_kind == PrimitiveKind::SIGNED || type->primitive_kind == PrimitiveKind::UNSIGNED;
}

bool IsFloatVector(Type* type)
{
	return type && type->type_kind == TypeKind::VECTOR && ((VectorType*)type)->element->primitive_kind == PrimitiveKind::FLOAT;
}

uint32 ReadComponent(Slice<uint8> bytes, uint32 index)
{
	assert((uint64)index * 4 + 4 <= bytes.count);
	uint32 bits;
	memcpy(&bits, bytes.data + (size_t)index * 4, 4);
	return bits;
}

void WriteComponent(Slice<uint8> bytes, uint32 index, uint32 bits)
{
	assert((uint64)index * 4 + 4 <= bytes.count);
	memcpy(bytes.data + (size_t)index * 4, &bits, 4);
}

float BitsToFloat(uint32 bits)
{
	float value;
	memcpy(&value, &bits, 4);
	return value;
}

uint32 FloatToBits(float value)
{
	uint32 bits;
	memcpy(&bits, &value, 4);
	return bits;
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
		return { .errors = ToSlice(arena, std::vector<ScriptError*>{ error }) };
	}

	Arena* intermediate_arena = CreateArena();
	DEFER(DestroyArena(intermediate_arena));

	// The shader is only converted, never run, so the context can go with the rest of the intermediate data.
	Context context;
	InitContext(context, intermediate_arena, ContextKind::SHADER);
	context.error_arena = arena;

	Parser parser = {
		.context = context,
		.source = (char*)source.CString(),
		.head = (char*)source.CString(),
		.arena = intermediate_arena,
	};
	Node* module = nullptr;
	if (!Parse(parser, &module))
		return { .errors = ToSlice(arena, context.errors) };

	Resolver resolver = { .context = context, .arena = intermediate_arena };
	if (!Resolve(resolver, module))
		return { .errors = ToSlice(arena, context.errors) };

	Typer typer = { .context = context, .arena = intermediate_arena };
	if (!TypeCheck(typer, module))
		return { .errors = ToSlice(arena, context.errors) };

	ShaderInterfaceBuilder interface_builder = {
		.context = context,
		.arena = intermediate_arena,
		.reflection_arena = arena,
		.backend = options.backend,
	};
	ShaderInterface shader_interface;
	if (!BuildShaderInterface(interface_builder, module, &shader_interface))
		return { .errors = ToSlice(arena, context.errors) };

	IRModule ir;
	InitIRModule(ir, &context, intermediate_arena);
	GenerateIR(ir, module, &shader_interface);
	ClampIndices(ir);

	// The wrappers, one per entry point, come in the same order as the entry points.
	uint32 count = shader_interface.entry_points.count;
	CompiledEntryPoint* entry_points = (CompiledEntryPoint*)arena->Allocate(count * sizeof(CompiledEntryPoint), alignof(CompiledEntryPoint));
	uint32 index = 0;
	for (IRRef function = ir.first_function; function; function = ir[function].function.info->next)
	{
		EntryPoint* entry_point = ir[function].function.info->entry_point;
		if (!entry_point)
			continue;
		CompiledEntryPoint& compiled = entry_points[index++];
		compiled = { .stage = entry_point->stage, .name = entry_point->function->name };
		IRReachable used;
		FindReachable(ir, function, used);
		for (IRRef global : used.globals)
		{
			if (((PointerType*)ir[global].type)->space == AddressSpace::UNIFORM)
				compiled.bind_groups |= 1u << ir[global].global.bind_group->reflection.index;
		}
		for (uint32 group = 0; group < GPU::MAX_BIND_GROUPS; ++group)
			compiled.d3d11_bind_group_registers[group] = entry_point->d3d11_bind_group_registers[group];
		if (options.backend == Backend::VULKAN)
		{
			Slice<uint32> words = EmitSPIRV(ir, function, arena);
			compiled.code = Slice<uint8>((uint8*)words.data, words.count * 4);
		}
		else
		{
			ZTStringView text = options.backend == Backend::D3D11 ? EmitHLSL(ir, function, arena) : EmitMSL(ir, function, arena);
			compiled.code = Slice<uint8>(text.data, (uint32)text.length);
		}
	}
	assert(index == count);
	if (!context.errors.empty()) // limits of the target
		return { .errors = ToSlice(arena, context.errors) };
	GPU::ReflectedBindGroup* bind_groups = (GPU::ReflectedBindGroup*)arena->Allocate(
		shader_interface.bind_groups.count * sizeof(GPU::ReflectedBindGroup), alignof(GPU::ReflectedBindGroup));
	for (uint32 i = 0; i < shader_interface.bind_groups.count; ++i)
		bind_groups[i] = shader_interface.bind_groups[i].reflection;
	return {
		.entry_points = Slice<CompiledEntryPoint>(entry_points, count),
		.bind_groups = Slice<GPU::ReflectedBindGroup>(bind_groups, shader_interface.bind_groups.count),
	};
}

}
