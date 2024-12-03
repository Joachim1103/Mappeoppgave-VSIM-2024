//sources
// Matematikk3_V24_VSIM101_H24 Document
// Assistance from ChatGPT

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    out vec3 FragPos;
    out vec3 Normal;

    void main()
    {
        FragPos = vec3(model * vec4(aPos, 1.0)); // Calculate world-space position
        Normal = mat3(transpose(inverse(model))) * aNormal; // Transform normal
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core

    in vec3 FragPos;
    in vec3 Normal;

    uniform vec3 lightPos;
    uniform vec3 viewPos; // Camera position
    uniform vec3 lightColor;
    uniform vec3 objectColor;

    out vec4 FragColor;

    void main()
    {
        // Ambient
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * lightColor;

        // Diffuse
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Specular
        float specularStrength = 1.0;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;

        vec3 result = (ambient + diffuse + specular) * objectColor;
        FragColor = vec4(result, 1.0);
    }
)";

struct Point
{
    float x, y, z;
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

glm::vec3 cameraPos(0.0f, 1.0f, 5.0f); 
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f; 
float lastFrame = 0.0f; 
float yaw = -90.0f;     
float pitch = 0.0f;
float lastX = 960.0f, lastY = 540.0f;
bool firstMouse = true;

bool keys[1024] = { false };

float roll = 0.0f;
bool isRMBPressed = false;

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Error: Shader Compilation Failed\n" << infoLog << std::endl;
    }
    return shader;
}

GLuint createShaderProgram()
{
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Error: Program Linking Failed\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

std::vector<Point> loadTerrainData(const std::string& filename)
{
    std::vector<Point> points;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return points;
    }

    int numPoints;
    file >> numPoints;

    float x, y, z;
    while (file >> x >> y >> z)
    {
        points.push_back({ x, y, z });
    }

    file.close();
    std::cout << "Loaded " << points.size() << " points." << std::endl;
    return points;
}

bool isPointInCircumcircle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::mat4 mat(1.0f);
    mat[0] = glm::vec4(a.x - p.x, a.y - p.y, a.z - p.z, (a.x - p.x) * (a.x - p.x) + (a.y - p.y) * (a.y - p.y));
    mat[1] = glm::vec4(b.x - p.x, b.y - p.y, b.z - p.z, (b.x - p.x) * (b.x - p.x) + (b.y - p.y) * (b.y - p.y));
    mat[2] = glm::vec4(c.x - p.x, c.y - p.y, c.z - p.z, (c.x - p.x) * (c.x - p.x) + (c.y - p.y) * (c.y - p.y));
    mat[3] = glm::vec4(0, 0, 0, 1.0f);

    return glm::determinant(mat) > 0;
}

void normalizePoints(std::vector<Point>& points)
{
    if (points.empty()) return;

    float minX = points[0].x, maxX = points[0].x;
    float minY = points[0].y, maxY = points[0].y;
    float minZ = points[0].z, maxZ = points[0].z;

    for (const auto& p : points)
    {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
        if (p.z < minZ) minZ = p.z;
        if (p.z > maxZ) maxZ = p.z;
    }

    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    float centerZ = (minZ + maxZ) / 2.0f;

    float scale = std::max({ maxX - minX, maxY - minY, maxZ - minZ }) / 2.0f;
    if (scale == 0.0f) scale = 1.0f;

    for (auto& p : points)
    {
        p.x = (p.x - centerX) / scale;
        p.y = (p.y - centerY) / scale;
        p.z = (p.z - centerZ) / scale;
    }

    std::cout << "Points normalized to range [-1, 1]." << std::endl;
}

