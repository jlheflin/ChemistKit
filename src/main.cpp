#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <fstream>
#include <unistd.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>
  
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui_internal.h"

static float g_zoom = 0.2f;

struct OrbitCamera {
  glm::vec3 target = glm::vec3(0.0f);
  float distance = 10.0f;
  float yaw = 0.0f;
  float pitch = 0.3f;

  float fov = 45.0f;
  float nearPlane = 0.01f;
  float farPlane = 1000.0f;

  glm::vec3 position() const {
    glm::vec3 dir;
    dir.x = cos(pitch) * sin(yaw);
    dir.y = sin(pitch);
    dir.z = cos(pitch) * cos(yaw);
    return target + dir * distance;
  }

  glm::mat4 view() const {
    return glm::lookAt(position(), target, glm::vec3(0,1,0));
  }

  glm::mat4 projection(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
  }
};

static OrbitCamera g_cam;

size_t getMemoryUsageMB() {
  std::ifstream file("/proc/self/statm");
  size_t size, resident;
  file >> size >> resident;
  return resident * (size_t)sysconf(_SC_PAGESIZE) / (1024 * 1024);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }

  g_cam.distance *= std::exp(-0.1f * float(yoffset));
  g_cam.distance = glm::clamp(g_cam.distance, 0.2f, 500.0f);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

struct Mesh {
  std::vector<float> vertices;
  std::vector<unsigned int> indices;
};

struct Instance {
  float x, y ,z;
  float radius;
  float r, g, b;
};

Mesh createSphere(float radius, int sectorCount, int stackCount) {
  Mesh mesh;
  const float pi = M_PI;

  float x, y, z, xy;
  float sectorStep = 2 * pi / sectorCount;
  float stackStep = pi / stackCount;
  float sectorAngle, stackAngle;

  for (int i = 0; i <= stackCount; i++) {
    stackAngle = pi / 2 - i * stackStep;
    xy = radius * cosf(stackAngle);
    y = radius * sinf(stackAngle);

    for (int j = 0; j <= sectorCount; j++) {
      sectorAngle = j * sectorStep;

      x = xy * cosf(sectorAngle);
      z = xy * sinf(sectorAngle);

      mesh.vertices.push_back(x);
      mesh.vertices.push_back(y);
      mesh.vertices.push_back(z);

      mesh.vertices.push_back(x / radius);
      mesh.vertices.push_back(y / radius);
      mesh.vertices.push_back(z / radius);
    }
  }

  int k1, k2;
  for (int i = 0; i < stackCount; i++) {
    k1 = i * (sectorCount + 1);
    k2 = k1 + sectorCount + 1;

    for (int j = 0; j < sectorCount; j++, k1++, k2++) {
      if (i != 0) {
        mesh.indices.push_back(k1);
        mesh.indices.push_back(k2);
        mesh.indices.push_back(k1 + 1);
      }

      if (i != (stackCount - 1)) {
        mesh.indices.push_back(k1 + 1);
        mesh.indices.push_back(k2);
        mesh.indices.push_back(k2 + 1);
      }
    }
  }

  return mesh;
}

static GLuint compileShader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);

  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, & ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(s, 1024, nullptr, log);
    std::cerr << "Shader compile error:\n" << log << "\n";
  }
  return s;
}

static GLuint createProgram(const char* vsSrc, const char* fsSrc) {
  GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);

  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);

  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(p, 1024, nullptr, log);
    std::cerr << "Program link error:\n" << log << "\n";
  }

  glDeleteShader(vs);
  glDeleteShader(fs);
  return p;
}

struct Atom {
  std::string sym;
  int atomicNumber;
  float x, y, z;
};

struct AtomDraw {
  float x, y, z;
  float radius;
  float r, g, b;
};

struct Molecule {
  std::vector<Atom> atoms;

  int size() const {
    return (int)atoms.size();
  }

  std::vector<float> coords1D() const {
    std::vector<float> out;
    out.reserve(atoms.size() * 3);

    for (const Atom& a : atoms) {
      out.push_back(a.x);
      out.push_back(a.y);
      out.push_back(a.z);
    }

    return out;
  }

