#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script_IR.hpp>
#include <string.h>

using namespace EVA;
using namespace EVA::Script;

static void CheckText(Test::Context& test, const char* file, int line, const char* what, ZTStringView got, StringView expected)
{
	if (got == expected)
		return;
	Test::ReportFailure(test, file, line, "%s\n    got\n%s\n    expected\n%.*s", what, got.CString(), (int)expected.length,
		(const char*)expected.data);
}

// The module validates with exactly this error, "" for none.
#define CHECK_VALID(module, expected) CheckText(test, __FILE__, __LINE__, "validating", ValidateIR(module, test.arena), expected)
#define CHECK_DUMP(module, expected) CheckText(test, __FILE__, __LINE__, "dumping", IRModuleToString(module, test.arena), expected)

static Constant* FloatConstant(Arena* arena, Type* type, std::initializer_list<float> values)
{
	Constant* constant = arena->New<Constant>();
	constant->type = type;
	uint8* bytes = (uint8*)arena->Allocate(type->size, 4);
	memset(bytes, 0, type->size);
	uint32 i = 0;
	for (float value : values)
		memcpy(bytes + (size_t)4 * i++, &value, 4);
	constant->bytes = Slice<uint8>(bytes, type->size);
	return constant;
}

TEST(IR, TriangleShader)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);
	IRModule module;
	InitIRModule(module, &context, test.arena);

	Type* float2 = GetVectorType(context, context.float_type, 2);
	Type* float4 = GetVectorType(context, context.float_type, 4);
	Type* positions_type = GetArrayType(context, float2, 3);
	ShaderIO vertex_index = { .direction = IODirection::INPUT, .semantic = Semantic::VERTEX_INDEX, .type = context.uint_type };
	ShaderIO position = { .direction = IODirection::OUTPUT, .semantic = Semantic::POSITION, .type = float4 };
	ShaderIO color = { .direction = IODirection::OUTPUT, .io_kind = IOKind::LOCATION, .location = 0, .type = float4 };
	EntryPoint vertex = { .stage = ShaderStage::VERTEX };
	EntryPoint fragment = { .stage = ShaderStage::FRAGMENT };

	IRRef positions = AddIRGlobal(module, GetAtom("positions"), AddressSpace::CONSTANT, positions_type,
		FloatConstant(test.arena, positions_type, { 0.0f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f }));
	IRRef in0 = AddIRGlobal(module, GetAtom("VSMain.in0"), AddressSpace::INPUT, context.uint_type, nullptr);
	module[in0].global.io = &vertex_index;
	IRRef vs_out0 = AddIRGlobal(module, GetAtom("VSMain.out0"), AddressSpace::OUTPUT, float4, nullptr);
	module[vs_out0].global.io = &position;
	IRRef ps_out0 = AddIRGlobal(module, GetAtom("PSMain.out0"), AddressSpace::OUTPUT, float4, nullptr);
	module[ps_out0].global.io = &color;

	// function @VSMain(%0: uint): float4
	Type* vs_parameters[] = { context.uint_type };
	IRRef vs = AddIRFunction(module, GetAtom("VSMain"), GetFunctionType(context, float4, vs_parameters), nullptr);
	{
		IRRef vertex_id = GetIRParameter(module, vs, 0);
		IRRef local = AddIRLocal(module, vs, context.uint_type, nullptr);
		IRRef block = AddIRBlock(module, vs);
		AddIRInstruction(module, block, IROp::STORE, nullptr, { local, vertex_id });
		IRRef index = AddIRInstruction(module, block, IROp::LOAD, context.uint_type, { local });
		IRRef element = AddIRInstruction(module, block, IROp::ACCESS, GetPointerType(context, AddressSpace::CONSTANT, float2),
			{ positions, index });
		IRRef xy = AddIRInstruction(module, block, IROp::LOAD, float2, { element });
		IRRef result = AddIRInstruction(module, block, IROp::CONSTRUCT, float4,
			{ xy, GetIRFloat(module, 0.0f), GetIRFloat(module, 1.0f) });
		AddIRInstruction(module, block, IROp::RETURN, nullptr, { result });
	}

	// function @PSMain(): float4
	IRRef ps = AddIRFunction(module, GetAtom("PSMain"), GetFunctionType(context, float4, {}), nullptr);
	{
		IRRef block = AddIRBlock(module, ps);
		IRRef one = GetIRFloat(module, 1.0f);
		IRRef white = AddIRInstruction(module, block, IROp::CONSTRUCT, float4, { one, one, one, one });
		AddIRInstruction(module, block, IROp::RETURN, nullptr, { white });
	}

	// The entry wrappers.
	FunctionType* wrapper_type = GetFunctionType(context, context.void_type, {});
	IRRef vs_entry = AddIRFunction(module, GetAtom("VSMain.entry"), wrapper_type, nullptr);
	module[vs_entry].function.info->entry_point = &vertex;
	{
		IRRef block = AddIRBlock(module, vs_entry);
		IRRef id = AddIRInstruction(module, block, IROp::LOAD, context.uint_type, { in0 });
		IRRef result = AddIRInstruction(module, block, IROp::CALL, float4, { vs, id });
		AddIRInstruction(module, block, IROp::STORE, nullptr, { vs_out0, result });
		AddIRInstruction(module, block, IROp::RETURN, nullptr, {});
	}
	IRRef ps_entry = AddIRFunction(module, GetAtom("PSMain.entry"), wrapper_type, nullptr);
	module[ps_entry].function.info->entry_point = &fragment;
	{
		IRRef block = AddIRBlock(module, ps_entry);
		IRRef result = AddIRInstruction(module, block, IROp::CALL, float4, { ps });
		AddIRInstruction(module, block, IROp::STORE, nullptr, { ps_out0, result });
		AddIRInstruction(module, block, IROp::RETURN, nullptr, {});
	}

	CHECK_VALID(module, "");
	CHECK_DUMP(module,
		"global @positions: *constant [3]float2 = {(0.0, 0.5), (0.5, -0.5), (-0.5, -0.5)}\n"
		"global @VSMain.in0: *input uint semantic(vertex_index)\n"
		"global @VSMain.out0: *output float4 semantic(position)\n"
		"global @PSMain.out0: *output float4 location(0)\n"
		"\n"
		"function @VSMain(%0: uint): float4\n"
		"\tlocal $0: *function uint\n"
		"block0:\n"
		"\tstore $0, %0\n"
		"\t%1: uint = load $0\n"
		"\t%2: *constant float2 = access @positions, %1\n"
		"\t%3: float2 = load %2\n"
		"\t%4: float4 = construct %3, float 0.0, float 1.0\n"
		"\treturn %4\n"
		"\n"
		"function @PSMain(): float4\n"
		"block0:\n"
		"\t%0: float4 = construct float 1.0, float 1.0, float 1.0, float 1.0\n"
		"\treturn %0\n"
		"\n"
		"function @VSMain.entry(): void [entry vertex]\n"
		"block0:\n"
		"\t%0: uint = load @VSMain.in0\n"
		"\t%1: float4 = call @VSMain, %0\n"
		"\tstore @VSMain.out0, %1\n"
		"\treturn\n"
		"\n"
		"function @PSMain.entry(): void [entry fragment]\n"
		"block0:\n"
		"\t%0: float4 = call @PSMain\n"
		"\tstore @PSMain.out0, %0\n"
		"\treturn\n");
}

