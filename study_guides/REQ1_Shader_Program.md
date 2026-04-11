# Requirement 1: Shader Program — Complete Study Guide

---

## 🧠 What is a Shader?

When your computer draws a 3D scene to the screen, it has to figure out **two things for every triangle**:

1. **Where** should each corner (vertex) of the triangle appear on screen?
2. **What color** should each pixel inside the triangle be?

These two questions are answered by two small programs that run on your **GPU (Graphics Processing Unit)** called **shaders**.

| Shader Type | Runs Once Per | Job |
|---|---|---|
| **Vertex Shader** (`.vert`) | Every vertex (corner) | Decides screen position |
| **Fragment Shader** (`.frag`) | Every pixel | Decides pixel color |

Shaders are written in a language called **GLSL** (OpenGL Shading Language), which looks like C.

---

## 🏗️ What is a Shader Program?

A **Shader Program** is when you take a vertex shader and a fragment shader and **link them together** into one combined GPU program. After linking, you can tell OpenGL: *"Use this shader program when drawing the next object"*.

The whole process looks like this:

```
[Vertex Shader file .vert]   →  compile  →  shader object
[Fragment Shader file .frag] →  compile  →  shader object
                                              ↓
                                         link together
                                              ↓
                                      [Shader Program]
```

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/shader/shader.hpp` | Class declaration — defines what `ShaderProgram` can do |
| `source/common/shader/shader.cpp` | Class implementation — **the actual code that compiles and links shaders** |
| `assets/shaders/triangle.vert` | A vertex shader that draws a colored triangle |
| `assets/shaders/color-mixer.frag` | A fragment shader that mixes color channels |
| `assets/shaders/checkerboard.frag` | A fragment shader that draws a checkerboard pattern |

---

## 📖 Understanding `shader.hpp` — The Class Declaration

```cpp
class ShaderProgram {
private:
    GLuint program;  // This is an ID number (integer) that OpenGL assigns 
                     // to the shader program. Think of it like a receipt number.
public:
    ShaderProgram(){
        program = glCreateProgram(); // Ask OpenGL to create a program, get back its ID
    }
    ~ShaderProgram(){
        glDeleteProgram(program);    // When done, tell OpenGL to free the memory
    }

    bool attach(const std::string &filename, GLenum type) const;  // Load+compile a shader
    bool link() const;          // Link vertex + fragment shaders together
    void use();                 // Activate this program before drawing
    GLint getUniformLocation(const std::string &name); // Find a uniform variable by name

    // Many overloaded "set" functions to send data to the GPU:
    void set(const std::string &uniform, GLfloat value);   // send 1 float
    void set(const std::string &uniform, glm::vec4 value); // send 4 floats (e.g. color RGBA)
    void set(const std::string &uniform, glm::mat4 matrix);// send a 4×4 matrix
    // ... and more
};
```

### What is `GLuint`?
`GLuint` is just an `unsigned int` — a positive whole number. OpenGL uses numbers as IDs for everything it creates (programs, textures, buffers, etc.).

### What is a Uniform?
When the CPU (your C++ code) wants to send data **to the GPU shader**, it uses something called a **uniform variable**. Think of it as a global variable that the shader reads from.

Example: your C++ code sends the triangle's color as a uniform → the shader reads it and uses it.

---

## 📖 Understanding `shader.cpp` — The Implementation

### The `attach()` Function (Loading and Compiling One Shader)

```cpp
bool our::ShaderProgram::attach(const std::string &filename, GLenum type) const {
    // Step 1: Open the .vert or .frag file and read it as a string
    std::ifstream file(filename);
    std::string sourceString = std::string(std::istreambuf_iterator<char>(file), 
                                           std::istreambuf_iterator<char>());
    const char* sourceCStr = sourceString.c_str();
    file.close();

    // Step 2: Create a shader object (vertex OR fragment, based on `type`)
    GLuint shader = glCreateShader(type);
    //  type is GL_VERTEX_SHADER or GL_FRAGMENT_SHADER

    // Step 3: Give the GLSL source code to OpenGL
    glShaderSource(shader, 1, &sourceCStr, nullptr);

    // Step 4: Tell OpenGL to compile the source code
    glCompileShader(shader);

    // Step 5: Check if compilation worked
    std::string error = checkForShaderCompilationErrors(shader);
    if (!error.empty()) {
        // Print the error and return false (failure)
        glDeleteShader(shader);
        return false;
    }

    // Step 6: Attach the compiled shader to the program
    glAttachShader(program, shader);

    // Step 7: Delete the shader object — it is now "baked into" the program
    glDeleteShader(shader);

    return true; // success
}
```

**Step-by-step in plain English:**
1. Open the text file (e.g. `triangle.vert`) from disk.
2. Read all the GLSL code into a C++ string.
3. Create an empty shader slot in OpenGL.
4. Hand the GLSL code to OpenGL.
5. Compile it (like compiling C++ code, but on the GPU).
6. Check for errors (typos in GLSL).
7. Attach the compiled result to your program.
8. Delete the now-unnecessary shader object (the program keeps a copy).

---

### The `link()` Function (Linking Vertex + Fragment Together)

```cpp
bool our::ShaderProgram::link() const {
    // Tell OpenGL to link all attached shaders into one program
    glLinkProgram(program);

    // Check for linking errors
    std::string error = checkForLinkingErrors(program);
    if (!error.empty()) {
        std::cerr << "Shader linking error: " << error << std::endl;
        return false;
    }
    return true;
}
```

**Why is linking needed?**
- The vertex shader outputs a varying (e.g., `vs_out.color`).
- The fragment shader reads that same varying (e.g., `fs_in.color`).
- Linking checks that these "connections" between vertex and fragment shaders are valid.

---

### Error Checking Helpers

```cpp
std::string checkForShaderCompilationErrors(GLuint shader){
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status); // Ask: did it compile?
    if (!status) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length); // How long is the error message?
        char *logStr = new char[length];
        glGetShaderInfoLog(shader, length, nullptr, logStr); // Get the error message
        std::string errorLog(logStr);
        delete[] logStr;
        return errorLog; // Return the error text
    }
    return std::string(); // Empty string = no error
}
```

This pattern is repeated for linking errors. OpenGL doesn't throw exceptions — you have to ask it explicitly if something went wrong.

---

## 📖 Understanding the Shaders

### `triangle.vert` — Drawing a Triangle

```glsl
#version 330

