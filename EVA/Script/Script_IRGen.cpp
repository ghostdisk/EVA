#include <EVA/Script/Script_IR.hpp>
#include <EVA/Core/Panic.hpp>
#include <string.h>
#include <unordered_map>

// Lowers the typed tree to IR. An expression is lowered as one of:
// - a value: a scalar, vector or matrix, for operands, arguments and conditions;
// - a place: a pointer to where it lives, for indexing and member access. foo.bar[i] is a chain of accesses, and only
//   its leaf is loaded when it's used as a value. Something that isn't in memory gets a temporary local;
// - into a destination: arrays and structs are never values, so whoever wants one passes the memory to write it to.
// Expressions are evaluated left to right, each once.
//
// Writing into a destination directly is only safe when nothing being read can alias it: a fresh local, the return
// pointer. Assignment into existing memory has to build its value in a temporary and copy it, see TODO.md.

namespace EVA::Script
{

namespace
{

bool IsAggregate(Type* type)
{
	return type->type_kind == TypeKind::ARRAY || type->type_kind == TypeKind::STRUCT;
}

// The function being generated.
struct FunctionState
{
	IRRef function = 0;
	IRRef block = 0;          // where instructions go, 0 after a terminator until one is needed
	Type* return_type = nullptr; // the source's
	IRRef return_pointer = 0; // when it returns an array or struct
};

struct Generator
{
	IRModule& module;
	Context& context;
	std::unordered_map<Node*, IRRef> places;    // PARAMETER and VARIABLE: their pointer. CONST: its global, once needed
	std::unordered_map<Node*, IRRef> functions; // FUNCTION
	std::unordered_map<Constant*, IRRef> constant_globals;
	uint32 next_constant = 0;
	FunctionState* state = nullptr;

	IRRef Emit(IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op = 0)
	{
		if (!state->block)
			state->block = AddIRBlock(module, state->function); // after a terminator: unreachable, but still code
		return AddIRInstruction(module, state->block, op, type, operands, sub_op);
	}

	void Terminate(IROp op, Slice<IRRef> operands)
	{
		Emit(op, nullptr, operands);
		state->block = 0;
	}

	IRRef Uint(uint32 value) { return GetIRUint(module, value); }

	PointerType* PointerTo(IRRef pointer, Type* pointee)
	{
		return GetPointerType(context, ((PointerType*)module[pointer].type)->space, pointee);
	}

	Constant* ZeroConstant(Type* type)
	{
		Constant* constant = module.arena->New<Constant>();
		constant->type = type;
		uint8* bytes = (uint8*)module.arena->Allocate(type->size, 4);
		memset(bytes, 0, type->size);
		constant->bytes = Slice<uint8>(bytes, type->size);
		return constant;
	}

	IRRef Number(Node* node)
	{
		NumberLiteral* number = node->number;
		PrimitiveType* type = (PrimitiveType*)node->type;
		if (type->primitive_kind == PrimitiveKind::FLOAT)
			return GetIRFloat(module, number->kind == NumberKind::FLOAT ? number->f32 : (float)number->integer);
		if (type->primitive_kind == PrimitiveKind::SIGNED)
			return GetIRInt(module, (int32)number->integer);
		return Uint((uint32)number->integer);
	}

	// A constant array or struct as a global, one per constant.
	IRRef ConstantGlobal(Constant* constant, Atom name)
	{
		auto found = constant_globals.find(constant);
		if (found != constant_globals.end())
			return found->second;
		if (name == Atom::NONE)
			name = GetAtom(aprintf(module.arena, "constant%u", next_constant++));
		IRRef global = AddIRGlobal(module, name, AddressSpace::CONSTANT, constant->type, constant);
		constant_globals[constant] = global;
		return global;
	}

	// Places

	IRRef Temporary(Node* node)
	{
		IRRef local = AddIRLocal(module, state->function, node->type, nullptr);
		if (IsAggregate(node->type))
			Into(node, local);
		else
			Emit(IROp::STORE, nullptr, { local, Value(node) });
		return local;
	}