TEST(IR, Constants)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);
	IRModule module;
	InitIRModule(module, &context, test.arena);

	// Interned by type and bits.
	CHECK_EQ(GetIRUint(module, 2), GetIRUint(module, 2));
	CHECK(GetIRUint(module, 2) != GetIRInt(module, 2));
	CHECK(GetIRFloat(module, 0.0f) != GetIRFloat(module, -0.0f));
	Constant* two = test.arena->New<Constant>();
	two->type = context.uint_type;
	uint32 bits = 2;
	two->bytes = Slice<uint8>((uint8*)&bits, 4);
	CHECK_EQ(GetIRConstant(module, two), GetIRUint(module, 2));
	CHECK_EQ(module[GetIRUint(module, 2)].kind, IRValueKind::CONSTANT);
	CHECK(module[GetIRUint(module, 2)].type == context.uint_type);
}

TEST(IR, InsertAndRemove)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);
	IRModule module;
	InitIRModule(module, &context, test.arena);
	IRRef function = AddIRFunction(module, GetAtom("f"), GetFunctionType(context, context.uint_type, {}), nullptr);
	IRRef block = AddIRBlock(module, function);
	IRRef one = GetIRUint(module, 1);
	IRRef a = AddIRInstruction(module, block, IROp::ADD, context.uint_type, { one, one });
	IRRef ret = AddIRInstruction(module, block, IROp::RETURN, nullptr, { a });
	IRRef b = InsertIRInstruction(module, ret, IROp::MUL, context.uint_type, { a, a });
	IRRef c = InsertIRInstruction(module, a, IROp::SUB, context.uint_type, { one, one });
	CHECK_VALID(module, "");
	CHECK_DUMP(module,
		"function @f(): uint\n"
		"block0:\n"
		"\t%0: uint = sub uint 1, uint 1\n"
		"\t%1: uint = add uint 1, uint 1\n"
		"\t%2: uint = mul %1, %1\n"
		"\treturn %1\n");

	// Freed values are reused.
	IRRef three = GetIRInt(module, 3);
	RemoveIRInstruction(module, c);
	RemoveIRInstruction(module, b);
	CHECK_EQ(module[b].kind, IRValueKind::FREE);
	IRRef d = InsertIRInstruction(module, ret, IROp::NEG, context.int_type, { three });
	CHECK_EQ(d, b);
	CHECK_VALID(module, "");
	CHECK_DUMP(module,
		"function @f(): uint\n"
		"block0:\n"
		"\t%0: uint = add uint 1, uint 1\n"
		"\t%1: int = neg int 3\n"
		"\treturn %0\n");

	// Operands can come from another instruction's, which adding them moves.
	module.operands.shrink_to_fit();
	IRRef e = InsertIRInstruction(module, ret, IROp::ADD, context.uint_type, GetIROperands(module, a));
	CHECK_EQ(GetIROperands(module, e)[0], one);
	CHECK_EQ(GetIROperands(module, e)[1], one);
}

