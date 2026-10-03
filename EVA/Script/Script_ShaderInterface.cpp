#include <EVA/Script/Script.hpp>
#include <algorithm>
#include <unordered_map>

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

// Bind groups

// D3D11 allows 4096 registers of 16 bytes in a cbuffer. Sizes stop growing past SIZE_CAP while they're computed, so
// nested arrays can't overflow before they're rejected.
static const uint64 MAX_UNIFORM_BYTES = 65536;
static const uint64 SIZE_CAP = 1ull << 40;

static uint64 RoundUp(uint64 value, uint64 alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

// The index in a let's bind_group attribute, or -1 without one. The typer checked the argument.
static int32 GetBindGroupIndex(Node* declaration)
{
	for (Node* attribute = declaration->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE)
			continue;
		Intrinsic* intrinsic = GetAttributeIntrinsic(attribute);
		if (intrinsic && intrinsic->intrinsic_kind == IntrinsicKind::BIND_GROUP)
			return (int32)ConstantToInteger(FindChild(attribute, Usage::ARGUMENT)->constant);
	}
	return -1;
}

static bool IsBindGroup(Element* element)
{
	return element->kind == ElementKind::NODE && ((Node*)element)->node_type == NodeType::VARIABLE &&
		   ((Node*)element)->usage == Usage::DECLARATION && GetBindGroupIndex((Node*)element) >= 0;
}

// D3D11's cbuffer packing, fxc's: scalars and vectors pack into 16-byte registers without straddling one. Matrices,
// arrays and structs start a register, as does each array element, and what follows them can pack into the rest of
// their last register. Matrices are row_major in HLSL, whose rows are the IR's columns, so each column takes a register.
struct Layouter
{
	ShaderInterfaceBuilder& builder;
	std::unordered_map<Type*, uint64> sizes;
	std::unordered_map<Type*, GPU::TypeLayout*> layouts;

	static bool StartsRegister(Type* type)
	{
		return type->type_kind == TypeKind::MATRIX || type->type_kind == TypeKind::ARRAY || type->type_kind == TypeKind::STRUCT;
	}

	static uint64 Place(uint64 offset, Type* type, uint64 size)
	{
		if (StartsRegister(type))
			return RoundUp(offset, 16);
		offset = RoundUp(offset, 4);
		if (size && offset / 16 != (offset + size - 1) / 16)
			return RoundUp(offset, 16);
		return offset;
	}

	uint64 Size(Type* type)
	{
		auto found = sizes.find(type);
		if (found != sizes.end())
			return found->second;
		uint64 size = 0;
		switch (type->type_kind)
		{
		case TypeKind::PRIMITIVE: size = 4; break;
		case TypeKind::VECTOR: size = 4 * (uint64)((VectorType*)type)->count; break;
		case TypeKind::MATRIX:
		{
			MatrixType* matrix = (MatrixType*)type;
			size = 16 * (uint64)(matrix->columns - 1) + 4 * (uint64)matrix->rows;
			break;
		}
		case TypeKind::ARRAY:
		{
			ArrayType* array = (ArrayType*)type;
			uint64 element = Size(array->element);
			uint64 stride = RoundUp(element, 16);
			size = stride && array->length - 1 > SIZE_CAP / stride ? SIZE_CAP : stride * (array->length - 1) + element;
			break;
		}
		case TypeKind::STRUCT:
		{
			StructType* structure = (StructType*)type;
			for (uint32 i = 0; i < structure->fields.count; ++i)
			{
				Type* field = structure->fields[i].type;
				uint64 field_size = Size(field);
				size = std::min(Place(size, field, field_size) + field_size, SIZE_CAP);
			}
			break;
		}
		default: assert(false); break;
		}
		size = std::min(size, SIZE_CAP);
		sizes[type] = size;
		return size;
	}

	static GPU::ScalarKind Scalar(PrimitiveType* type)
	{
		switch (type->primitive_kind)
		{
		case PrimitiveKind::BOOL: return GPU::ScalarKind::BOOL;
		case PrimitiveKind::SIGNED: return GPU::ScalarKind::INT;
		case PrimitiveKind::UNSIGNED: return GPU::ScalarKind::UINT;
		default: return GPU::ScalarKind::FLOAT;
		}
	}