  std::vector<std::string> symbols() const {
    std::vector<std::string> out;
    out.reserve(atoms.size());

    for (const Atom& a : atoms) {
      out.push_back(a.sym);
    }

    return out;
  }
};

Molecule read_xyz(std::string filename) {
  std::ifstream xyz_file(filename);
  if (!xyz_file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename + "\n");
  }

  Molecule mol;
  int natoms;
  std::string comment;

  if (!(xyz_file >> natoms)){
    throw std::runtime_error("Failed to read atom count.\n");
  }

  xyz_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  std::getline(xyz_file, comment);

  std::string element;
  float x, y, z;
  while (xyz_file >> element >> x >> y >> z) {
    int atomicNum = 0;
    if (element == "H") {
      atomicNum = 1;
    } else if (element == "O") {
      atomicNum = 8;
    } else {
      std::cerr << "Unknown atom: " << element << "\n";
      throw std::runtime_error("Unknown atom: " + element + "\n");
    }

    Atom a = {
      .sym = element,
      .atomicNumber = atomicNum,
      .x = x,
      .y = y,
      .z = z
    };

    mol.atoms.push_back(a);
  }
  if ((int)mol.atoms.size() != natoms) {
    throw std::runtime_error(
      "Number of atoms not equal to mol.atoms: " + std::to_string(natoms) + " : " + std::to_string(mol.atoms.size())
    );
  }
  return mol;
}

AtomDraw toDraw(const Atom& a) {
  AtomDraw d{};
  d.x = a.x;
  d.y = a.y;
  d.z = a.z;

  switch(a.atomicNumber) {
    case 1: d.radius=(25.0/53.0)*0.2; d.r=0.8f; d.g=0.8f; d.b=0.8f; break;
    case 8: d.radius=(60.0/53.0)*0.2; d.r=1.0f; d.g=0.0f; d.b=0.0f; break;
    default: d.radius=(53.0)*0.2; d.r=0.0f; d.g=0.0f; d.b=0.0f;
  }

  return d;
}

static float elementRadius[119];
static float elementColor[119][3];

static uint32_t rng_state = 123456789;

inline float fastRand() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;

  return (rng_state & 0x00FFFFFF) / 16777216.0f;
}

inline float randRange(float min, float max) {
  return min + (max - min) * fastRand();
}

