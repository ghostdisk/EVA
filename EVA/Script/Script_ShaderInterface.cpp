#include <EVA/Script/Script.hpp>
#include <stdarg.h>

namespace EVA::Script
{

// Locations go up to 31: no target has more than 32 vertex attributes or inter-stage values, or 8 color targets. The
// device's own limits are checked when creating the pipeline.
static const uint32 LOCATION_LIMIT = 32;

ScriptError* EmitError(ShaderInterfaceBuilder& builder, const char* format, ...)
{
	ScriptError* error = builder.error_arena->New<ScriptError>();
	va_list args;
	va_start(args, format);
	error->message = avprintf(builder.error_arena, format, args);
	va_end(args);
	builder.errors.push_back(error);
	return error;
}

static const char* AtomName(ShaderInterfaceBuilder& builder, Atom atom)
{
	return GetAtomString(atom, builder.arena).CString();
}

static const char* TypeName(ShaderInterfaceBuilder& builder, Type* type)
{
	return TypeToString(type, builder.arena).CString();
}

static Intrinsic* AttributeIntrinsic(Node* attribute)
{
	Node* callee = attribute->node_type == NodeType::CALL ? FindChild(attribute, Usage::CALLEE) : attribute;
	if (callee->node_type != NodeType::REFERENCE || callee->target->kind != ElementKind::INTRINSIC)
		return nullptr;
	return (Intrinsic*)callee->target;
}

// The number of builtin and location attributes on node, and the first one's semantic in io.
static uint32 FindSemantics(Node* node, ShaderIO* io)
{
	uint32 count = 0;
	for (Node* attribute = node->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE)
			continue;
		Intrinsic* intrinsic = AttributeIntrinsic(attribute);
		if (!intrinsic)
			continue;
		if (intrinsic->intrinsic_kind != IntrinsicKind::BUILTIN && intrinsic->intrinsic_kind != IntrinsicKind::LOCATION)
			continue;
		if (count++)
			continue;

		// The typer checked the argument: an ENUM_VALUE reference for builtin, a folded uint for location.
		Node* argument = FindChild(attribute, Usage::ARGUMENT);
		if (intrinsic->intrinsic_kind == IntrinsicKind::BUILTIN)
		{
			io->io_kind = IOKind::BUILTIN;
			io->builtin = (Builtin)((Node*)argument->target)->enum_value;
		}
		else
		{
			io->io_kind = IOKind::LOCATION;
			io->location = (uint32)ConstantToInteger(argument->constant);
		}
	}
	return count;
}

static uint32 CountStageAttributes(Node* function, ShaderStage* stage)
{
	uint32 count = 0;
	for (Node* attribute = function->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE)
			continue;
		Intrinsic* intrinsic = AttributeIntrinsic(attribute);
		if (intrinsic && intrinsic->intrinsic_kind == IntrinsicKind::VERTEX)
		{
			*stage = ShaderStage::VERTEX;
			count++;
		}
		else if (intrinsic && intrinsic->intrinsic_kind == IntrinsicKind::FRAGMENT)
		{
			*stage = ShaderStage::FRAGMENT;
			count++;
		}
	}
	return count;
}

static bool IsVoid(Type* type)
{
	return type->type_kind == TypeKind::PRIMITIVE && ((PrimitiveType*)type)->primitive_kind == PrimitiveKind::VOID;
}

struct BuiltinRule
{
	Builtin builtin;
	ShaderStage stage;
	IODirection direction;
	PrimitiveKind component;
	uint32 count;
	const char* type_name;
};

static const BuiltinRule BUILTIN_RULES[] = {
	{ Builtin::VERTEX_INDEX, ShaderStage::VERTEX, IODirection::INPUT, PrimitiveKind::UNSIGNED, 1, "uint" },
	{ Builtin::POSITION, ShaderStage::VERTEX, IODirection::OUTPUT, PrimitiveKind::FLOAT, 4, "float4" },
	{ Builtin::POSITION, ShaderStage::FRAGMENT, IODirection::INPUT, PrimitiveKind::FLOAT, 4, "float4" },
};

// One entry point's inputs or outputs being flattened.
struct Flattening
{
	ShaderInterfaceBuilder& builder;
	ShaderStage stage;
	IODirection direction;
	std::vector<uint32> path;
	std::vector<ShaderIO>& io;
	uint32 locations = 0; // bit per location used in this direction
	uint32 builtins = 0;  // bit per Builtin
};