	IRRef Place(Node* node)
	{
		switch (node->node_type)
		{
		case NodeType::REFERENCE:
		{
			if (node->target->kind == ElementKind::CONSTANT && IsAggregate(node->type))
				return ConstantGlobal((Constant*)node->target, Atom::NONE);
			if (node->target->kind != ElementKind::NODE)
				break;
			Node* target = (Node*)node->target;
			if (target->node_type == NodeType::PARAMETER || target->node_type == NodeType::VARIABLE)
				return places.at(target);
			if (target->node_type == NodeType::CONST && IsAggregate(target->type))
				return ConstantGlobal(FindChild(target, Usage::VALUE)->constant, target->name);
			break;
		}
		case NodeType::CONSTANT:
			if (IsAggregate(node->type))
				return ConstantGlobal(node->constant, Atom::NONE);
			break;
		case NodeType::MEMBER:
		{
			IRRef object = Place(FindChild(node, Usage::OBJECT));
			StructType* structure = (StructType*)FindChild(node, Usage::OBJECT)->type;
			uint32 index = 0;
			while (structure->fields[index].name != node->name)
				index++;
			return Emit(IROp::ACCESS, PointerTo(object, node->type), { object, Uint(index) });
		}
		case NodeType::INDEX:
		{
			IRRef object = Place(FindChild(node, Usage::OBJECT));
			IRRef index = Value(FindChild(node, Usage::INDEX));
			return Emit(IROp::ACCESS, PointerTo(object, node->type), { object, index });
		}
		default: break;
		}
		return Temporary(node);
	}

	// Values

	IRRef Constructor(Node* node)
	{
		VectorType* vector = (VectorType*)node->type;
		std::vector<IRRef> arguments;
		for (Node* argument = node->child; argument; argument = argument->next)
		{
			if (argument->usage == Usage::ARGUMENT)
				arguments.push_back(Value(argument));
		}
		if (arguments.size() == 1 && module[arguments[0]].type == vector)
			return arguments[0];
		if (arguments.size() == 1) // a splat
		{
			IRRef scalar = arguments[0];
			arguments.assign(vector->count, scalar);
		}
		return Emit(IROp::CONSTRUCT, vector, Slice<IRRef>(arguments.data(), (uint32)arguments.size()));
	}

	IRRef Value(Node* node)
	{
		switch (node->node_type)
		{
		case NodeType::NUMBER: return Number(node);
		case NodeType::CONSTANT: return GetIRConstant(module, node->constant);
		case NodeType::REFERENCE:
		{
			if (node->target->kind == ElementKind::CONSTANT)
				return GetIRConstant(module, (Constant*)node->target);
			Node* target = (Node*)node->target;
			if (target->node_type == NodeType::CONST)
				return GetIRConstant(module, FindChild(target, Usage::VALUE)->constant);
			return Emit(IROp::LOAD, node->type, { Place(node) });
		}
		case NodeType::MEMBER:
		case NodeType::INDEX: return Emit(IROp::LOAD, node->type, { Place(node) });
		case NodeType::UNARY:
		{
			IRRef operand = Value(FindChild(node, Usage::OPERAND));
			switch (node->op)
			{
			case TokenType::PLUS: return operand;
			case TokenType::MINUS: return Emit(IROp::NEG, node->type, { operand });
			case TokenType::TILDE: return Emit(IROp::NOT, node->type, { operand });
			default: break;
			}
			break;
		}
		case NodeType::BINARY:
		{
			IRRef left = Value(FindChild(node, Usage::LEFT));
			IRRef right = Value(FindChild(node, Usage::RIGHT));
			IROp op = IROp::NONE;
			switch (node->op)
			{
			case TokenType::PLUS: op = IROp::ADD; break;
			case TokenType::MINUS: op = IROp::SUB; break;
			case TokenType::ASTERISK: op = IROp::MUL; break;
			case TokenType::SLASH: op = IROp::DIV; break;
			case TokenType::PERCENT: op = IROp::REM; break;
			default: break;
			}
			if (op == IROp::NONE)
				break;
			return Emit(op, node->type, { left, right });
		}
		case NodeType::CALL: return Constructor(node); // only vector constructors type so far
		default: break;
		}
		Panic("IR gen: can't lower %s as a value", NodeTypeToString(node->node_type).CString());
	}

	// Into a destination

	void Into(Node* node, IRRef destination)
	{
		if (node->node_type != NodeType::INIT_LIST)
		{
			Emit(IROp::COPY, nullptr, { destination, Place(node) });
			return;
		}
		uint32 index = 0;
		for (Node* element = node->child; element; element = element->next)
		{
			if (element->usage != Usage::ELEMENT)
				continue;
			IRRef pointer = Emit(IROp::ACCESS, PointerTo(destination, element->type), { destination, Uint(index++) });
			if (IsAggregate(element->type))
				Into(element, pointer);
			else
				Emit(IROp::STORE, nullptr, { pointer, Value(element) });
		}
	}