auto main() -> int {
  if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
  }
  glfwInit();
  int platform = glfwGetPlatform();
  switch (platform)
  {
      case GLFW_PLATFORM_WIN32:
          printf("Platform: Win32\n");
          break;

      case GLFW_PLATFORM_COCOA:
          printf("Platform: Cocoa (macOS)\n");
          break;

      case GLFW_PLATFORM_WAYLAND:
          printf("Platform: Wayland\n");
          break;

      case GLFW_PLATFORM_X11:
          printf("Platform: X11\n");
          break;

      case GLFW_PLATFORM_NULL:
          printf("Platform: Null (no window system)\n");
          break;

      default:
          printf("Unknown platform\n");
          break;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, 1);
  

  Molecule mol = read_xyz("./waterbox-1195.xyz");
  for (size_t i = 0; i < mol.atoms.size(); i++) {
    auto a = mol.atoms[i];
    std::cout << "[" << i << "]: " << a.sym
              << "(Z= " << a.atomicNumber << ") (" << a.x << ", " << a.y << ", " << a.z << ")\n";
  }

  // for (int i = 0; i < 50; i++) {
  //   Atom a;
  //   a.sym = "H";
  //   a.atomicNumber = 1;
  //   a.x = randRange(-5.0f, 5.0f);
  //   a.y = randRange(-5.0f, 5.0f);
  //   a.z = randRange(-5.0f, 5.0f);

  //   atoms.push_back(a);
  // }

  GLFWwindow* window = glfwCreateWindow(1200, 800, "Test", NULL, NULL);
  GLFWwindow* window2 = glfwCreateWindow(1200, 800, "Test", NULL, window);

  if (window == nullptr) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }
  if (window2 == nullptr) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    return -1;
  }
  std::cout << "Vendor:   " << glGetString(GL_VENDOR)   << "\n";
  std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
  std::cout << "Version:  " << glGetString(GL_VERSION)  << "\n";

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");

  int fbw, fbh;
  glfwGetFramebufferSize(window, &fbw, &fbh);
  glViewport(0, 0, fbw, fbh);
  

  const char* vsSrc = R"(#version 330 core
  layout(location=0) in vec3 aPos;
  layout(location=1) in vec3 aNrm;

  layout(location=2) in vec3 iPos;
  layout(location=3) in float iRadius;
  layout(location=4) in vec3 iColor;

  out vec3 vNrmVS;
  out vec3 vColor;

  uniform mat4 uView;
  uniform mat4 uProj;

  void main() {
    vec3 worldPos = (aPos * iRadius) + iPos;

    vNrmVS = mat3(uView) * aNrm;
    vColor = iColor;

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
  }
  )";

  const char* fsSrc = R"(#version 330 core
  in vec3 vNrmVS;
  in vec3 vColor;
  out vec4 FragColor;

  void main() {
    vec3 N = normalize(vNrmVS);
    vec3 L = normalize(vec3(0.0, 0.0, 1.0));

    float diff = max(dot(N, L), 0.0);
    vec3 color = vColor * (0.2 + 0.8 * diff);
    FragColor = vec4(color, 1.0);
  }
  )";

  GLuint program = createProgram(vsSrc, fsSrc);

  Mesh sphere = createSphere(1.0f, 100, 100);

  GLuint VAO, VBO, EBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER,
               sphere.vertices.size() * sizeof(float),
               sphere.vertices.data(),
               GL_STATIC_DRAW
             );

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               sphere.indices.size() * sizeof(unsigned int),
               sphere.indices.data(),
               GL_STATIC_DRAW
             );

  glVertexAttribPointer(
    0,
    3,
    GL_FLOAT,
    GL_FALSE,
    6 * sizeof(float),
    (void*)0
  );
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(
    1,
    3,
    GL_FLOAT,
    GL_FALSE,
    6 * sizeof(float),
    (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  GLuint instanceVBO;
  glGenBuffers(1, &instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER,
               mol.atoms.size() * sizeof(Instance),
               nullptr,
               GL_STREAM_DRAW);

  // attach instance attributes to the SAME VAO
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

  // iPos (location=2) => offset x
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Instance),
                        (void*)offsetof(Instance, x));
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);

  // iRadius (location=3) => offset radius
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Instance),
                        (void*)offsetof(Instance, radius));
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);

  // iColor (location=4) => offset r
  glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Instance),
                        (void*)offsetof(Instance, r));
  glEnableVertexAttribArray(4);
  glVertexAttribDivisor(4, 1);
  // glBindVertexArray(VAO);
  // glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

  // // layout(location=2) vec3 iPos
  // glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
  // glEnableVertexAttribArray(2);
  // glVertexAttribDivisor(2, 1);

  // // layout(location=3) float iRadius
  // glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
  // glEnableVertexAttribArray(2);
  // glVertexAttribDivisor(2, 1);

  // // layout(location=2) vec3 iPos
  // glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
  // glEnableVertexAttribArray(2);
  // glVertexAttribDivisor(2, 1);

  glfwSwapInterval(1);
  std::vector<Instance> instances(mol.atoms.size());
  std::vector<std::vector<Instance>> steps(50000, instances);

  for (size_t i = 0; i < mol.atoms.size(); i++) {
    instances[i] = {
      .x=mol.atoms[i].x,
      .y=mol.atoms[i].y,
      .z=mol.atoms[i].z};
  }
  glm::vec3 c(0);
  for (auto& a : mol.atoms) c += glm::vec3(a.x,a.y,a.z);
  c /= float(mol.atoms.size());
  g_cam.target = c;
  g_cam.distance = 30.0f; // tweak

  for (size_t step = 0; step < steps.size(); step++) {
    for (size_t i = 0; i < mol.atoms.size(); i++) {
      mol.atoms[i].x += randRange(-0.05f, 0.05f);
      mol.atoms[i].y += randRange(-0.05f, 0.05f);
      mol.atoms[i].z += randRange(-0.05f, 0.05f);
      AtomDraw d  = toDraw(mol.atoms[i]);
      instances[i] = {d.x, d.y, d.z, d.radius, d.r, d.g, d.b};
    }
    steps[step] =  instances;
  }

  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferSubData(GL_ARRAY_BUFFER,
                  0,
                  instances.size() * sizeof(Instance),
                  instances.data()
                  );

  size_t step = 0;
  double lastTime = glfwGetTime();
  int frameCount = 0;

  while (glfwWindowShouldClose(window) == 0) {
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---------- ImGui begin frame (MUST be before any ImGui calls) ----------
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
          
        }
        if (ImGui::MenuItem("Save Screenshot", "Ctrl+S")) {
          
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {
          glfwSetWindowShouldClose(window, 1);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Reset Camera", "R")) {
          g_cam.yaw = 0.0f;
          g_cam.pitch = 0.3f;
          g_cam.distance = 30.0f;
        }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }

    static bool dragging = false;
    static double lastX = 0.0, lastY = 0.0;

    if (!io.WantCaptureMouse) {
      if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (!dragging) {
          dragging = true;
          lastX = x;
          lastY = y;
        } else {
          float dx = float(x - lastX);
          float dy = float(y - lastY);
          lastX = x;
          lastY = y;

          float sensitivity = 0.005f;
          g_cam.yaw -= dx * sensitivity;
          g_cam.pitch += dy * sensitivity;

          float limit = glm::radians(89.0f);
          g_cam.pitch = glm::clamp(g_cam.pitch, -limit, limit);
        }
      } else {
        dragging = false;
      }
    }

    // ---------- Dockspace with fixed left panel (20%) ----------
    ImGuiWindowFlags dock_flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpaceRoot", nullptr, dock_flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    static bool layout_built = false;
    if (!layout_built) {
      layout_built = true;

      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

      ImGuiID dock_left = 0, dock_right = 0;
      dock_left = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Left, 0.20f, &dock_left, &dock_right);

      ImGui::DockBuilderDockWindow("LeftPanel", dock_left);
      ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    // ---------- Your docked windows ----------
    ImGui::Begin("LeftPanel");
    ImGui::Text("Controls go here");
    ImGui::Text("Step: %zu", step);
    ImGui::End();

    // ---------- Your OpenGL draw ----------
    glEnable(GL_DEPTH_TEST);


    

    glUseProgram(program);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

    glm::mat4 view = g_cam.view();
    glm::mat4 proj = g_cam.projection(aspect);

    glUniformMatrix4fv(
      glGetUniformLocation(program, "uView"),
      1, GL_FALSE, glm::value_ptr(view)
    );

    glUniformMatrix4fv(
      glGetUniformLocation(program, "uProj"),
      1, GL_FALSE, glm::value_ptr(proj)
    );


    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    steps[step].size() * sizeof(Instance),
                    steps[step].data());

    glDrawElementsInstanced(GL_TRIANGLES,
                            (GLsizei)sphere.indices.size(),
                            GL_UNSIGNED_INT,
                            0,
                            (GLsizei)mol.atoms.size());

    // ---------- Render ImGui on top ----------
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // ---------- Events / swap ----------
    glfwPollEvents();
    glfwSwapBuffers(window);

    // ---------- FPS + RAM (once per second) ----------
    frameCount++;
    double currentTime = glfwGetTime();
    double delta = currentTime - lastTime;
    if (delta >= 1.0) {
      std::ostringstream ss;
      double fps = frameCount / delta;
      ss << "FPS: " << fps << "  RAM: " << getMemoryUsageMB() << " MB";
      glfwSetWindowTitle(window, ss.str().c_str());
      frameCount = 0;
      lastTime = currentTime;
    }

    step = (step + 1) % steps.size();
  }

  glViewport(0, 0, 800, 600);

  glfwTerminate();
  return 0;
}