TEST(IR, PoolGrowsInPages)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SHADER);
	IRModule module;
	InitIRModule(module, &context, test.arena);
	IRRef function = AddIRFunction(module, GetAtom("f"), GetFunctionType(context, context.void_type, {}), nullptr);
	IRRef block = AddIRBlock(module, function);
	IRRef local = AddIRLocal(module, function, context.uint_type, nullptr);
	IRValue* first = &module[block];
	for (uint32 i = 0; i < 3 * IR_PAGE_SIZE; ++i)
		AddIRInstruction(module, block, IROp::STORE, nullptr, { local, GetIRUint(module, i % 7) });
	AddIRInstruction(module, block, IROp::RETURN, nullptr, {});
	CHECK_EQ(&module[block], first); // values don't move
	CHECK(module.pages.size() >= 4);
	CHECK_VALID(module, "");
}

// A function @f(%0: uint, %1: float4): float4 with a local $0: float4 and a struct local $1: S, to add instructions to.
struct Fixture
{
	Context context;
	IRModule module;
	Type* float2 = nullptr;
	Type* float4 = nullptr;
	StructType* s = nullptr;
	IRRef function = 0;
	IRRef block = 0;
	IRRef u = 0; // %0: uint
	IRRef v = 0; // %1: float4
	IRRef local = 0;
	IRRef struct_local = 0;
	IRRef table = 0; // @table: *constant [2]float4
	IRRef other = 0; // @g(%0: uint): float4

