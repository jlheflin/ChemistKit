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

size_t getMemoryUsageMB() {
  std::ifstream file("/proc/self/statm");
  size_t size, resident;
  file >> size >> resident;
  return resident * (size_t)sysconf(_SC_PAGESIZE) / (1024 * 1024);
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

AtomDraw toDraw(const Atom& a) {
  AtomDraw d{};
  d.x = a.x;
  d.y = a.y;
  d.z = a.z;

  d.radius = 0.25f;
  d.r = 0.8f;
  d.g = 0.8f;
  d.b = 0.8f;

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
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, 1);
  std::vector<Atom> atoms;

  for (int i = 0; i < 500000; i++) {
    Atom a;
    a.sym = "H";
    a.atomicNumber = 1;
    a.x = randRange(-5.0f, 5.0f);
    a.y = randRange(-5.0f, 5.0f);
    a.z = randRange(-5.0f, 5.0f);

    atoms.push_back(a);
  }

  GLFWwindow* window = glfwCreateWindow(1200, 800, "Test", NULL, NULL);

  if (window == nullptr) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    return -1;
  }
  std::cout << "Vendor:   " << glGetString(GL_VENDOR)   << "\n";
  std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
  std::cout << "Version:  " << glGetString(GL_VERSION)  << "\n";

  int fbw, fbh;
  glfwGetFramebufferSize(window, &fbw, &fbh);
  glViewport(0, 0, fbw, fbh);
  

  const char* vsSrc = R"(#version 330 core
  layout(location=0) in vec3 aPos;
  layout(location=1) in vec3 aNrm;

  layout(location=2) in vec3 iPos;

  out vec3 vNrm;
  out vec3 vColor;

  uniform float uAspect;
  uniform float uZoom;
  uniform float uRadius;
  uniform vec3 uColor;

  void main() {
    vNrm = aNrm;
    vColor = uColor;
    
    vec4 pos = vec4((aPos * uRadius) + iPos, 1.0);
    pos.x /= uAspect;
    pos.xyz *= uZoom;
    gl_Position = pos;
  }
  )";

  const char* fsSrc = R"(#version 330 core
  in vec3 vNrm;
  in vec3 vColor;
  out vec4 FragColor;

  uniform vec3 uColor;

  void main() {
    float lighting = max(dot(normalize(vNrm), normalize(vec3(0.5, 0.8, 0.2))), 0.0);
    FragColor = vec4(vColor * (0.2 + 0.8 * lighting), 1.0);
  }
  )";

  GLuint program = createProgram(vsSrc, fsSrc);

  Mesh sphere = createSphere(1.0f, 12, 6);

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
               atoms.size() * sizeof(Instance),
               nullptr,
               GL_STREAM_DRAW);

  // attach instance attributes to the SAME VAO
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

  // layout(location=2) vec3 iPos
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);

  glfwSwapInterval(1);
  std::vector<Instance> instances(atoms.size());
  std::vector<std::vector<Instance>> steps(500, instances);

  for (size_t i = 0; i < atoms.size(); i++) {
    instances[i] = {atoms[i].x, atoms[i].y, atoms[i].z};
  }

  for (size_t step = 0; step < steps.size(); step++) {
    for (size_t i = 0; i < atoms.size(); i++) {
      atoms[i].x += randRange(-0.1f, 0.1f);
      atoms[i].y += randRange(-0.1f, 0.1f);
      atoms[i].z += randRange(-0.1f, 0.1f);
      AtomDraw d  = toDraw(atoms[i]);
      instances[i] = {d.x, d.y, d.z};
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

    glEnable(GL_DEPTH_TEST);
    GLint loc = glGetUniformLocation(program, "uAspect");
    GLint locZoom = glGetUniformLocation(program, "uZoom");
    GLint locRadius = glGetUniformLocation(program, "uRadius");
    GLint locColor = glGetUniformLocation(program, "uColor");

    glUseProgram(program);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    float zoom = 0.2f;

    
    glUniform1f(loc, aspect);
    glUniform1f(locZoom, zoom);
    glUniform1f(locRadius, 0.25f);
    glUniform3f(locColor, 0.8f, 0.8f, 0.8f);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    steps[step].size() * sizeof(Instance),
                    steps[step].data()
    );

    glDrawElementsInstanced(
        GL_TRIANGLES,
        (GLsizei)sphere.indices.size(),
        GL_UNSIGNED_INT,
        0,
        (GLsizei)atoms.size()
    );


    glfwPollEvents();

    glfwSwapBuffers(window);
    frameCount++;
    double currentTime = glfwGetTime();
    double delta = currentTime - lastTime;
    if (delta >= 1.0) {
      std::ostringstream ss;
      double fps = frameCount / delta;
      ss << "FPS: " << fps << " "
         << "RAM: " << getMemoryUsageMB() << " MB";
      glfwSetWindowTitle(window, ss.str().c_str());
      frameCount = 0;
      lastTime = currentTime;
    }



    step = (step+ 1) % steps.size();
  }

  glViewport(0, 0, 800, 600);

  glfwTerminate();
  return 0;
}
