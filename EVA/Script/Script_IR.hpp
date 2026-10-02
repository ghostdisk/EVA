#pragma once
#include <EVA/Script/Script.hpp>

// The intermediate representation between the front end and the backends. See Docs/IR.md.

namespace EVA::Script
{

// An index into a module's pool of values. 0 is none.
typedef uint32 IRRef;

enum class IRValueKind : uint8
{
	FREE, // on the free list
	INSTRUCTION,
	BLOCK,
	PARAMETER,
	LOCAL,
	GLOBAL,
	CONSTANT,
	FUNCTION,
};

enum class IROp : uint8
{
	NONE,

	// memory
	LOAD,   // pointer
	STORE,  // pointer, value
	ACCESS, // pointer, indices...: a pointer to a field, element, column or component
	COPY,   // destination pointer, source pointer: a whole object

	// pointers, reserved
	OFFSET,
	PTR_TO_INT,
	INT_TO_PTR,

	// vectors and matrices
	CONSTRUCT,       // components...: a vector from scalars and vectors, or a matrix from columns
	EXTRACT,         // vector or matrix, constant index
	EXTRACT_DYNAMIC, // vector, index
	SHUFFLE,         // vector, vector, constant indices...: components picked from both

	// component-wise arithmetic
	ADD,
	SUB,
	MUL,
	DIV,
	REM,
	NEG,

	// component-wise bits, logical on bool
	AND,
	OR,
	XOR,
	NOT,
	SHL,
	SHR,

	// component-wise comparison
	EQ,
	NE,
	LT,
	LE,
	GT,
	GE,

	SELECT, // condition, value if true, value if false

	// linear algebra
	MATMUL,
	SCALE,
	TRANSPOSE,

	// conversion
	CONVERT,
	BITCAST,

	INTRINSIC, // sub_op: IRIntrinsic, operands: its arguments
	CALL,      // function, arguments...

	// structure, before a header block's terminator
	SELECTION_MERGE, // merge block
	LOOP_MERGE,      // merge block, continue block

	// terminators
	BRANCH,    // block
	BRANCH_IF, // condition, block if true, block if false
	RETURN,    // value, unless the function returns void
	DISCARD,
	UNREACHABLE,

