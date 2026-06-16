#include <EVA/Renderer.hpp>
#include <EVA/Camera.hpp>
#include <vector>

GLuint LineShader;
extern Camera camera;

struct LineVertex
{
	float3 position;
	float4 color;
};

static std::vector<LineVertex> pending_lines;

void RendererInitialize()
{
	LineShader = GLCompileShaderProgram("Lines");
}

void DrawLine(float3 a, float3 b, float4 color)
{
	pending_lines.push_back({ a, color });
	pending_lines.push_back({ b, color });
}

void RenderPendingLines()
{
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, pending_lines.size() * sizeof(pending_lines[0]), pending_lines.data(), GL_STATIC_DRAW);
	GL_ERROR_CHECK();

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(LineVertex), (void*)0);
	glVertexAttribPointer(1, 4, GL_FLOAT, false, sizeof(LineVertex), (void*)12);

	glUseProgram(LineShader);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUniformMatrix4fv(0, 1, false, (float*)&camera.view_projection_matrix);
	glDrawArrays(GL_LINES, 0, pending_lines.size());
	GL_ERROR_CHECK();

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	pending_lines.clear();
}

void DrawGrid(int size)
{
	float4 color = {1,1,1,0.2};
	float s = size;
	for (int i = -size; i <= size; i++)
	{
		DrawLine({(float)i, -s, 0}, {(float)i, (float)size, 0}, color);
		DrawLine({-s, (float)i, 0}, {(float)size, (float)i, 0}, color);
	}
}
