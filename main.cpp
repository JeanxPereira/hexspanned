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
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum PolygonMode
{
    PMFill,
    PMLine,
    PMPoint
};

const char *polygonModes[] = {
    "Fill",
    "Wireframe",
    "Points"
};

unsigned polygonModeGLConstants[] = {
    GL_FILL,
    GL_LINE,
    GL_POINT
};

enum MeshType
{
    MTTriangle,
    MTTriangleStrip,
    MTTriangleFan,
    MTQuad,
    MTQuadStrip,
    MTLine,
    MTLineStrip,
    MTLineLoop,
    MTPoint
};

const char *meshTypes[] = {
    "Triangle",
    "Triangle Strip",
    "Triangle Fan",
    "Quad",
    "Quad Strip",
    "Line",
    "Line Strip",
    "Line Loop",
    "Point"
};

unsigned meshTypeGLConstants[] = {
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP,
    GL_TRIANGLE_FAN,
    GL_QUADS,
    GL_QUAD_STRIP,
    GL_LINES,
    GL_LINE_STRIP,
    GL_LINE_LOOP,
    GL_POINTS
};

struct VisParams
{
    int vertexBufferStart = 0;
    int indexBufferStart = 0;
    int vertexCount = 3;
    int vertexStride = 12;
    bool bigEndian = true;
    bool backfaceCulling = false;
    float viewDistance = 3.0f;
    bool indexedDraw = false;
    bool halfWidthIndexes = false;
    PolygonMode polygonMode = PMFill;
    MeshType meshType = MTTriangle;
};

void copyToGPU(unsigned vbo, std::vector<uint8_t>& data, bool& bigEndian)
{
    // TODO: Endian swap should take into account offset, probably requiring a re-upload with each address change
    std::vector<uint8_t> endianSwappedData;
    std::vector<uint8_t> *pData = &data;

    if (bigEndian) {
        endianSwappedData.resize(data.size());

        for (int i = 0; i < data.size(); i += 4) {
            endianSwappedData[i] = data[i + 3];
            endianSwappedData[i + 1] = data[i + 2];
            endianSwappedData[i + 2] = data[i + 1];
            endianSwappedData[i + 3] = data[i];
        }

        pData = &endianSwappedData;
    }

    // Update buffer bindings
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (long) pData->size(), pData->data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

unsigned compileShader(const char *source, unsigned type)
{
    unsigned shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        std::cerr << "Error compiling shader: " << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment") << std::endl;
    }

    return shader;
}