	// Statements

	void Statement(Node* node)
	{
		switch (node->node_type)
		{
		case NodeType::BLOCK:
			for (Node* child = node->child; child; child = child->next)
			{
				if (child->usage != Usage::ATTRIBUTE)
					Statement(child);
			}
			break;
		case NodeType::VARIABLE:
		{
			// The value is evaluated before the name refers to the local, so let x = x; reads an outer x.
			IRRef local = AddIRLocal(module, state->function, node->type, nullptr);
			if (Node* value = FindChild(node, Usage::VALUE))
			{
				if (IsAggregate(node->type))
					Into(value, local); // a fresh local, which nothing in the value can alias
				else
					Emit(IROp::STORE, nullptr, { local, Value(value) });
			}
			places[node] = local;
			break;
		}
		case NodeType::CONST:
		case NodeType::STRUCT:
		case NodeType::TYPE_ALIAS: break;
		case NodeType::FUNCTION: Function(node); break;
		case NodeType::RETURN:
		{
			Node* value = FindChild(node, Usage::VALUE);
			if (value && IsAggregate(value->type))
			{
				Into(value, state->return_pointer);
				Terminate(IROp::RETURN, {});
			}
			else if (value)
				Terminate(IROp::RETURN, { Value(value) });
			else
				Terminate(IROp::RETURN, {});
			break;
		}
		default:
			// An expression, evaluated for nothing yet: there are no side effects to keep.
			if (IsAggregate(node->type))
				Place(node);
			else
				Value(node);
			break;
		}
	}

	FunctionType* Signature(Node* node)
	{
		std::vector<Type*> parameters;
		for (Node* parameter = node->child; parameter; parameter = parameter->next)
		{
			if (parameter->usage != Usage::PARAMETER)
				continue;
			Type* type = parameter->type;
			parameters.push_back(IsAggregate(type) ? GetPointerType(context, AddressSpace::FUNCTION, type) : type);
		}
		Node* return_node = FindChild(node, Usage::RETURN_TYPE);
		Type* return_type = return_node ? return_node->type : context.void_type;
		if (IsAggregate(return_type))
		{
			parameters.push_back(GetPointerType(context, AddressSpace::FUNCTION, return_type));
			return_type = context.void_type;
		}
		return GetFunctionType(context, return_type, Slice<Type*>(parameters.data(), (uint32)parameters.size()));
	}

	void Function(Node* node)
	{
		IRRef function = AddIRFunction(module, node->name, Signature(node), node);
		functions[node] = function;

		Node* return_node = FindChild(node, Usage::RETURN_TYPE);
		FunctionState function_state = { .function = function, .return_type = return_node ? return_node->type : context.void_type };
		FunctionState* outer = state;
		state = &function_state;
		state->block = AddIRBlock(module, function);

		// Parameters of value types go into locals like variables, arrays and structs already are in the caller's.
		uint32 index = 0;
		for (Node* parameter = node->child; parameter; parameter = parameter->next)
		{
			if (parameter->usage != Usage::PARAMETER)
				continue;
			IRRef value = GetIRParameter(module, function, index++);
			if (IsAggregate(parameter->type))
			{
				places[parameter] = value;
				continue;
			}
			IRRef local = AddIRLocal(module, function, parameter->type, nullptr);
			Emit(IROp::STORE, nullptr, { local, value });
			places[parameter] = local;
		}
		if (IsAggregate(state->return_type))
			state->return_pointer = GetIRParameter(module, function, index);

		Statement(FindChild(node, Usage::BODY));

		// Falling off the end returns zero, so it's defined.
		if (state->block)
		{
			if (state->return_type == context.void_type || IsAggregate(state->return_type))
				Terminate(IROp::RETURN, {});
			else
				Terminate(IROp::RETURN, { GetIRConstant(module, ZeroConstant(state->return_type)) });
		}
		state = outer;
	}

	// Entry wrappers

	// A pointer to the leaf at path[first...] under base.
	IRRef AccessPath(IRRef base, Slice<uint32> path, uint32 first)
	{
		if (first >= path.count)
			return base;
		std::vector<IRRef> operands = { base };
		Type* type = ((PointerType*)module[base].type)->pointee;
		for (uint32 i = first; i < path.count; ++i)
		{
			operands.push_back(Uint(path[i]));
			type = ((StructType*)type)->fields[path[i]].type;
		}
		return Emit(IROp::ACCESS, PointerTo(base, type), Slice<IRRef>(operands.data(), (uint32)operands.size()));
	}