	void Init(Arena* arena)
	{
		InitContext(context, arena, ContextKind::SHADER);
		InitIRModule(module, &context, arena);
		float2 = GetVectorType(context, context.float_type, 2);
		float4 = GetVectorType(context, context.float_type, 4);

		s = arena->New<StructType>();
		s->name = GetAtom("S");
		StructField* fields = (StructField*)arena->Allocate(2 * sizeof(StructField), alignof(StructField));
		fields[0] = { .name = GetAtom("a"), .type = context.uint_type, .offset = 0 };
		fields[1] = { .name = GetAtom("b"), .type = float4, .offset = 4 };
		s->fields = Slice<StructField>(fields, 2);
		s->size = 20;
		s->alignment = 4;
		s->state = StructState::COMPLETE;

		Type* table_type = GetArrayType(context, float4, 2);
		table = AddIRGlobal(module, GetAtom("table"), AddressSpace::CONSTANT, table_type, FloatConstant(arena, table_type, {}));

		Type* parameters[] = { context.uint_type, float4 };
		function = AddIRFunction(module, GetAtom("f"), GetFunctionType(context, float4, parameters), nullptr);
		u = GetIRParameter(module, function, 0);
		v = GetIRParameter(module, function, 1);
		local = AddIRLocal(module, function, float4, nullptr);
		struct_local = AddIRLocal(module, function, s, nullptr);
		block = AddIRBlock(module, function);

		Type* other_parameters[] = { context.uint_type };
		other = AddIRFunction(module, GetAtom("g"), GetFunctionType(context, float4, other_parameters), nullptr);
		IRRef other_block = AddIRBlock(module, other);
		AddIRInstruction(module, other_block, IROp::RETURN, nullptr, { GetIRConstant(module, FloatConstant(arena, float4, {})) });
	}

	IRRef Add(IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op = 0)
	{
		return AddIRInstruction(module, block, op, type, operands, sub_op);
	}

	void Return() { Add(IROp::RETURN, nullptr, { v }); }
};

TEST(IR, ValidMemory)
{
	Fixture f;
	f.Init(test.arena);
	f.Add(IROp::STORE, nullptr, { f.local, f.v });
	IRRef loaded = f.Add(IROp::LOAD, f.float4, { f.local });
	IRRef field = f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::FUNCTION, f.float4),
		{ f.struct_local, GetIRUint(f.module, 1) });
	f.Add(IROp::STORE, nullptr, { field, loaded });
	IRRef component = f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::FUNCTION, f.context.float_type),
		{ f.struct_local, GetIRUint(f.module, 1), f.u });
	f.Add(IROp::LOAD, f.context.float_type, { component });
	IRRef element = f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::CONSTANT, f.float4), { f.table, f.u });
	f.Add(IROp::COPY, nullptr, { f.local, element });
	f.Return();
	CHECK_VALID(f.module, "");
}

TEST(IR, ValidValues)
{
	Fixture f;
	f.Init(test.arena);
	Type* bool4 = GetVectorType(f.context, f.context.bool_type, 4);
	IRRef x = f.Add(IROp::EXTRACT, f.context.float_type, { f.v, GetIRUint(f.module, 0) });
	IRRef y = f.Add(IROp::EXTRACT_DYNAMIC, f.context.float_type, { f.v, f.u });
	IRRef xy = f.Add(IROp::CONSTRUCT, f.float2, { x, y });
	IRRef zw = f.Add(IROp::SHUFFLE, f.float2, { f.v, f.v, GetIRUint(f.module, 2), GetIRUint(f.module, 7) });
	IRRef sum = f.Add(IROp::ADD, f.float2, { xy, zw });
	IRRef doubled = f.Add(IROp::CONSTRUCT, f.float4, { sum, sum });
	IRRef less = f.Add(IROp::LT, bool4, { doubled, f.v });
	IRRef chosen = f.Add(IROp::SELECT, f.float4, { less, doubled, f.v });
	IRRef small = f.Add(IROp::INTRINSIC, f.float4, { chosen, f.v }, (uint8)IRIntrinsic::MIN);
	IRRef as_int = f.Add(IROp::CONVERT, f.context.int_type, { x });
	IRRef bits = f.Add(IROp::BITCAST, f.context.uint_type, { as_int });
	f.Add(IROp::SHR, f.context.uint_type, { bits, f.u });
	f.Add(IROp::RETURN, nullptr, { small });
	CHECK_VALID(f.module, "");
}

