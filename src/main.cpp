#include <iostream>
#include <numeric>
#include <algorithm>
#include <ctime>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <chrono>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "shader.hpp"
#include "watcher.hpp"

using namespace std::chrono_literals;

template <typename CharT, typename Traits, glm::length_t L, typename T, glm::qualifier Q>
auto& operator << (std::basic_ostream<CharT, Traits>& os, glm::vec<L, T, Q>& vec)
{
	if (L == 0) return os << "()";
	os << '(' << vec[0];
	for (glm::length_t i = 1; i < L; ++i) os << '|' << vec[i];
	return os << ')';
}

template <typename CharT, typename Traits, typename Rep, typename Period>
auto& operator << (std::basic_ostream<CharT, Traits>& os, std::chrono::duration<Rep, Period> duration)
{
	return os << duration.count();
}

int main()
{
	std::srand(std::time(0));

	if (!glfwInit()) return 0;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	auto window_size = glm::ivec2{1280, 720};
	auto window = glfwCreateWindow(window_size.x, window_size.y, "glsl", nullptr, nullptr);
	glfwSetKeyCallback(window, [] (GLFWwindow* window, int key, [[maybe_unused]] int scancode, [[maybe_unused]] int action, int mods) {
		if (!mods && key == GLFW_KEY_Q) glfwSetWindowShouldClose(window, true);
	});
	glfwMakeContextCurrent(window);
	// glfwSwapInterval(0);

	if (glewInit() != GLEW_OK) return 0;
	glClearColor(.2, .1, 0, 1);

	// std::this_thread::sleep_for(1s);
	auto display_program = createProgram({{GL_VERTEX_SHADER, "res/vertex.glsl"}, {GL_FRAGMENT_SHADER, "res/fragment.glsl"}});
	auto compute_program = createProgram({{GL_COMPUTE_SHADER, "res/compute.glsl"}});
	glUseProgram(compute_program);
	auto compute_shader_watcher = Watcher("res/compute.glsl", [&] () {
		if (compute_program > 0)
			glDeleteProgram(compute_program);
		compute_program = createProgram({{GL_COMPUTE_SHADER, "res/compute.glsl"}});
	});

	auto frame_tex_size = glm::uvec2(window_size);
	GLuint frame_tex_out;
	glGenTextures(1, &frame_tex_out);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frame_tex_out);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, frame_tex_size.x, frame_tex_size.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindImageTexture(0, frame_tex_out, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	glUseProgram(display_program);
	enum { vertex_position, vertex_uv };
	GLuint vao;
	GLuint vbos[2];
	glCreateVertexArrays(1, &vao);
	glGenBuffers(2, vbos);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(vertex_position);
	glBindBuffer(GL_ARRAY_BUFFER, vbos[vertex_position]);
	glm::vec2 vertex_positions[] = { {-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1} };
	glBufferData(GL_ARRAY_BUFFER, sizeof (vertex_positions), vertex_positions, GL_STATIC_DRAW);
	glVertexAttribPointer(vertex_position, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(vertex_uv);
	glBindBuffer(GL_ARRAY_BUFFER, vbos[vertex_uv]);
	glm::vec2 vertex_uvs[] = { {0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1} };
	glBufferData(GL_ARRAY_BUFFER, sizeof (vertex_uvs), vertex_uvs, GL_STATIC_DRAW);
	glVertexAttribPointer(vertex_uv, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindVertexArray(0);
	glUseProgram(0);

	using clock = std::chrono::steady_clock;
	auto start_time = clock::now();
	auto elapsed_time = 0ms;
	auto fps_print_time = elapsed_time;

	auto frame_count = 0ul;
	auto fps = 0ul;

	while (!glfwWindowShouldClose(window)) {
		auto cur_time = clock::now();
		auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - (start_time + elapsed_time));
		elapsed_time += delta_time;

		compute_shader_watcher.update();

		glfwPollEvents();
		glfwGetWindowSize(window, &window_size.x, &window_size.y);
		glViewport(0, 0, window_size.x, window_size.y);
		frame_tex_size = glm::uvec2(window_size);
		glBindTexture(GL_TEXTURE_2D, frame_tex_out);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, frame_tex_size.x, frame_tex_size.y, 0, GL_RGBA, GL_FLOAT, nullptr);
		glBindImageTexture(0, frame_tex_out, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

		glm::dvec2 mouse;
		glfwGetCursorPos(window, &mouse.x, &mouse.y);

		{ // launch compute shaders and draw to image
			glUseProgram(compute_program);
			glUniform1f(glGetUniformLocation(compute_program, "elapsed_time"), elapsed_time.count() / 1000.f);
			glUniform1f(glGetUniformLocation(compute_program, "delta_time"), delta_time.count() / 1000.f);
			glUniform2i(glGetUniformLocation(compute_program, "mouse_coord"), mouse.x, window_size.y - mouse.y);
			glUniform1i(glGetUniformLocation(compute_program, "frame_count"), frame_count);
			glDispatchCompute(frame_tex_size.x / 16 + 1, frame_tex_size.y / 16 + 1, 1);
		}

		// make sure writing to image has finished before read
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		{ // present image to screen
			glUseProgram(display_program);
			glUniform2i(glGetUniformLocation(display_program, "tex_size"), frame_tex_size.x, frame_tex_size.y);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindVertexArray(vao);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, frame_tex_out);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
			glfwSwapBuffers(window);
			glBindVertexArray(0);
		}

		++frame_count;
		++fps;

		if (fps_print_time.count() + 1000 < elapsed_time.count()) {
			fps_print_time += elapsed_time - fps_print_time;
			std::cout << fps << " fps" << std::endl;
			fps = 0;
		}
	}

	glfwTerminate();

	return 0;
}