	// Loads the inputs, calls the entry point and stores the outputs. Only wrappers touch interface globals.
	void Wrapper(EntryPoint& entry_point)
	{
		Node* node = entry_point.function;
		const char* name = GetAtomString(node->name, module.arena).CString();
		IRRef function = AddIRFunction(module, GetAtom(aprintf(module.arena, "%s.entry", name)),
			GetFunctionType(context, context.void_type, {}), nullptr);
		module[function].function.info->entry_point = &entry_point;
		FunctionState function_state = { .function = function, .return_type = context.void_type };
		state = &function_state;
		state->block = AddIRBlock(module, function);

		std::vector<IRRef> globals;
		uint32 inputs = 0;
		uint32 outputs = 0;
		for (uint32 i = 0; i < entry_point.io.count; ++i)
		{
			ShaderIO& io = entry_point.io[i];
			bool input = io.direction == IODirection::INPUT;
			Atom global_name = GetAtom(aprintf(module.arena, "%s.%s%u", name, input ? "in" : "out", input ? inputs++ : outputs++));
			IRRef global = AddIRGlobal(module, global_name, input ? AddressSpace::INPUT : AddressSpace::OUTPUT, io.type, nullptr);
			module[global].global.io = &io;
			globals.push_back(global);
		}

		std::vector<IRRef> arguments = { functions.at(node) };
		uint32 index = 0;
		for (Node* parameter = node->child; parameter; parameter = parameter->next)
		{
			if (parameter->usage != Usage::PARAMETER)
				continue;
			IRRef argument = IsAggregate(parameter->type) ? AddIRLocal(module, function, parameter->type, nullptr) : 0;
			for (uint32 i = 0; i < entry_point.io.count; ++i)
			{
				ShaderIO& io = entry_point.io[i];
				if (io.direction != IODirection::INPUT || io.path[0] != index)
					continue;
				IRRef value = Emit(IROp::LOAD, io.type, { globals[i] });
				if (argument)
					Emit(IROp::STORE, nullptr, { AccessPath(argument, io.path, 1), value });
				else
					argument = value;
			}
			arguments.push_back(argument);
			index++;
		}

		Node* return_node = FindChild(node, Usage::RETURN_TYPE);
		Type* return_type = return_node ? return_node->type : context.void_type;
		IRRef returned = 0;
		if (IsAggregate(return_type))
		{
			returned = AddIRLocal(module, function, return_type, nullptr);
			arguments.push_back(returned);
		}
		IRRef result = Emit(IROp::CALL, IsAggregate(return_type) || return_type == context.void_type ? nullptr : return_type,
			Slice<IRRef>(arguments.data(), (uint32)arguments.size()));

		for (uint32 i = 0; i < entry_point.io.count; ++i)
		{
			ShaderIO& io = entry_point.io[i];
			if (io.direction != IODirection::OUTPUT)
				continue;
			IRRef value = returned ? Emit(IROp::LOAD, io.type, { AccessPath(returned, io.path, 0) }) : result;
			Emit(IROp::STORE, nullptr, { globals[i], value });
		}
		Terminate(IROp::RETURN, {});
		state = nullptr;
	}
};

}

void GenerateIR(IRModule& module, Node* ast, ShaderInterface* shader_interface)
{
	Generator generator = { .module = module, .context = *module.context };
	// Globals first, since any function can use them. Their values are folded constants.
	for (Node* declaration = ast->child; declaration; declaration = declaration->next)
	{
		if (declaration->node_type != NodeType::VARIABLE)
			continue;
		Node* value = FindChild(declaration, Usage::VALUE);
		Constant* initializer = value ? value->constant : nullptr;
		generator.places[declaration] =
			AddIRGlobal(module, declaration->name, AddressSpace::PRIVATE, declaration->type, initializer);
	}
	for (Node* declaration = ast->child; declaration; declaration = declaration->next)
	{
		if (declaration->node_type == NodeType::FUNCTION)
			generator.Function(declaration);
	}
	if (shader_interface)
	{
		for (uint32 i = 0; i < shader_interface->entry_points.count; ++i)
			generator.Wrapper(shader_interface->entry_points[i]);
	}
}

}