TEST(IR, ValidControlFlow)
{
	// if (u == 0) { local = v } else {} return local, with a value from the header used after the merge.
	Fixture f;
	f.Init(test.arena);
	IRRef then_block = AddIRBlock(f.module, f.function);
	IRRef else_block = AddIRBlock(f.module, f.function);
	IRRef merge = AddIRBlock(f.module, f.function);
	IRRef zero = f.Add(IROp::EQ, f.context.bool_type, { f.u, GetIRUint(f.module, 0) });
	f.Add(IROp::SELECTION_MERGE, nullptr, { merge });
	f.Add(IROp::BRANCH_IF, nullptr, { zero, then_block, else_block });
	AddIRInstruction(f.module, then_block, IROp::STORE, nullptr, { f.local, f.v });
	AddIRInstruction(f.module, then_block, IROp::BRANCH, nullptr, { merge });
	AddIRInstruction(f.module, else_block, IROp::BRANCH, nullptr, { merge });
	IRRef result = AddIRInstruction(f.module, merge, IROp::LOAD, f.float4, { f.local });
	AddIRInstruction(f.module, merge, IROp::SELECT, f.float4, { zero, result, f.v });
	AddIRInstruction(f.module, merge, IROp::RETURN, nullptr, { result });
	CHECK_VALID(f.module, "");
	CHECK_DUMP(f.module,
		"global @table: *constant [2]float4 = {(0.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 0.0)}\n"
		"\n"
		"function @f(%0: uint, %1: float4): float4\n"
		"\tlocal $0: *function float4\n"
		"\tlocal $1: *function S\n"
		"block0:\n"
		"\t%2: bool = eq %0, uint 0\n"
		"\tselection_merge block3\n"
		"\tbranch_if %2, block1, block2\n"
		"block1:\n"
		"\tstore $0, %1\n"
		"\tbranch block3\n"
		"block2:\n"
		"\tbranch block3\n"
		"block3:\n"
		"\t%3: float4 = load $0\n"
		"\t%4: float4 = select %2, %3, %1\n"
		"\treturn %3\n"
		"\n"
		"function @g(%0: uint): float4\n"
		"block0:\n"
		"\treturn float4 (0.0, 0.0, 0.0, 0.0)\n");
}

TEST(IR, StructureErrors)
{
	{
		Fixture f;
		f.Init(test.arena);
		CHECK_VALID(f.module, "@f: block0 doesn't end with a terminator");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Return();
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (return): a terminator before the end of the block");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::BRANCH, nullptr, { f.module[f.other].function.first_block });
		CHECK_VALID(f.module, "@f: block0 branches to something that isn't one of the function's blocks");
	}
	{
		Fixture f;
		f.Init(test.arena);
		IRRef merge = AddIRBlock(f.module, f.function);
		IRRef zero = f.Add(IROp::EQ, f.context.bool_type, { f.u, GetIRUint(f.module, 0) });
		f.Add(IROp::SELECTION_MERGE, nullptr, { merge });
		f.Add(IROp::NOT, f.context.bool_type, { zero });
		f.Add(IROp::BRANCH_IF, nullptr, { zero, merge, merge });
		AddIRInstruction(f.module, merge, IROp::RETURN, nullptr, { f.v });
		CHECK_VALID(f.module, "@f block0, instruction 1 (selection_merge): has to come right before the terminator");
	}
	{
		// A value from one branch used after the merge.
		Fixture f;
		f.Init(test.arena);
		IRRef then_block = AddIRBlock(f.module, f.function);
		IRRef merge = AddIRBlock(f.module, f.function);
		IRRef zero = f.Add(IROp::EQ, f.context.bool_type, { f.u, GetIRUint(f.module, 0) });
		f.Add(IROp::SELECTION_MERGE, nullptr, { merge });
		f.Add(IROp::BRANCH_IF, nullptr, { zero, then_block, merge });
		IRRef loaded = AddIRInstruction(f.module, then_block, IROp::LOAD, f.float4, { f.local });
		AddIRInstruction(f.module, then_block, IROp::BRANCH, nullptr, { merge });
		AddIRInstruction(f.module, merge, IROp::RETURN, nullptr, { loaded });
		CHECK_VALID(f.module, "@f block2, instruction 0 (return): operand 0 is used before it's defined");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::LOOP_MERGE, nullptr, { f.block, f.block });
		f.Add(IROp::BRANCH, nullptr, { f.block });
		CHECK_VALID(f.module, "@f block0, instruction 0 (loop_merge): names block0, before its header");
	}
}