static const char* DirectionName(IODirection direction)
{
	return direction == IODirection::INPUT ? "input" : "output";
}

static const char* StageName(ShaderStage stage)
{
	return stage == ShaderStage::VERTEX ? "vertex" : "fragment";
}

// e.g. 'color', the return value
static const char* DeclarationName(ShaderInterfaceBuilder& builder, Node* declaration)
{
	if (declaration->usage == Usage::RETURN_TYPE)
		return "the return value";
	return aprintf(builder.arena, "'%s'", AtomName(builder, declaration->name)).CString();
}

static bool CheckBuiltin(Flattening& flattening, ShaderIO& io)
{
	ShaderInterfaceBuilder& builder = flattening.builder;
	const char* name = BuiltinToString(io.builtin).CString();
	for (const BuiltinRule& rule : BUILTIN_RULES)
	{
		if (rule.builtin != io.builtin || rule.stage != flattening.stage || rule.direction != flattening.direction)
			continue;
		PrimitiveType* component = ComponentType(io.type);
		if (component->primitive_kind != rule.component || ComponentCount(io.type) != rule.count ||
			(rule.count == 1) != (io.type->type_kind == TypeKind::PRIMITIVE))
		{
			EmitError(builder, "'%s' must be %s, got %s", name, rule.type_name, TypeName(builder, io.type));
			return false;
		}
		return true;
	}
	EmitError(builder, "'%s' can't be an %s of a %s shader", name, DirectionName(flattening.direction),
		StageName(flattening.stage));
	return false;
}

// Adds the leaves of a value of type, declared by declaration, stopping at the first error. Each leaf has a builtin or
// location that isn't used yet, and a struct has at least one field, so the walk ends after a few dozen leaves even for
// structs nested to be exponentially large.
static bool Flatten(Flattening& flattening, Type* type, Node* declaration)
{
	ShaderInterfaceBuilder& builder = flattening.builder;
	CHECK_RECURSION(builder);

	ShaderIO io = { .direction = flattening.direction, .type = type, .declaration = declaration };
	uint32 semantics = FindSemantics(declaration, &io);
	const char* name = DeclarationName(builder, declaration);

	if (type->type_kind == TypeKind::STRUCT)
	{
		StructType* struct_type = (StructType*)type;
		if (semantics)
		{
			EmitError(builder, "%s is a struct, only its fields can have a builtin or location", name);
			return false;
		}
		if (!struct_type->fields.count)
		{
			EmitError(builder, "%s is an empty struct, which can't be an %s", name, DirectionName(flattening.direction));
			return false;
		}
		for (uint32 i = 0; i < struct_type->fields.count; ++i)
		{
			flattening.path.push_back(i);
			bool flattened = Flatten(flattening, struct_type->fields[i].type, struct_type->fields[i].declaration);
			flattening.path.pop_back();
			if (!flattened)
				return false;
		}
		return true;
	}

	PrimitiveType* component = ComponentType(type);
	if (!component || !IsNumeric(component))
	{
		EmitError(builder, "%s is %s, which can't be an %s", name, TypeName(builder, type), DirectionName(flattening.direction));
		return false;
	}
	if (!semantics)
	{
		EmitError(builder, "%s needs a builtin or location", name);
		return false;
	}
	if (semantics > 1)
	{
		EmitError(builder, "%s can only have one builtin or location", name);
		return false;
	}

	const char* direction = flattening.direction == IODirection::INPUT ? "inputs" : "outputs";
	if (io.io_kind == IOKind::BUILTIN)
	{
		if (!CheckBuiltin(flattening, io))
			return false;
		uint32 bit = 1u << (uint32)io.builtin;
		if (flattening.builtins & bit)
		{
			EmitError(builder, "'%s' is used twice in the %s", BuiltinToString(io.builtin).CString(), direction);
			return false;
		}
		flattening.builtins |= bit;
	}
	else
	{
		if (io.location >= LOCATION_LIMIT)
		{
			EmitError(builder, "location %u is out of range, the limit is %u", io.location, LOCATION_LIMIT - 1);
			return false;
		}
		uint32 bit = 1u << io.location;
		if (flattening.locations & bit)
		{
			EmitError(builder, "location %u is used twice in the %s", io.location, direction);
			return false;
		}
		flattening.locations |= bit;
	}

	uint32* path = (uint32*)builder.arena->Allocate(flattening.path.size() * sizeof(uint32), alignof(uint32));
	for (size_t i = 0; i < flattening.path.size(); ++i)
		path[i] = flattening.path[i];
	io.path = Slice<uint32>(path, (uint32)flattening.path.size());
	flattening.io.push_back(io);
	return true;
}