void triangulateSimplified(const std::vector<Point>& points, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices)
{
    if (points.size() < 3)
    {
        std::cerr << "Not enough points to create a mesh." << std::endl;
        return;
    }

    vertices.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i)
    {
        vertices[i].position = glm::vec3(points[i].x, points[i].y, points[i].z);
        vertices[i].normal = glm::vec3(0.0f);
    }

    std::vector<Point> sortedPoints = points;
    std::sort(sortedPoints.begin(), sortedPoints.end(), [](const Point& a, const Point& b) {
        return (a.x < b.x) || (a.x == b.x && a.z < b.z);
        });

    for (size_t i = 0; i < sortedPoints.size() - 2; ++i)
    {
        glm::vec3 p0(sortedPoints[i].x, sortedPoints[i].y, sortedPoints[i].z);
        glm::vec3 p1(sortedPoints[i + 1].x, sortedPoints[i + 1].y, sortedPoints[i + 1].z);
        glm::vec3 p2(sortedPoints[i + 2].x, sortedPoints[i + 2].y, sortedPoints[i + 2].z);

        indices.push_back(i);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }

    float maxEdgeLength = 0.15f;
    std::vector<unsigned int> filteredIndices;

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        glm::vec3 v0 = vertices[indices[i]].position;
        glm::vec3 v1 = vertices[indices[i + 1]].position;
        glm::vec3 v2 = vertices[indices[i + 2]].position;

        float d01 = glm::distance(v0, v1);
        float d12 = glm::distance(v1, v2);
        float d20 = glm::distance(v2, v0);

        if (d01 < maxEdgeLength && d12 < maxEdgeLength && d20 < maxEdgeLength)
        {
            filteredIndices.push_back(indices[i]);
            filteredIndices.push_back(indices[i + 1]);
            filteredIndices.push_back(indices[i + 2]);
        }
        else
        {
            std::cout << "Removed triangle: " << indices[i] << ", " << indices[i + 1] << ", " << indices[i + 2]
                << " | Edges: " << d01 << ", " << d12 << ", " << d20 << std::endl;
        }
    }

    indices = filteredIndices;
    std::cout << "Filtered triangulation completed. Remaining triangles: " << indices.size() / 3 << std::endl;
}

void calculateNormals(std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    for (auto& vertex : vertices)
    {
        vertex.normal = glm::vec3(0.0f);
    }

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        glm::vec3 v0 = vertices[indices[i]].position;
        glm::vec3 v1 = vertices[indices[i + 1]].position;
        glm::vec3 v2 = vertices[indices[i + 2]].position;

        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        vertices[indices[i]].normal += normal;
        vertices[indices[i + 1]].normal += normal;
        vertices[indices[i + 2]].normal += normal;
    }

    for (auto& vertex : vertices)
    {
        vertex.normal = glm::normalize(vertex.normal);
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    static float lastX = 960.0f, lastY = 540.0f;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastX;
    float yOffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    if (isRMBPressed)
    {
        roll += xOffset;
    }
    else
    {
        yaw += xOffset;
        pitch += yOffset;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS)
            keys[key] = true;
        else if (action == GLFW_RELEASE)
            keys[key] = false;
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
            isRMBPressed = true;
        else if (action == GLFW_RELEASE)
            isRMBPressed = false;
    }
}

void processInput()
{
    float cameraSpeed = 2.5f * deltaTime;
    if (keys[GLFW_KEY_W])
        cameraPos += cameraSpeed * cameraFront;
    if (keys[GLFW_KEY_S])
        cameraPos -= cameraSpeed * cameraFront;
    if (keys[GLFW_KEY_A])
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys[GLFW_KEY_D])
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

void renderLoop(GLFWwindow* window, GLuint shaderProgram, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput();

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1920.0f / 1080.0f, 0.1f, 100.0f);

        glm::mat4 rollRotation = glm::rotate(glm::mat4(1.0f), glm::radians(roll), cameraFront);
        view = rollRotation * view;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glm::vec3 lightPos(3.0f, 5.0f, 3.0f);
        glm::vec3 lightColor(1.0f, 0.95f, 0.9f);
        glm::vec3 objectColor(0.2f, 0.6f, 0.3f);

        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(objectColor));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "3D Terrain Viewer", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    std::vector<Point> points = loadTerrainData("Elevation Data.txt");
    if (points.empty())
    {
        std::cerr << "No terrain data loaded." << std::endl;
        return -1;
    }

    normalizePoints(points);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    auto start = std::chrono::high_resolution_clock::now();

    triangulateSimplified(points, vertices, indices);

    std::cout << "Vertices: " << vertices.size() << ", Indices: " << indices.size() << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Triangulation time: " << elapsed.count() << " seconds" << std::endl;

    calculateNormals(vertices, indices);

    GLuint shaderProgram = createShaderProgram();
    renderLoop(window, shaderProgram, vertices, indices);

    glfwTerminate();
    return 0;
}