	// Only for types whose size fits a cbuffer.
	GPU::TypeLayout* Layout(Type* type)
	{
		auto found = layouts.find(type);
		if (found != layouts.end())
			return found->second;
		GPU::TypeLayout* layout = builder.reflection_arena->New<GPU::TypeLayout>();
		layout->size.bytes = (uint32)Size(type);
		layout->alignment = StartsRegister(type) ? 16 : 4;
		switch (type->type_kind)
		{
		case TypeKind::PRIMITIVE: layout->scalar = Scalar((PrimitiveType*)type); break;
		case TypeKind::VECTOR:
			layout->kind = GPU::ReflectedTypeKind::VECTOR;
			layout->scalar = Scalar(((VectorType*)type)->element);
			layout->columns = ((VectorType*)type)->count;
			break;
		case TypeKind::MATRIX:
			layout->kind = GPU::ReflectedTypeKind::MATRIX;
			layout->scalar = Scalar(((MatrixType*)type)->element);
			layout->columns = ((MatrixType*)type)->columns;
			layout->rows = ((MatrixType*)type)->rows;
			break;
		case TypeKind::ARRAY:
		{
			ArrayType* array = (ArrayType*)type;
			layout->kind = GPU::ReflectedTypeKind::ARRAY;
			layout->length = array->length;
			layout->element = Layout(array->element);
			layout->stride.bytes = (uint32)RoundUp(layout->element->size.bytes, 16);
			break;
		}
		case TypeKind::STRUCT:
		{
			StructType* structure = (StructType*)type;
			layout->kind = GPU::ReflectedTypeKind::STRUCT;
			layout->name = structure->name;
			GPU::VarLayout* fields = (GPU::VarLayout*)builder.reflection_arena->Allocate(
				structure->fields.count * sizeof(GPU::VarLayout), alignof(GPU::VarLayout));
			uint64 offset = 0;
			for (uint32 i = 0; i < structure->fields.count; ++i)
			{
				StructField& field = structure->fields[i];
				GPU::TypeLayout* field_layout = Layout(field.type);
				offset = Place(offset, field.type, field_layout->size.bytes);
				fields[i] = { .name = field.name, .type = field_layout };
				fields[i].offset.bytes = (uint32)offset;
				offset += field_layout->size.bytes;
			}
			layout->fields = Slice<GPU::VarLayout>(fields, structure->fields.count);
			break;
		}
		default: assert(false); break;
		}
		layouts[type] = layout;
		return layout;
	}
};

static uint64 HashLayout(const GPU::BindGroupLayout& layout)
{
	uint64 hash = 14695981039346656037ull; // FNV-1a
	auto add = [&](uint64 value) {
		for (uint32 i = 0; i < 8; ++i)
		{
			hash ^= (value >> (i * 8)) & 0xFF;
			hash *= 1099511628211ull;
		}
	};
	add(layout.uniform_size);
	for (uint32 i = 0; i < layout.ranges.count; ++i)
	{
		const GPU::BindingRange& range = layout.ranges[i];
		add((uint64)range.kind);
		add(range.count);
		add(range.buffer_size);
		add(range.offset.bytes);
		add(range.offset.binding_ranges);
		add(range.offset.d3d.cbv | (uint64)range.offset.d3d.srv << 32);
		add(range.offset.d3d.uav | (uint64)range.offset.d3d.sampler << 32);
	}
	return hash;
}

static bool BuildBindGroup(ShaderInterfaceBuilder& builder, Layouter& layouter, Node* declaration, int32 index,
	std::vector<ShaderBindGroup>& bind_groups)
{
	Context& context = builder.context;
	const char* name = GetAtomNameCString(context, declaration->name);
	for (ShaderBindGroup& other : bind_groups)
	{
		if (other.reflection.index == (uint32)index)
		{
			EmitError(context, "bind group %d is declared twice, by '%s' and '%s'", index,
				GetAtomNameCString(context, other.declaration->name), name);
			return false;
		}
	}
	if (FindChild(declaration, Usage::VALUE))
	{
		EmitError(context, "bind group '%s' can't have a value", name);
		return false;
	}
	if (declaration->type->type_kind != TypeKind::STRUCT)
	{
		EmitError(context, "bind group '%s' has to be a struct, got %s", name, GetTypeNameCString(context, declaration->type));
		return false;
	}
	uint64 size = layouter.Size(declaration->type);
	if (size > MAX_UNIFORM_BYTES)
	{
		EmitError(context, "bind group '%s' has %llu bytes of constants, D3D11 allows %llu", name, (unsigned long long)size,
			(unsigned long long)MAX_UNIFORM_BYTES);
		return false;
	}

	ShaderBindGroup bind_group = { .declaration = declaration };
	GPU::ReflectedBindGroup& reflection = bind_group.reflection;
	reflection.index = (uint32)index;
	reflection.name = declaration->name;
	reflection.type = layouter.Layout(declaration->type);
	if (size)
	{
		// The implicit uniform buffer comes first.
		GPU::BindingRange* range = builder.reflection_arena->New<GPU::BindingRange>();
		range->kind = GPU::BindingKind::UNIFORM_BUFFER;
		range->buffer_size = (uint32)size;
		reflection.layout.ranges = Slice<GPU::BindingRange>(range, 1);
		reflection.layout.uniform_size = (uint32)size;
		reflection.element_offset.binding_ranges = 1;
		reflection.element_offset.d3d.cbv = 1;
	}
	reflection.layout.hash = HashLayout(reflection.layout);
	bind_groups.push_back(bind_group);
	return true;
}