// Output to fragment shader: the color for this vertex
out Varyings {
    vec3 color;
} vs_out;

// Uniforms (sent from C++ code)
uniform vec2 translation = vec2(0.0, 0.0); // Default: no movement
uniform vec2 scale = vec2(1.0, 1.0);       // Default: no scaling

// Hardcoded triangle corners (in NDC: Normalized Device Coordinates)
const vec3 POSITIONS[3] = vec3[](
    vec3(-0.5, -0.5, 0.0),  // Bottom-left  → RED
    vec3( 0.5, -0.5, 0.0),  // Bottom-right → GREEN
    vec3( 0.0,  0.5, 0.0)   // Top-center   → BLUE
);

// Hardcoded colors per vertex
const vec3 COLORS[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // Red
    vec3(0.0, 1.0, 0.0),  // Green
    vec3(0.0, 0.0, 1.0)   // Blue
);

void main(){
    int index = gl_VertexID;  // OpenGL tells us which vertex (0, 1, or 2) we are
    vec3 v = POSITIONS[index];
    // Apply scale and translation:
    vec3 transformed = vec3(scale * v.xy + translation, v.z);
    gl_Position = vec4(transformed, 1.0);  // Final screen position
    vs_out.color = COLORS[index];          // Pass color to fragment shader
}
```

**What is NDC (Normalized Device Coordinates)?**
OpenGL maps the screen to a range of `[-1, 1]` in X and Y:
- `(-1, -1)` = bottom-left corner of the window
- `(+1, +1)` = top-right corner of the window
- `(0, 0)` = center of the screen

**What is `gl_VertexID`?**
A built-in variable. When OpenGL calls the vertex shader, it tells it which vertex it is currently processing via `gl_VertexID`. So vertex 0 gets `gl_VertexID = 0`, vertex 1 gets `gl_VertexID = 1`, etc.

**What is `gl_Position`?**
A built-in output variable. You **must** write to it. It tells OpenGL where this vertex is on screen.

---

### `color-mixer.frag` — Mixing Color Channels

```glsl
#version 330 core

// Input from vertex shader (interpolated across the triangle)
in Varyings {
    vec3 color;
} fs_in;

out vec4 frag_color;  // The final pixel color we output

// These are the "rows" of a color mixing matrix
uniform vec4 red   = vec4(1.0, 0.0, 0.0, 0.0); // Default: copy red as-is
uniform vec4 green = vec4(0.0, 1.0, 0.0, 0.0); // Default: copy green as-is
uniform vec4 blue  = vec4(0.0, 0.0, 1.0, 0.0); // Default: copy blue as-is