TEST(IR, OperandErrors)
{
	{
		Fixture f;
		f.Init(test.arena);
		IRRef later = f.Add(IROp::ADD, f.context.uint_type, { f.u, f.u });
		InsertIRInstruction(f.module, later, IROp::ADD, f.context.uint_type, { later, f.u });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (add): operand 0 is used before it's defined");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::ADD, f.context.uint_type, { GetIRParameter(f.module, f.other, 0), f.u });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (add): operand 0 is from another function");
	}
	{
		Fixture f;
		f.Init(test.arena);
		IRRef store = f.Add(IROp::STORE, nullptr, { f.local, f.v });
		f.Add(IROp::ADD, f.context.uint_type, { store, f.u });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 1 (add): operand 0 is an instruction without a result");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::ADD, f.context.uint_type, { f.u });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (add): has the wrong number of operands, 1");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::STORE, f.float4, { f.local, f.v });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (store): has a result type");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::OFFSET, GetPointerType(f.context, AddressSpace::FUNCTION, f.float4), { f.local, f.u });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (offset): isn't supported yet");
	}
	{
		Fixture f;
		f.Init(test.arena);
		f.Add(IROp::CALL, f.float4, { f.u });
		f.Return();
		CHECK_VALID(f.module, "@f block0, instruction 0 (call): calls something that isn't a function");
	}
}