bool
drawVisMenu(VisParams& visParams, int editAddress)
{
    bool needsReupload = false;

    ImGui::Begin("Vertex Visualization");
    ImGui::InputInt("Start", &visParams.vertexBufferStart, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
    if (ImGui::Button("Set to Highlighted Address")) {
        visParams.vertexBufferStart = editAddress;
    }

    if (visParams.indexedDraw) {
        ImGui::InputInt("Index Start", &visParams.indexBufferStart, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
        if (ImGui::Button("Set to Highlighted Address##STHA_IND")) {
            visParams.indexBufferStart = editAddress;
        }
    }

    ImGui::InputInt("Count", &visParams.vertexCount, 1, 100, 0);
    ImGui::InputInt("Stride", &visParams.vertexStride, 1, 100, 0);
    ImGui::Checkbox("Indexed Draw", &visParams.indexedDraw);

    if (visParams.indexedDraw) {
        ImGui::Checkbox("Half-Width (16-bit) Indexes", &visParams.halfWidthIndexes);
    }

    if (ImGui::Checkbox("Big-Endian", &visParams.bigEndian)) {
        needsReupload = true;
    }

    ImGui::Combo("Polygon Mode", (int *) &visParams.polygonMode, polygonModes, sizeof(polygonModes) / sizeof(char *));
    ImGui::Combo("Mesh Type", (int *) &visParams.meshType, meshTypes, sizeof(meshTypes) / sizeof(char *));
    ImGui::Checkbox("Backface Culling", &visParams.backfaceCulling);
    ImGui::InputFloat("View Distance", &visParams.viewDistance);
    ImGui::End();

    return needsReupload;
}

void loadFile(const std::string& name, std::vector<uint8_t>& data, unsigned& vbo, VisParams& visParams)
{
    // Load file to byte buffer
    std::ifstream in(name, std::ios::binary | std::ios::ate);
    auto size = in.tellg();
    data.resize(size);
    in.seekg(0, std::ios::beg);
    in.read((char *) data.data(), size);
    in.close();

    copyToGPU(vbo, data, visParams.bigEndian);
}

void render(const VisParams& visParams, unsigned int vao, unsigned int vbo, unsigned int program)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, visParams.vertexStride,
                          (void *) (uintptr_t) visParams.vertexBufferStart);
    glEnableVertexAttribArray(0);

    glUseProgram(program);

    if (visParams.backfaceCulling) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glPolygonMode(GL_FRONT_AND_BACK, polygonModeGLConstants[visParams.polygonMode]);

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(1, 1, 1) * visParams.viewDistance, glm::vec3(0, 0, 0),
                                 glm::vec3(0, 1, 0));
    auto model = glm::identity<glm::mat4>();
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(model));

    unsigned mode = meshTypeGLConstants[visParams.meshType];

    if (visParams.indexedDraw) {
        glDrawElements(mode, visParams.vertexCount,
                       visParams.halfWidthIndexes ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       ((void *) (uintptr_t) visParams.indexBufferStart));
    } else {
        glDrawArrays(mode, 0, visParams.vertexCount);
    }
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

    GLFWwindow *window = glfwCreateWindow(1280, 720, "hexspanned", nullptr, nullptr);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle().ScaleAllSizes(1.5f);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    MemoryEditor memEdit;
    ImGui::FileBrowser fileDialog;
    std::vector<uint8_t> data;
    unsigned vao, vbo;
    VisParams visParams;
    json prevFiles = json::array();

    {
        std::ifstream in(".hexspanned-history.json");
        if (in.is_open()) {
            in >> prevFiles;
            in.close();
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glEnable(GL_DEPTH_TEST);
    glPointSize(4.0f);

    unsigned vertex_shader = compileShader(
        "#version 330 core\n"
        "layout (location = 0) in vec3 pos;"
        "uniform mat4 projection;"
        "uniform mat4 view;"
        "uniform mat4 model;"
        "void main() {"
        "   gl_Position = projection * view * model * vec4(pos, 1.0);"
        "}",
        GL_VERTEX_SHADER);

    unsigned fragment_shader = compileShader(
        "#version 330 core\n"
        "out vec4 color;"
        "void main() {"
        "   color = vec4(1, 0, 0, 1);"
        "}",
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
            if (ImGui::BeginMenu("Open Recent")) {
                for (auto& recentFile: prevFiles) {
                    auto path = std::filesystem::path(recentFile.get<std::string>());
                    auto str_name = path.filename().string() + " (" + path.string() + ")";
                    if (ImGui::MenuItem(str_name.c_str())) {
                        loadFile(path.string(), data, vbo, visParams);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();

        if (drawVisMenu(visParams, (int) memEdit.DataEditingAddr)) {
            copyToGPU(vbo, data, visParams.bigEndian);
        }

        memEdit.DrawWindow("Hex View", data.data(), data.size());

        fileDialog.Display();

        if (fileDialog.HasSelected()) {
            loadFile(fileDialog.GetSelected().string(), data, vbo, visParams);

            auto absPath = std::filesystem::absolute(fileDialog.GetSelected()).string();
            if (std::find(prevFiles.begin(), prevFiles.end(), absPath) == prevFiles.end()) {
                prevFiles.push_back(absPath);
                if (prevFiles.size() > 10) prevFiles.erase(0);
                std::ofstream out(".hexspanned-history.json");
                if (out.is_open()) {
                    out << prevFiles;
                    out.close();
                }
            }
            fileDialog.Close();
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        bool canRenderRegular =
            visParams.vertexStride * visParams.vertexCount + visParams.vertexBufferStart < data.size();
        bool canRenderIndexed = visParams.vertexCount * 4 + visParams.indexBufferStart < data.size();
        bool canRender = visParams.indexedDraw ? canRenderIndexed : canRenderRegular;

        if (canRender) {
            render(visParams, vao, vbo, program);
        } else {
            ImGui::Begin("Oops!", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
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
