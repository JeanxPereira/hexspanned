#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "imgui_memory_editor.h"
#include "imfilebrowser.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

MemoryEditor memEdit;
ImGui::FileBrowser fileDialog;
std::vector<uint8_t> data;
unsigned vao, vbo;
glm::mat4 projection, view, model;

int vertex_buffer_start = 0;
int vertex_count = 3;
int vertex_stride = 12;
bool bigEndian = true;
bool wireframe = false;
bool backfaceCulling = false;
float viewDistance = 3.0f;
bool indexedDraw = false;
int indices_start;

void copyToGPU()
{
    std::vector<uint8_t> endian_swapped;
    std::vector<uint8_t> *pData = &data;
    if (bigEndian) {
        endian_swapped.resize(data.size());

        for (int i = 0; i < data.size(); i += 4) {
            endian_swapped[i] = data[i + 3];
            endian_swapped[i + 1] = data[i + 2];
            endian_swapped[i + 2] = data[i + 1];
            endian_swapped[i + 3] = data[i];
        }

        pData = &endian_swapped;
    }

    // Update buffer bindings
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, pData->size(), pData->data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

unsigned compile_shader(const char *source, unsigned type)
{
    unsigned shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        std::cerr << "Error compiling shader: " << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment") << std::endl;
    }

    return shader;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "hexspanned", NULL, NULL);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindVertexArray(vao);

    glEnable(GL_DEPTH_TEST);

    unsigned vertex_shader = compile_shader(
        "#version 330 core\nlayout (location = 0) in vec3 pos; uniform mat4 projection; uniform mat4 view; uniform mat4 model; void main() { gl_Position = projection * view * model * vec4(pos, 1.0); }",
        GL_VERTEX_SHADER);
    unsigned fragment_shader = compile_shader(
        "#version 330 core\nout vec4 color; void main() { color = vec4(1, 0, 0, 1); }",
        GL_FRAGMENT_SHADER);
    unsigned program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        glfwPollEvents();

        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) fileDialog.Open();

        ImGui::BeginMainMenuBar();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                fileDialog.Open();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();

        ImGui::Begin("Vertex Visualization");
        ImGui::InputInt("Start", &vertex_buffer_start, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
        if (ImGui::Button("Set to Highlighted Address")) {
            vertex_buffer_start = memEdit.DataEditingAddr;
        }
        if (indexedDraw) {
            ImGui::InputInt("Index Start", &indices_start, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
            if (ImGui::Button("Set to Highlighted Address#STHA_IND")) {
                indices_start = memEdit.DataEditingAddr;
            }
        }

        ImGui::InputInt("Count", &vertex_count, 1, 100, 0);
        ImGui::InputInt("Stride", &vertex_stride, 1, 100, 0);

        ImGui::Checkbox("Indexed Draw", &indexedDraw);


        if (ImGui::Checkbox("Big-Endian", &bigEndian)) {
            copyToGPU();
        }
        if (ImGui::Checkbox("Wireframe", &wireframe)) {
            if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        if (ImGui::Checkbox("Backface Culling", &backfaceCulling)) {
            if (backfaceCulling) {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
            } else {
                glDisable(GL_CULL_FACE);
            }
        }
        ImGui::InputFloat("View Distance", &viewDistance);
        ImGui::End();

        memEdit.DrawWindow("Hex View", data.data(), data.size());

        fileDialog.Display();

        if (fileDialog.HasSelected()) {
            // Load file to byte buffer
            std::ifstream in(fileDialog.GetSelected(), std::ios::binary | std::ios::ate);
            auto size = in.tellg();
            data.resize(size);
            in.seekg(0, std::ios::beg);
            in.read((char *) data.data(), size);
            in.close();

            copyToGPU();

            fileDialog.Close();
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_stride, (void *) (uintptr_t) vertex_buffer_start);
        glEnableVertexAttribArray(0);

        glUseProgram(program);


        projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);
        view = glm::lookAt(glm::vec3(1, 1, 1) * viewDistance, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        model = glm::identity<glm::mat4>();
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(model));

        if ((indexedDraw && vertex_count * 4 + indices_start < data.size()) ||
            (!indexedDraw && vertex_stride * vertex_count + vertex_buffer_start < data.size())) {
            if (indexedDraw) {
                glDrawElements(GL_TRIANGLES, vertex_count, GL_UNSIGNED_INT, ((void *) (uintptr_t) indices_start));
            } else {
                glDrawArrays(GL_TRIANGLES, 0, vertex_count);
            }
        } else {
            ImGui::Begin("Oops!", NULL, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("The current render parameters would read past the end of the file!");
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