TEST(IR, TypeErrors)
{
	struct Case
	{
		const char* expected;
		void (*build)(Fixture& f);
	};
	Case cases[] = {
		{ "@f block0, instruction 0 (load): expected a pointer, got uint",
			[](Fixture& f) { f.Add(IROp::LOAD, f.context.uint_type, { f.u }); } },
		{ "@f block0, instruction 0 (load): the result is float, expected float4",
			[](Fixture& f) { f.Add(IROp::LOAD, f.context.float_type, { f.local }); } },
		{ "@f block0, instruction 0 (load): can't load S whole",
			[](Fixture& f) { f.Add(IROp::LOAD, f.context.uint_type, { f.struct_local }); } },
		{ "@f block0, instruction 1 (store): can't store to constant space",
			[](Fixture& f) {
				IRRef element = f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::CONSTANT, f.float4), { f.table, f.u });
				f.Add(IROp::STORE, nullptr, { element, f.v });
			} },
		{ "@f block0, instruction 0 (store): the value is uint, expected float4",
			[](Fixture& f) { f.Add(IROp::STORE, nullptr, { f.local, f.u }); } },
		{ "@f block0, instruction 0 (access): the field index 2 is out of range, there are 2",
			[](Fixture& f) {
				f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::FUNCTION, f.float4), { f.struct_local, GetIRUint(f.module, 2) });
			} },
		{ "@f block0, instruction 0 (access): the field index has to be a uint constant",
			[](Fixture& f) { f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::FUNCTION, f.float4), { f.struct_local, f.u }); } },
		{ "@f block0, instruction 0 (access): the result is *function float4, expected *function float",
			[](Fixture& f) { f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::FUNCTION, f.float4), { f.local, f.u }); } },
		{ "@f block0, instruction 0 (access): can't access into float",
			[](Fixture& f) {
				f.Add(IROp::ACCESS, GetPointerType(f.context, AddressSpace::FUNCTION, f.context.float_type), { f.local, f.u, f.u });
			} },
		{ "@f block0, instruction 0 (copy): copies *function S to *function float4",
			[](Fixture& f) { f.Add(IROp::COPY, nullptr, { f.local, f.struct_local }); } },
		{ "@f block0, instruction 1 (construct): constructs float4 from 3 components",
			[](Fixture& f) {
				IRRef x = f.Add(IROp::EXTRACT, f.context.float_type, { f.v, GetIRUint(f.module, 0) });
				f.Add(IROp::CONSTRUCT, f.float4, { x, x, x });
			} },
		{ "@f block0, instruction 0 (construct): can't construct float4 from uint",
			[](Fixture& f) { f.Add(IROp::CONSTRUCT, f.float4, { f.u, f.u, f.u, f.u }); } },
		{ "@f block0, instruction 0 (extract): the component 4 is out of range, there are 4",
			[](Fixture& f) { f.Add(IROp::EXTRACT, f.context.float_type, { f.v, GetIRUint(f.module, 4) }); } },
		{ "@f block0, instruction 0 (shuffle): the component 8 is out of range, there are 8",
			[](Fixture& f) { f.Add(IROp::SHUFFLE, f.float2, { f.v, f.v, GetIRUint(f.module, 0), GetIRUint(f.module, 8) }); } },
		{ "@f block0, instruction 0 (add): the right operand is uint, expected float4",
			[](Fixture& f) { f.Add(IROp::ADD, f.float4, { f.v, f.u }); } },
		{ "@f block0, instruction 0 (neg): can't negate uint",
			[](Fixture& f) { f.Add(IROp::NEG, f.context.uint_type, { f.u }); } },
		{ "@f block0, instruction 0 (and): can't and float4",
			[](Fixture& f) { f.Add(IROp::AND, f.float4, { f.v, f.v }); } },
		{ "@f block0, instruction 0 (eq): the result is bool, expected bool4",
			[](Fixture& f) { f.Add(IROp::EQ, f.context.bool_type, { f.v, f.v }); } },
		{ "@f block0, instruction 0 (select): the condition is uint",
			[](Fixture& f) { f.Add(IROp::SELECT, f.float4, { f.u, f.v, f.v }); } },
		{ "@f block0, instruction 0 (convert): can't convert float4 to uint",
			[](Fixture& f) { f.Add(IROp::CONVERT, f.context.uint_type, { f.v }); } },
		{ "@f block0, instruction 0 (bitcast): can't bitcast float4 to uint",
			[](Fixture& f) { f.Add(IROp::BITCAST, f.context.uint_type, { f.v }); } },
		{ "@f block0, instruction 0 (intrinsic): the right operand is uint, expected float4",
			[](Fixture& f) { f.Add(IROp::INTRINSIC, f.float4, { f.v, f.u }, (uint8)IRIntrinsic::MAX); } },
		{ "@f block0, instruction 0 (call): an argument is float4, expected uint",
			[](Fixture& f) { f.Add(IROp::CALL, f.float4, { f.other, f.v }); } },
		{ "@f block0, instruction 0 (call): passes 0 arguments for 1 parameters",
			[](Fixture& f) { f.Add(IROp::CALL, f.float4, { f.other }); } },
		{ "@f block0, instruction 0 (branch_if): the condition is uint, expected bool",
			[](Fixture& f) { f.Add(IROp::BRANCH_IF, nullptr, { f.u, f.block, f.block }); } },
	};
	for (const Case& c : cases)
	{
		Fixture f;
		f.Init(test.arena);
		c.build(f);
		IRRef last = f.module[f.block].block.last;
		if (!last || !(GetIROpInfo(f.module[last].op).flags & IR_OP_TERMINATOR))
			f.Return();
		CHECK_VALID(f.module, c.expected);
	}

	Fixture f;
	f.Init(test.arena);
	f.Add(IROp::RETURN, nullptr, { f.u });
	CHECK_VALID(f.module, "@f block0, instruction 0 (return): the value is uint, expected float4");
}
