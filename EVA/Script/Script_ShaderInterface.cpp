#include <EVA/Script/Script.hpp>

namespace EVA::Script
{

// Locations go up to 31: no target has more than 32 vertex attributes or inter-stage values. Fragment outputs are color
// targets, of which D3D has 8. The device's own limits are checked when creating the pipeline.
static const uint32 LOCATION_LIMIT = 32;
static const uint32 COLOR_TARGET_LIMIT = 8;

Intrinsic* GetAttributeIntrinsic(Node* attribute)
{
	Node* callee = attribute->node_type == NodeType::CALL ? FindChild(attribute, Usage::CALLEE) : attribute;
	if (callee->node_type != NodeType::REFERENCE || callee->target->kind != ElementKind::INTRINSIC)
		return nullptr;
	return (Intrinsic*)callee->target;
}

// The number of semantic and location attributes on node, with the first one in io.
static uint32 FindIOAttributes(Node* node, ShaderIO* io)
{
	uint32 count = 0;
	for (Node* attribute = node->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE)
			continue;
		Intrinsic* intrinsic = GetAttributeIntrinsic(attribute);
		if (!intrinsic)
			continue;
		if (intrinsic->intrinsic_kind != IntrinsicKind::SEMANTIC && intrinsic->intrinsic_kind != IntrinsicKind::LOCATION)
			continue;
		if (count++)
			continue;

		// The typer checked the argument: an ENUM_VALUE reference for semantic, a folded uint for location.
		Node* argument = FindChild(attribute, Usage::ARGUMENT);
		if (intrinsic->intrinsic_kind == IntrinsicKind::SEMANTIC)
		{
			io->io_kind = IOKind::SEMANTIC;
			io->semantic = (Semantic)((Node*)argument->target)->enum_value;
		}
		else
		{
			io->io_kind = IOKind::LOCATION;
			io->location = (uint32)ConstantToInteger(argument->constant);
		}
	}
	return count;
}

static uint32 CountEntryAttributes(Node* function, ShaderStage* stage)
{
	uint32 count = 0;
	for (Node* attribute = function->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE)
			continue;
		Intrinsic* intrinsic = GetAttributeIntrinsic(attribute);
		if (!intrinsic || intrinsic->intrinsic_kind != IntrinsicKind::ENTRY)
			continue;
		// An ENUM_VALUE reference, checked by the typer.
		*stage = (ShaderStage)((Node*)FindChild(attribute, Usage::ARGUMENT)->target)->enum_value;
		count++;
	}
	return count;
}

static bool IsVoid(Type* type)
{
	return type->type_kind == TypeKind::PRIMITIVE && ((PrimitiveType*)type)->primitive_kind == PrimitiveKind::VOID;
}

struct SemanticRule
{
	Semantic semantic;
	ShaderStage stage;
	IODirection direction;
	PrimitiveKind component;
	uint32 count;
	const char* type_name;
};

static const SemanticRule SEMANTIC_RULES[] = {
	{ Semantic::VERTEX_INDEX, ShaderStage::VERTEX, IODirection::INPUT, PrimitiveKind::UNSIGNED, 1, "uint" },
	{ Semantic::POSITION, ShaderStage::VERTEX, IODirection::OUTPUT, PrimitiveKind::FLOAT, 4, "float4" },
	{ Semantic::POSITION, ShaderStage::FRAGMENT, IODirection::INPUT, PrimitiveKind::FLOAT, 4, "float4" },
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
	uint32 semantics = 0; // bit per Semantic
};

static const char* GetDirectionName(IODirection direction)
{
	return direction == IODirection::INPUT ? "input" : "output";
}

// e.g. 'color', the return value
static const char* GetDeclarationNameCString(Context& context, Node* declaration)
{
	if (declaration->usage == Usage::RETURN_TYPE)
		return "the return value";
	return aprintf(context.arena, "'%s'", GetAtomNameCString(context, declaration->name)).CString();
}

static bool CheckSemantic(Flattening& flattening, ShaderIO& io)
{
	ShaderInterfaceBuilder& builder = flattening.builder;
	const char* name = SemanticToString(io.semantic).CString();
	for (const SemanticRule& rule : SEMANTIC_RULES)
	{
		if (rule.semantic != io.semantic || rule.stage != flattening.stage || rule.direction != flattening.direction)
			continue;
		PrimitiveType* component = GetComponentType(io.type);
		if (component->primitive_kind != rule.component || GetComponentCount(io.type) != rule.count ||
			(rule.count == 1) != (io.type->type_kind == TypeKind::PRIMITIVE))
		{
			EmitError(builder.context, "'%s' must be %s, got %s", name, rule.type_name, GetTypeNameCString(builder.context, io.type));
			return false;
		}
		return true;
	}
	EmitError(builder.context, "'%s' can't be an %s of a %s shader", name, GetDirectionName(flattening.direction),
		ShaderStageToString(flattening.stage).CString());
	return false;
}

