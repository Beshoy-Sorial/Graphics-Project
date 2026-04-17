#include "shader.hpp"

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

//Forward definition for error checking functions
std::string checkForShaderCompilationErrors(GLuint shader);
std::string checkForLinkingErrors(GLuint program);

// Extract the directory portion of a file path (e.g. "assets/shaders/lit.frag" -> "assets/shaders")
static std::string getDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    return (pos == std::string::npos) ? "." : filepath.substr(0, pos);
}

// Recursively resolve #include "file" directives in GLSL source.
// Paths in #include are resolved relative to the including file's directory.
static std::string processIncludes(const std::string& source, const std::string& directory) {
    std::string result;
    std::istringstream stream(source);
    std::string line;
    while(std::getline(stream, line)) {
        // Find #include (allow leading whitespace)
        size_t nonSpace = line.find_first_not_of(" \t");
        if(nonSpace != std::string::npos && line.substr(nonSpace, 8) == "#include") {
            size_t start = line.find('"', nonSpace) + 1;
            size_t end   = line.rfind('"');
            if(start < end) {
                std::string includePath = directory + "/" + line.substr(start, end - start);
                std::ifstream inc(includePath);
                if(inc) {
                    std::string content((std::istreambuf_iterator<char>(inc)),
                                         std::istreambuf_iterator<char>());
                    result += processIncludes(content, directory) + "\n";
                } else {
                    std::cerr << "ERROR: Couldn't open include: " << includePath << std::endl;
                }
                continue;
            }
        }
        result += line + "\n";
    }
    return result;
}

bool our::ShaderProgram::attach(const std::string &filename, GLenum type) const {
    // Here, we open the file and read a string from it containing the GLSL code of our shader
    std::ifstream file(filename);
    if(!file){
        std::cerr << "ERROR: Couldn't open shader file: " << filename << std::endl;
        return false;
    }
    std::string sourceString = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    file.close();

    // Resolve any #include directives before passing to the GL compiler
    sourceString = processIncludes(sourceString, getDirectory(filename));

    const char* sourceCStr = sourceString.c_str();

    // Create the shader object
    GLuint shader = glCreateShader(type);
    // Set the source code
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    // Compile the shader
    glCompileShader(shader);

    // Check for compilation errors
    std::string error = checkForShaderCompilationErrors(shader);
    if (!error.empty()) {
        std::cerr << "Shader compilation error in " << filename << ": " << error << std::endl;
        glDeleteShader(shader);
        return false;
    }

    // Attach the shader to the program
    glAttachShader(program, shader);
    // Delete the shader object as it's no longer needed after attachment
    glDeleteShader(shader);

    // We return true if the compilation and attachment succeeded
    return true;
}



bool our::ShaderProgram::link() const {
    // Link the shader program
    glLinkProgram(program);

    // Check for linking errors
    std::string error = checkForLinkingErrors(program);
    if (!error.empty()) {
        std::cerr << "Shader linking error: " << error << std::endl;
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////
// Function to check for compilation and linking error in shaders //
////////////////////////////////////////////////////////////////////

std::string checkForShaderCompilationErrors(GLuint shader){
     //Check and return any error in the compilation process
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *logStr = new char[length];
        glGetShaderInfoLog(shader, length, nullptr, logStr);
        std::string errorLog(logStr);
        delete[] logStr;
        return errorLog;
    }
    return std::string();
}

std::string checkForLinkingErrors(GLuint program){
     //Check and return any error in the linking process
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *logStr = new char[length];
        glGetProgramInfoLog(program, length, nullptr, logStr);
        std::string error(logStr);
        delete[] logStr;
        return error;
    }
    return std::string();
}