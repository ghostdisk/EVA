#include <EVA/Core/FS.hpp>

namespace FS {

String GetExtension(String path) {
	String candidate = {};

	for (int i = path.size - 1; i >= 0; i--) {
		if (path[i] == '/' || path[i] == '\\') break;
		if (path[i] == '.') candidate = path.Skip(i);
	}
	return candidate;
}

String WithoutExtension(String path) {
	String ext = GetExtension(path);
	return path.Take(path.size - ext.size);
}

}
