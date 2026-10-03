#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script_IR.hpp>
#include <EVA/Script/Test/OutputValidation.hpp>
#include <stdio.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

// Golden tests: each source file under Golden/ (EVA_GOLDEN_DIR) is compiled, and the output of each stage compared with
// a file of expected output next to it, named after it. See Golden/README.md. std::filesystem and std::string since
// this is tooling and there's no file API yet.

using namespace EVA;
using namespace EVA::Script;
using GPU::Backend;
using GPU::CompiledEntryPoint;

namespace
{

enum class Suite
{
	SHADER, // Golden/Shader: compiled as shaders, every stage's output checked
	SCRIPT, // Golden/Script: compiled as scripts, the IR checked
};

struct SuiteInfo
{
	Suite suite;
	const char* directory;
	std::vector<const char*> extensions; // of the expected outputs, every one required
};

const SuiteInfo suites[] = {
	{ Suite::SHADER, "Shader", { ".interface", ".ir", ".d3d11.hlsl", ".msl", ".spvasm" } },
	{ Suite::SCRIPT, "Script", { ".ir" } },
};

struct BackendInfo
{
	Backend backend;
	const char* extension;
	const char* comment; // starts a line comment in the output
};

const BackendInfo backends[] = {
	{ Backend::D3D11, ".d3d11.hlsl", "//" },
	{ Backend::METAL, ".msl", "//" },
	{ Backend::VULKAN, ".spvasm", ";" },
};

struct GoldenCase
{
	const SuiteInfo* suite = nullptr;
	std::string name;   // Suite.file, e.g. Shader.triangle
	std::string source; // path of the .eva file
	std::string stem;   // the path without .eva, which the expected outputs' extensions go after
};

std::filesystem::path GoldenDirectory()
{
	return std::filesystem::path(EVA_GOLDEN_DIR);
}

bool ReadFile(const std::string& path, std::string* out_contents)
{
	FILE* file = fopen(path.c_str(), "rb");
	if (!file)
		return false;
	out_contents->clear();
	char buffer[4096];
	size_t read;
	while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
		out_contents->append(buffer, read);
	fclose(file);
	return true;
}

bool WriteFile(const std::string& path, StringView contents)
{
	FILE* file = fopen(path.c_str(), "wb");
	if (!file)
		return false;
	bool ok = fwrite(contents.data, 1, contents.length, file) == contents.length;
	return fclose(file) == 0 && ok;
}

std::vector<std::string_view> Lines(std::string_view text)
{
	std::vector<std::string_view> lines;
	while (!text.empty())
	{
		size_t end = text.find('\n');
		if (end == std::string_view::npos)
			end = text.size() - 1;
		lines.push_back(text.substr(0, end + 1));
		text.remove_prefix(end + 1);
	}
	return lines;
}

// Reports the first line where got differs from the expected file, with the lines around it from both.
void ReportDifference(Test::Context& test, const std::string& path, std::string_view expected, std::string_view got)
{
	std::vector<std::string_view> expected_lines = Lines(expected);
	std::vector<std::string_view> got_lines = Lines(got);
	size_t first = 0;
	while (first < expected_lines.size() && first < got_lines.size() && expected_lines[first] == got_lines[first])
		first++;
	size_t from = first > 2 ? first - 2 : 0;
	std::string message = "differs from the output, run with --update to accept it";
	for (int side = 0; side < 2; ++side)
	{
		std::vector<std::string_view>& lines = side ? got_lines : expected_lines;
		message += side ? "\n    got:" : "\n    expected:";
		if (first >= lines.size())
			message += first ? " (ends here)" : " (nothing)";
		for (size_t i = from; i < lines.size() && i < first + 4; ++i)
		{
			message += i == first ? "\n  > " : "\n    ";
			std::string_view line = lines[i];
			message.append(line.data(), line.size() - (line.back() == '\n'));
			if (line.back() != '\n')
				message += " (no newline at the end)";
		}
	}
	Test::ReportFailure(test, path.c_str(), (int)first + 1, "%s", message.c_str());
}

// The output has to match the case's file with the extension. With --update, the file is written instead.
void Expect(Test::Context& test, const GoldenCase& golden, const char* extension, StringView got)
{
	std::string path = golden.stem + extension;
	std::string expected;
	bool exists = ReadFile(path, &expected);
	std::string_view got_view((const char*)got.data, got.length);
	if (exists && expected == got_view)
		return;
	if (Test::UpdateExpected())
	{
		if (WriteFile(path, got))
			printf("%s %s\n", exists ? "updated" : "created", path.c_str());
		else
			Test::ReportFailure(test, path.c_str(), 1, "can't write the file");
		return;
	}
	if (!exists)
	{
		Test::ReportFailure(test, golden.source.c_str(), 1, "%s doesn't exist, run with --update to create it",
			std::filesystem::path(path).filename().string().c_str());
		return;
	}
	ReportDifference(test, path, expected, got_view);
}

ZTStringView JoinErrors(Arena* arena, const std::vector<ScriptError*>& errors)
{
	StringBuilder builder(arena);
	for (size_t i = 0; i < errors.size(); ++i)
	{
		builder.Append(i ? "\n    " : "");
		builder.Append(errors[i]->message);
	}
	return builder.ToString();
}

// Runs the front end, and for shaders the interface pass, reporting the errors of the stage that fails.
Node* FrontEnd(Test::Context& test, const GoldenCase& golden, Context& context, const char* source,
	ShaderInterface* out_interface)
{
	const char* file = golden.source.c_str();
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	Node* module = nullptr;
	if (!Parse(parser, &module))
	{
		Test::ReportFailure(test, file, 1, "failed to parse:\n    %s", JoinErrors(test.arena, parser.errors).CString());
		return nullptr;
	}
	Resolver resolver = { .context = &context, .arena = test.arena, .error_arena = test.arena };
	if (!Resolve(resolver, module))
	{
		Test::ReportFailure(test, file, 1, "failed to resolve:\n    %s", JoinErrors(test.arena, resolver.errors).CString());
		return nullptr;
	}
	Typer typer = { .context = &context, .arena = test.arena, .error_arena = test.arena };
	if (!TypeCheck(typer, module))
	{
		Test::ReportFailure(test, file, 1, "failed to type:\n    %s", JoinErrors(test.arena, typer.errors).CString());
		return nullptr;
	}
	if (out_interface)
	{
		ShaderInterfaceBuilder builder = { .arena = test.arena, .error_arena = test.arena };
		if (!BuildShaderInterface(builder, module, out_interface))
		{
			Test::ReportFailure(test, file, 1, "failed in the interface pass:\n    %s",
				JoinErrors(test.arena, builder.errors).CString());
			return nullptr;
		}
	}
	return module;
}

// The generated IR, before indices are clamped, which has to validate.
void CheckIR(Test::Context& test, const GoldenCase& golden, Context& context, Node* module, ShaderInterface* shader_interface)
{
	IRModule ir;
	InitIRModule(ir, &context, test.arena);
	GenerateIR(ir, module, shader_interface);
	ZTStringView dump = IRModuleToString(ir, test.arena);
	ZTStringView error = ValidateIR(ir, test.arena);
	if (error.length)
	{
		Test::ReportFailure(test, golden.source.c_str(), 1, "generated invalid IR: %s\n%s", error.CString(), dump.CString());
		return;
	}
	Expect(test, golden, ".ir", dump);
}

// The entry point's problem according to the target's own tools, empty if there's none or they aren't available.
ZTStringView Validate(Test::Context& test, Backend backend, CompiledEntryPoint& entry_point)
{
	if (backend == Backend::VULKAN)
		return Validation::ValidateSPIRV(Slice<uint32>((uint32*)entry_point.code.data, entry_point.code.count / 4), test.arena);
	StringView text((const char*)entry_point.code.data, entry_point.code.count);
	if (backend == Backend::METAL)
		return Validation::CompileMSL(text, test.arena);
	return Validation::CompileHLSL(text, entry_point.stage, test.arena);
}

// Every entry point's output for the backend, as text, each after a comment naming it. Each has to pass the target's
// tools.
void CheckBackend(Test::Context& test, const GoldenCase& golden, const char* source, const BackendInfo& backend)
{
	const char* file = golden.source.c_str();
	CompileShaderResult result = CompileShader({ .arena = test.arena, .source = source, .backend = backend.backend });
	if (result.errors.count)
	{
		Test::ReportFailure(test, file, 1, "failed to compile for %s: %s", backend.extension, result.errors[0]->message.CString());
		return;
	}
	StringBuilder builder(test.arena);
	for (uint32 i = 0; i < result.entry_points.count; ++i)
	{
		CompiledEntryPoint& entry_point = result.entry_points[i];
		ZTStringView text;
		if (backend.backend == Backend::VULKAN)
		{
			Slice<uint32> words((uint32*)entry_point.code.data, entry_point.code.count / 4);
			text = Validation::DisassembleSPIRV(words, test.arena);
			ZTStringView tools_text = Validation::DisassembleSPIRVWithTools(words, test.arena);
			if (tools_text.length && text != tools_text)
				Test::ReportFailure(test, file, 1, "our SPIR-V disassembly differs from SPIRV-Tools'\n%s\n    SPIRV-Tools:\n%s",
					text.CString(), tools_text.CString());
		}
		else
			text = InternString(test.arena, StringView((const char*)entry_point.code.data, entry_point.code.count));

		ZTStringView problem = Validate(test, backend.backend, entry_point);
		ZTStringView name = GetAtomString(entry_point.name, test.arena);
		if (problem.length)
			Test::ReportFailure(test, file, 1, "%s output for %s is invalid: %s\n%s", backend.extension, name.CString(),
				problem.CString(), text.CString());

		builder.AppendFormat("%s%s %s (%s)\n", i ? "\n" : "", backend.comment, name.CString(),
			entry_point.stage == ShaderStage::VERTEX ? "vertex" : "fragment");
		builder.Append(text);
	}
	Expect(test, golden, backend.extension, builder.ToString());
}

void RunCase(Test::Context& test)
{
	const GoldenCase& golden = *(const GoldenCase*)test.data;
	std::string source;
	if (!ReadFile(golden.source, &source))
	{
		Test::ReportFailure(test, golden.source.c_str(), 1, "can't read the file");
		return;
	}

	Context context;
	if (golden.suite->suite == Suite::SCRIPT)
	{
		InitContext(context, test.arena, ContextKind::SCRIPT);
		Node* module = FrontEnd(test, golden, context, source.c_str(), nullptr);
		if (module)
			CheckIR(test, golden, context, module, nullptr);
		return;
	}

	InitContext(context, test.arena, ContextKind::SHADER);
	ShaderInterface shader_interface;
	Node* module = FrontEnd(test, golden, context, source.c_str(), &shader_interface);
	if (!module)
		return;
	Expect(test, golden, ".interface", ShaderInterfaceToString(shader_interface, test.arena));
	CheckIR(test, golden, context, module, &shader_interface);
	for (const BackendInfo& backend : backends)
		CheckBackend(test, golden, source.c_str(), backend);
}

bool EndsWith(const std::string& string, const char* suffix)
{
	size_t length = strlen(suffix);
	return string.size() >= length && string.compare(string.size() - length, length, suffix) == 0;
}

// Every file in the suites has to be a source or an expected output of one, so a misnamed file isn't silently ignored.
void CheckFiles(Test::Context& test)
{
	std::filesystem::path root = GoldenDirectory();
	std::error_code error;
	if (!std::filesystem::is_directory(root, error))
	{
		Test::ReportFailure(test, root.generic_string().c_str(), 1, "isn't a directory");
		return;
	}
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, error))
	{
		std::string name = entry.path().filename().string();
		bool known = name == "README.md";
		for (const SuiteInfo& suite : suites)
			known = known || name == suite.directory;
		if (!known)
			Test::ReportFailure(test, entry.path().generic_string().c_str(), 1, "isn't a suite of golden tests");
	}
	for (const SuiteInfo& suite : suites)
	{
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root / suite.directory, error))
		{
			std::string path = entry.path().generic_string();
			bool known = EndsWith(path, ".eva");
			for (const char* extension : suite.extensions)
			{
				if (!known && EndsWith(path, extension))
					known = std::filesystem::exists(path.substr(0, path.size() - strlen(extension)) + ".eva");
			}
			if (!known)
				Test::ReportFailure(test, path.c_str(), 1, "isn't a .eva file or an expected output of one in %s",
					suite.directory);
		}
	}
}

}

TEST_GENERATOR(Golden)
{
	std::filesystem::path root = GoldenDirectory();
	for (const SuiteInfo& suite : suites)
	{
		std::vector<std::filesystem::path> sources;
		std::error_code error;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root / suite.directory, error))
		{
			if (entry.path().extension() == ".eva")
				sources.push_back(entry.path());
		}
		std::sort(sources.begin(), sources.end());
		for (const std::filesystem::path& source : sources)
		{
			// Lives until the process ends, like the test case.
			GoldenCase* golden = new GoldenCase();
			golden->suite = &suite;
			golden->name = std::string(suite.directory) + "." + source.stem().string();
			golden->source = source.generic_string();
			golden->stem = (source.parent_path() / source.stem()).generic_string();
			Test::AddTest("Golden", golden->name.c_str(), golden->source.c_str(), 1, RunCase, golden);
		}
	}
	Test::AddTest("Golden", "Files", __FILE__, __LINE__, CheckFiles, nullptr);
}