void main(){
    vec4 color = vec4(fs_in.color, 1.0); // Add alpha=1 to the interpolated color

    // dot(red, color) = red.r*color.r + red.g*color.g + red.b*color.b + red.a*color.a
    frag_color.r = dot(red, color);
    frag_color.g = dot(green, color);
    frag_color.b = dot(blue, color);
    frag_color.a = 1.0;
}
```

**What is interpolation?**
Between the 3 triangle vertices (which have different colors: red, green, blue), OpenGL automatically blends the color smoothly. A pixel exactly between vertex 0 and vertex 1 will get 50% red + 50% green. This is called **interpolation**.

**What is the `dot` product here?**
It's just a weighted sum:
```
dot(vec4(a,b,c,d), vec4(x,y,z,w)) = a*x + b*y + c*z + d*w
```

This lets you mix channels. If `red = vec4(0, 1, 0, 0)`, then `frag_color.r = green_channel` — the red output now shows what the green input was!

---

### `checkerboard.frag` — Drawing a Checkerboard

```glsl
#version 330 core

out vec4 frag_color;

uniform int size = 32;        // Each tile is 32×32 pixels
uniform vec3 colors[2];       // Color 0 and Color 1

void main(){
    // gl_FragCoord.x/y = pixel position in screen space (pixels from bottom-left)
    int tile_x = int(gl_FragCoord.x / size);  // Which tile column is this pixel in?
    int tile_y = int(gl_FragCoord.y / size);  // Which tile row is this pixel in?
    int pattern = (tile_x + tile_y) % 2;      // 0 or 1, alternating like a checkerboard
    frag_color = vec4(colors[pattern], 1.0);  // Pick color 0 or color 1
}
```

**How does the checkerboard pattern work?**
- If tile_x=0, tile_y=0: (0+0)%2 = 0 → color[0]
- If tile_x=1, tile_y=0: (1+0)%2 = 1 → color[1]
- If tile_x=0, tile_y=1: (0+1)%2 = 1 → color[1]
- If tile_x=1, tile_y=1: (1+1)%2 = 0 → color[0]

This creates the classic checkerboard alternating pattern!

**What is `gl_FragCoord`?**
A built-in GLSL variable that gives the current pixel's position in screen space (in pixels). Unlike `gl_Position` which uses NDC, `gl_FragCoord` uses actual pixel coordinates.

---

## 🔄 The Full Pipeline (How It All Connects)

```
C++ Code                        GPU
─────────────────────────────────────────────────────
1. Load triangle.vert text
2. glCreateShader(GL_VERTEX_SHADER)
3. glShaderSource(...)
4. glCompileShader(...)          → Compiles on GPU
5. Load color-mixer.frag text
6. glCreateShader(GL_FRAGMENT_SHADER)
7. glShaderSource(...)
8. glCompileShader(...)          → Compiles on GPU
9. glCreateProgram()
10. glAttachShader(both)
11. glLinkProgram()              → Links together
12. glUseProgram()               → Activate!
13. Draw call (glDrawArrays)     → Shader runs for 
                                   every vertex/pixel
```

---

## ✅ Key Things to Remember

| Concept | Explanation |
|---|---|
| `glCreateShader(type)` | Creates an empty shader slot. `type` = `GL_VERTEX_SHADER` or `GL_FRAGMENT_SHADER` |
| `glShaderSource(...)` | Gives the GLSL source code to OpenGL |
| `glCompileShader(shader)` | Compiles the GLSL code |
| `glCreateProgram()` | Creates an empty program object |
| `glAttachShader(program, shader)` | Connects a compiled shader to the program |
| `glLinkProgram(program)` | Links vertex + fragment shader together |
| `glUseProgram(program)` | Activates the program for drawing |
| `glUniform*` | Sends data from CPU to GPU shader |
| `gl_VertexID` | Built-in: which vertex is currently being processed |
| `gl_Position` | Built-in output: where the vertex appears on screen (required!) |
| `gl_FragCoord` | Built-in: current pixel's position in screen pixels |
| Varying | Data passed from vertex shader → fragment shader, interpolated automatically |
| Uniform | Data passed from CPU → shader (same value for every vertex/pixel) |

---

## 🧪 How to Test

Run the shader test:
```powershell
./bin/GAME_APPLICATION.exe -c='config/shader-test/test-0.jsonc' -f=10
```

Compare your output in `screenshots/` with the expected output in `expected/shader-test/`.