// A bind group is only read through its fields, it's never a value itself.
static bool CheckBindGroupUse(ShaderInterfaceBuilder& builder, Node* parent, Node* node)
{
	if (node->node_type != NodeType::REFERENCE || !IsBindGroup(node->target))
		return true;
	if (parent->node_type == NodeType::MEMBER && node->usage == Usage::OBJECT)
		return true;
	EmitError(builder.context, "'%s' is a bind group, only its fields can be read", GetAtomNameCString(builder.context, node->name));
	return false;
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
		if (child->usage == Usage::ATTRIBUTE)
			continue;
		found = CheckBindGroupUse(builder, node, child) && found;
		found = FindEntryPoints(builder, child, node->node_type == NodeType::MODULE, entry_points) && found;
	}
	return found;
}

bool BuildShaderInterface(ShaderInterfaceBuilder& builder, Node* module, ShaderInterface* out_interface)
{
	size_t errors = builder.context.errors.size();
	bool built = true;

	// Scripts have globals; shaders only have bind groups.
	Layouter layouter = { .builder = builder };
	std::vector<ShaderBindGroup> bind_groups;
	bool unsupported_reported = false;
	for (Node* declaration = module->child; declaration; declaration = declaration->next)
	{
		if (declaration->node_type != NodeType::VARIABLE)
			continue;
		int32 index = GetBindGroupIndex(declaration);
		if (index >= 0 && builder.backend != GPU::Backend::D3D11)
		{
			if (!unsupported_reported)
			{
				unsupported_reported = true;
				const char* target = builder.backend == GPU::Backend::VULKAN ? "Vulkan" : builder.backend == GPU::Backend::METAL ? "Metal" : "this target";
				EmitError(builder.context, "bind groups aren't supported on %s yet", target);
			}
			built = false;
		}
		else if (index >= 0)
			built = BuildBindGroup(builder, layouter, declaration, index, bind_groups) && built;
		else
		{
			EmitError(builder.context, "'%s' is a global, which shaders don't support yet",
				GetAtomNameCString(builder.context, declaration->name));
			built = false;
		}
	}
	std::sort(bind_groups.begin(), bind_groups.end(),
		[](const ShaderBindGroup& a, const ShaderBindGroup& b) { return a.reflection.index < b.reflection.index; });

	// D3D11 has no register spaces: each group's registers follow the previous groups'.
	GPU::D3DRegisters registers[GPU::MAX_BIND_GROUPS] = {};
	GPU::D3DRegisters next = {};
	for (uint32 group = 0, i = 0; group < GPU::MAX_BIND_GROUPS; ++group)
	{
		registers[group] = next;
		if (i < bind_groups.size() && bind_groups[i].reflection.index == group)
		{
			const GPU::BindGroupLayout& layout = bind_groups[i++].reflection.layout;
			for (uint32 r = 0; r < layout.ranges.count; ++r)
				next.cbv += layout.ranges[r].kind == GPU::BindingKind::UNIFORM_BUFFER ? layout.ranges[r].count : 0;
		}
	}

	std::vector<EntryPoint> entry_points;
	built = FindEntryPoints(builder, module, false, entry_points) && built;
	for (EntryPoint& entry_point : entry_points)
	{
		for (uint32 group = 0; group < GPU::MAX_BIND_GROUPS; ++group)
			entry_point.d3d11_bind_group_registers[group] = registers[group];
	}
	assert(built || builder.context.errors.size() > errors); // every failure is reported
	out_interface->entry_points = ToSlice(builder.arena, entry_points);
	out_interface->bind_groups = ToSlice(builder.arena, bind_groups);
	return builder.context.errors.size() == errors;
}

}
