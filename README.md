# EVA

# Structure

Code is split in small libraries located under EVA/ dir.

## EVA/Core

Basic utilities used everywhere throughout this codebase.

## EVA/OS

Low level OS abstraction layer.

## EVA/PAL

Platofrm Abstraction Layer - integration with the window system, HID.

## EVA/GPU

Low-level RHI (GPU API abstraction layer). Unified API for draw calls, buffers, textures.

## EVA/HLSL

HLSL compiler targeting the GPU lib.

# Code style

STL is discouraged. Prefer the utilities in EVA/Core. std::vector is an exception for now, until we sort out memory management properly.