	COUNT,
};

enum class IRIntrinsic : uint8
{
	MIN,
	MAX,
};

enum IROpFlags : uint8
{
	IR_OP_RESULT = 1,        // produces a value. CALL does unless it returns void
	IR_OP_TERMINATOR = 2,
	IR_OP_READS_MEMORY = 4,
	IR_OP_WRITES_MEMORY = 8,
	IR_OP_SIDE_EFFECTS = 16, // beyond writing memory: control flow, discarding
	IR_OP_RESERVED = 32,     // not implemented yet
};

static const uint8 IR_VARIADIC = 0xFF;

struct IROpInfo
{
	const char* name;
	uint8 min_operands;
	uint8 max_operands; // IR_VARIADIC for no limit
	uint8 flags;        // IROpFlags
};

const IROpInfo& GetIROpInfo(IROp op);

// Keep in sync with IRIntrinsic (Script_IR.cpp).
ZTStringView IRIntrinsicToString(IRIntrinsic intrinsic);

struct IRInstructionData
{
	uint32 operands; // offset into IRModule::operands
	uint16 operand_count;
	uint16 unused;
	IRRef prev; // in its block
	IRRef next;
};

struct IRBlockData
{
	IRRef first; // instructions, the last one the terminator
	IRRef last;
	IRRef prev; // in its function, in structured order
	IRRef next;
};

struct IRParameterData
{
	uint32 index;
	IRRef next;
	uint64 unused;
};

struct IRLocalData
{
	Constant* initializer; // nullptr: zeroed by the safety pass
	IRRef next;
	uint32 index; // dense within its function
};

struct IRGlobalData // NOLINT(bugprone-tagged-union-member-count): the address space says which, name isn't a tag
{
	union
	{
		Constant* initializer; // CONSTANT, PRIVATE
		ShaderIO* io;          // INPUT, OUTPUT
	};
	Atom name;
	IRRef next;
};

struct IRConstantData
{
	Constant* constant;
	IRRef next; // in its bucket of the module's constants
	uint32 unused;
};

struct IRFunctionInfo;

struct IRFunctionData
{
	IRFunctionInfo* info;
	IRRef first_block; // the entry block
	IRRef last_block;
};

struct IRValue
{
	IRValueKind kind;
	IROp op;      // INSTRUCTION
	uint8 sub_op; // INSTRUCTION: e.g. the IRIntrinsic
	uint8 flags;
	IRRef parent; // INSTRUCTION: its block. BLOCK, PARAMETER, LOCAL: their function. FREE: the next free value
	Type* type;   // pointer types for locals and globals, FunctionType for functions, nullptr for blocks and
	              // instructions without a result
	union
	{
		IRInstructionData instruction;
		IRBlockData block;
		IRParameterData parameter;
		IRLocalData local;
		IRGlobalData global;
		IRConstantData constant;
		IRFunctionData function;
	};
};
static_assert(sizeof(IRValue) == 32);

// What functions need beyond their value.
struct IRFunctionInfo
{
	Atom name;
	Node* declaration = nullptr;       // nullptr for generated functions
	EntryPoint* entry_point = nullptr; // entry wrappers only
	IRRef first_parameter = 0;
	IRRef last_parameter = 0;
	IRRef first_local = 0;
	IRRef last_local = 0;
	uint32 parameter_count = 0;
	uint32 local_count = 0;
	IRRef next = 0; // in the module
};

static const uint32 IR_PAGE_SIZE = 1024;      // values per page of the pool
static const uint32 IR_CONSTANT_BUCKETS = 256;

struct IRModule
{
	Context* context = nullptr;
	Arena* arena = nullptr;       // the pool's pages, function infos, constants made for the IR
	std::vector<IRValue*> pages;  // the pool, in pages so values don't move as it grows
	uint32 count = 0;             // values allocated so far, including the unused 0
	IRRef free_list = 0;
	std::vector<IRRef> operands;  // ranges abandoned by passes aren't reclaimed, the module is short-lived
	IRRef first_function = 0;
	IRRef last_function = 0;
	IRRef first_global = 0;
	IRRef last_global = 0;
	IRRef constant_buckets[IR_CONSTANT_BUCKETS] = {};

	IRValue& operator[](IRRef ref)
	{
		assert(ref && ref < count);
		return pages[ref / IR_PAGE_SIZE][ref % IR_PAGE_SIZE];
	}
};

void InitIRModule(IRModule& module, Context* context, Arena* arena);

// Building. Each appends to the end of its list.

// Adds the function and a parameter for each of its type's parameters.
IRRef AddIRFunction(IRModule& module, Atom name, FunctionType* type, Node* declaration);
IRRef AddIRBlock(IRModule& module, IRRef function);
IRRef AddIRLocal(IRModule& module, IRRef function, Type* type, Constant* initializer);
IRRef AddIRGlobal(IRModule& module, Atom name, AddressSpace space, Type* type, Constant* initializer);

// The parameter at index.
IRRef GetIRParameter(IRModule& module, IRRef function, uint32 index);

// The module's one value for a constant of the same type and bytes.
IRRef GetIRConstant(IRModule& module, Constant* constant);
IRRef GetIRUint(IRModule& module, uint32 value);
IRRef GetIRInt(IRModule& module, int32 value);
IRRef GetIRFloat(IRModule& module, float value);

// Adds an instruction at the end of block. type is the result's, nullptr for none.
IRRef AddIRInstruction(IRModule& module, IRRef block, IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op = 0);

// Adds an instruction before another, in its block.
IRRef InsertIRInstruction(IRModule& module, IRRef before, IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op = 0);

// Unlinks the instruction from its block and frees it. Nothing may use its result anymore.
void RemoveIRInstruction(IRModule& module, IRRef instruction);

// The instruction's operands. Valid until operands are added to the module.
Slice<IRRef> GetIROperands(IRModule& module, IRRef instruction);

// Checks the module's structure and types. Returns an empty string if it's valid, otherwise what's wrong, allocated in
// arena. A problem is a compiler bug, never the user's: user errors all come from the front end. For tests, fuzzing and
// offline use, not called when compiling.
ZTStringView ValidateIR(IRModule& module, Arena* arena);

// The module in the textual form described in Docs/IR.md. Allocated in arena.
ZTStringView IRModuleToString(IRModule& module, Arena* arena);

}
