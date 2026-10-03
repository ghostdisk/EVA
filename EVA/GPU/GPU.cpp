#include <EVA/GPU/GPU.hpp>
#ifdef EVA_METAL
#include <EVA/GPU/GPU_Metal.hpp>
#endif
#ifdef EVA_WIN32
#include <EVA/GPU/GPU_D3D11.hpp>
#endif
#ifdef EVA_VULKAN
#include <EVA/GPU/GPU_Vulkan.hpp>
#endif

#include <EVA/Core/Panic.hpp>
#include <string.h>

namespace EVA::GPU
{

Device device;

BackendDesc* backend_descs[] = {
#ifdef EVA_METAL
	&Metal::backend_desc,
#endif
#ifdef EVA_VULKAN
	&Vulkan::backend_desc,
#endif
#ifdef EVA_WIN32
	&D3D11::backend_desc,
#endif
	nullptr
};

void Init(const InitOptions& init_options)
{
	BackendDesc* backend_desc = nullptr;

	for (BackendDesc** candidate = backend_descs; *candidate; ++candidate)
	{
		if (!backend_desc)
			backend_desc = *candidate;
		if ((*candidate)->backend == init_options.preferred_backend)
		{
			backend_desc = *candidate;
			break;
		}
	}

	if (!backend_desc)
		Panic("no GPU backend available");
	if (!backend_desc->Init(device, init_options))
		Panic("failed to initialize the GPU backend");
}

ShaderCursor GetCursor(BindGroup* group)
{
	return group ? ShaderCursor{ .group = group, .type = group->type } : ShaderCursor{};
}

ShaderCursor ShaderCursor::Field(StringView name) const
{
	if (!type || type->kind != ReflectedTypeKind::STRUCT)
		return { .group = group };
	Atom atom = GetAtom(name);
	for (uint32 i = 0; i < type->fields.count; ++i)
	{
		const VarLayout& field = type->fields[i];
		if (field.name == atom)
			return { .group = group, .type = field.type, .bytes = bytes + field.offset.bytes };
	}
	return { .group = group };
}

ShaderCursor ShaderCursor::Element(uint32 index) const
{
	if (!type || type->kind != ReflectedTypeKind::ARRAY || index >= type->length)
		return { .group = group };
	return { .group = group, .type = type->element, .bytes = bytes + index * type->stride.bytes };
}

bool ShaderCursor::Write(const void* data, uint32 size) const
{
	if (!type || size != type->size.bytes || (uint64)bytes + size > group->constants.count)
		return false;
	memcpy(group->constants.data + bytes, data, size);
	group->constants_changed = true;
	return true;
}

}
