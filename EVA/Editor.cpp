#include <EVA/Editor.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Console.hpp>

static EdOp* root = nullptr;

EdOp* EdCreateOp()
{
	EdOp* op = new EdOp();
	return op;
}

void EdDeselectRecursive(EdOp* op)
{
	for (EdOp* child : op->children) child->selected = false;
	op->selected = false;
}

void EdDeselectAll()
{
	EdDeselectRecursive(root);
}

void EdSelect(EdOp* op, bool additive = false)
{
	if (!additive) EdDeselectAll();
	op->selected = true;
}

void EdDestroyOp(EdOp* op)
{
	if (op->brush) CSGDestroyBrush(op->brush);
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);
	for (EdOp* child : op->children) EdDestroyOp(child);
	delete op;
}

EdOp* EdCreateCube()
{
	EdOp* op = EdCreateOp();
	op->brush = CSGCreateCube({1,1,1});
	op->type = EdOpType_Brush;
	EdSelect(op);
	return {};
}

void EdBuild(EdOp* op)
{
	for (CSGBrush* brush : op->built) CSGDestroyBrush(brush);
}

void EdInitialize()
{
	ConRegisterCommand("ed_cube", [](int argc, ConValue* argv) -> ConValue { EdCreateCube(); return {}; });
	ConRegisterCommand("ed_build", [](int argc, ConValue* argv) -> ConValue { EdBuild(root); return {}; });

	root = EdCreateOp();
	root->type = EdOpType_Stack;
}

void EdTick()
{

}