// Adds the leaves of a value of type, declared by declaration, stopping at the first error. Each leaf has a semantic or
// location that isn't used yet, and a struct has at least one field, so the walk ends after a few dozen leaves even for
// structs nested to be exponentially large.
static bool Flatten(Flattening& flattening, Type* type, Node* declaration)
{
	ShaderInterfaceBuilder& builder = flattening.builder;
	CHECK_RECURSION(builder.context);

	ShaderIO io = { .direction = flattening.direction, .type = type, .declaration = declaration };
	uint32 attributes = FindIOAttributes(declaration, &io);
	const char* name = GetDeclarationNameCString(builder.context, declaration);

	if (type->type_kind == TypeKind::STRUCT)
	{
		StructType* struct_type = (StructType*)type;
		if (attributes)
		{
			EmitError(builder.context, "%s is a struct, only its fields can have a semantic or location", name);
			return false;
		}
		if (!struct_type->fields.count)
		{
			EmitError(builder.context, "%s is an empty struct, which can't be an %s", name, GetDirectionName(flattening.direction));
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

	PrimitiveType* component = GetComponentType(type);
	if (!component || !IsNumeric(component))
	{
		EmitError(builder.context, "%s is %s, which can't be an %s", name, GetTypeNameCString(builder.context, type), GetDirectionName(flattening.direction));
		return false;
	}
	if (!attributes)
	{
		EmitError(builder.context, "%s needs a semantic or location", name);
		return false;
	}
	if (attributes > 1)
	{
		EmitError(builder.context, "%s can only have one semantic or location", name);
		return false;
	}

	const char* direction = flattening.direction == IODirection::INPUT ? "inputs" : "outputs";
	if (io.io_kind == IOKind::SEMANTIC)
	{
		if (!CheckSemantic(flattening, io))
			return false;
		uint32 bit = 1u << (uint32)io.semantic;
		if (flattening.semantics & bit)
		{
			EmitError(builder.context, "'%s' is used twice in the %s", SemanticToString(io.semantic).CString(), direction);
			return false;
		}
		flattening.semantics |= bit;
	}
	else
	{
		bool color_target = flattening.stage == ShaderStage::FRAGMENT && flattening.direction == IODirection::OUTPUT;
		uint32 limit = color_target ? COLOR_TARGET_LIMIT : LOCATION_LIMIT;
		if (io.location >= limit)
		{
			EmitError(builder.context, "location %u is out of range, the limit is %u%s", io.location, limit - 1,
				color_target ? " for fragment outputs" : "");
			return false;
		}
		uint32 bit = 1u << io.location;
		if (flattening.locations & bit)
		{
			EmitError(builder.context, "location %u is used twice in the %s", io.location, direction);
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
		if (!IsVoid(return_node->type) || FindIOAttributes(return_node, &unused))
			outputs_built = Flatten(outputs, return_node->type, return_node);
	}
	if (outputs_built && stage == ShaderStage::VERTEX && !(outputs.semantics & (1u << (uint32)Semantic::POSITION)))
	{
		EmitError(builder.context, "vertex shader '%s' has to output @semantic(position)", GetAtomNameCString(builder.context, function->name));
		outputs_built = false;
	}

	*out = { .stage = stage, .function = function, .io = ToSlice(builder.arena, io) };
	return built && outputs_built;
}

// A non-entry point function, whose parameters and return value can't have semantics or locations.
static bool CheckFunction(ShaderInterfaceBuilder& builder, Node* function)
{
	ShaderIO unused;
	for (Node* child = function->child; child; child = child->next)
	{
		if ((child->usage == Usage::PARAMETER || child->usage == Usage::RETURN_TYPE) && FindIOAttributes(child, &unused))
		{
			EmitError(builder.context, "'%s' isn't an entry point, so its parameters and return value can't have a semantic or location",
				GetAtomNameCString(builder.context, function->name));
			return false;
		}
	}
	return true;
}

static bool FindEntryPoints(ShaderInterfaceBuilder& builder, Node* node, bool top_level, std::vector<EntryPoint>& entry_points)
{
	CHECK_RECURSION(builder.context);
	bool found = true;
	if (node->node_type == NodeType::FUNCTION)
	{
		ShaderStage stage = ShaderStage::VERTEX;
		uint32 stages = CountEntryAttributes(node, &stage);
		const char* name = GetAtomNameCString(builder.context, node->name);
		if (stages > 1)
		{
			EmitError(builder.context, "'%s' can only have one 'entry'", name);
			found = false;
		}
		else if (stages && !top_level)
		{
			EmitError(builder.context, "entry point '%s' has to be declared at the top level", name);
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
	size_t errors = builder.context.errors.size();
	// Scripts have globals; shaders only will through bind groups (Docs/Plan/Bindings.md).
	for (Node* declaration = module->child; declaration; declaration = declaration->next)
	{
		if (declaration->node_type == NodeType::VARIABLE)
			EmitError(builder.context, "'%s' is a global, which shaders don't support yet", GetAtomNameCString(builder.context, declaration->name));
	}

	std::vector<EntryPoint> entry_points;
	bool built = FindEntryPoints(builder, module, false, entry_points);
	assert(built || builder.context.errors.size() > errors); // every failure is reported
	out_interface->entry_points = ToSlice(builder.arena, entry_points);
	return builder.context.errors.size() == errors;
}

}