template<typename T>
static Slice<T> ToSlice(Arena* arena, const std::vector<T>& items)
{
	T* data = (T*)arena->Allocate(items.size() * sizeof(T), alignof(T));
	for (size_t i = 0; i < items.size(); ++i)
		data[i] = items[i];
	return Slice<T>(data, (uint32)items.size());
}

static bool BuildEntryPoint(ShaderInterfaceBuilder& builder, Node* function, ShaderStage stage, EntryPoint* out)
{
	std::vector<ShaderIO> io;
	bool built = true;

	Flattening inputs = { .builder = builder, .stage = stage, .direction = IODirection::INPUT, .io = io };
	uint32 index = 0;
	for (Node* parameter = function->child; parameter; parameter = parameter->next)
	{
		if (parameter->usage != Usage::PARAMETER)
			continue;
		inputs.path.push_back(index++);
		built = Flatten(inputs, parameter->type, parameter) && built;
		inputs.path.pop_back();
	}

	Flattening outputs = { .builder = builder, .stage = stage, .direction = IODirection::OUTPUT, .io = io };
	bool outputs_built = true;
	Node* return_node = FindChild(function, Usage::RETURN_TYPE);
	if (return_node)
	{
		ShaderIO unused;
		if (!IsVoid(return_node->type) || FindSemantics(return_node, &unused))
			outputs_built = Flatten(outputs, return_node->type, return_node);
	}
	if (outputs_built && stage == ShaderStage::VERTEX && !(outputs.builtins & (1u << (uint32)Builtin::POSITION)))
	{
		EmitError(builder, "vertex shader '%s' has to output @builtin(position)", AtomName(builder, function->name));
		outputs_built = false;
	}

	*out = { .stage = stage, .function = function, .io = ToSlice(builder.arena, io) };
	return built && outputs_built;
}

// A non-entry point function, whose parameters and return value can't have builtins or locations.
static bool CheckFunction(ShaderInterfaceBuilder& builder, Node* function)
{
	ShaderIO unused;
	for (Node* child = function->child; child; child = child->next)
	{
		if ((child->usage == Usage::PARAMETER || child->usage == Usage::RETURN_TYPE) && FindSemantics(child, &unused))
		{
			EmitError(builder, "'%s' isn't an entry point, so its parameters and return value can't have a builtin or location",
				AtomName(builder, function->name));
			return false;
		}
	}
	return true;
}

static bool FindEntryPoints(ShaderInterfaceBuilder& builder, Node* node, bool top_level, std::vector<EntryPoint>& entry_points)
{
	CHECK_RECURSION(builder);
	bool found = true;
	if (node->node_type == NodeType::FUNCTION)
	{
		ShaderStage stage = ShaderStage::VERTEX;
		uint32 stages = CountStageAttributes(node, &stage);
		const char* name = AtomName(builder, node->name);
		if (stages > 1)
		{
			EmitError(builder, "'%s' can only have one of 'vertex' and 'fragment'", name);
			found = false;
		}
		else if (stages && !top_level)
		{
			EmitError(builder, "entry point '%s' has to be declared at the top level", name);
			found = false;
		}
		else if (stages)
		{
			EntryPoint entry_point;
			found = BuildEntryPoint(builder, node, stage, &entry_point);
			entry_points.push_back(entry_point);
		}
		else
			found = CheckFunction(builder, node);
	}

	for (Node* child = node->child; child; child = child->next)
	{
		if (child->usage != Usage::ATTRIBUTE)
			found = FindEntryPoints(builder, child, node->node_type == NodeType::MODULE, entry_points) && found;
	}
	return found;
}

bool BuildShaderInterface(ShaderInterfaceBuilder& builder, Node* module, ShaderInterface* out_interface)
{
	std::vector<EntryPoint> entry_points;
	bool built = FindEntryPoints(builder, module, false, entry_points);
	assert(built || !builder.errors.empty()); // every failure is reported
	out_interface->entry_points = ToSlice(builder.arena, entry_points);
	return builder.errors.empty();
}

}
