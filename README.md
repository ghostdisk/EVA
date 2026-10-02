# EVA

# Structure

Code is split in small libraries located under EVA/ dir.

## EVA/Core

Basic utilities used everywhere throughout this codebase.

## EVA/PAL

Platofrm Abstraction Layer - integration with the window system, HID.

## EVA/GPU

Low-level RHI (GPU API abstraction layer). Unified API for draw calls, buffers, textures.

## EVA/Script

Custom scripting language, also compiled for the GPU lib as the shading language.

# Code style

STL is discouraged.
Prefer the utilities in EVA/Core.
Heavily prefer StringView and ZTStringView over const char* or std::string.
Use Arena-based memory management when appropriate.
std::vector is an exception for now, until we sort out memory management properly.