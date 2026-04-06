#include "shader.hpp"

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

//Forward definition for error checking functions
std::string checkForShaderCompilationErrors(GLuint shader);
std::string checkForLinkingErrors(GLuint program);

bool our::ShaderProgram::attach(const std::string &filename, GLenum type) const {
    // Here, we open the file and read a string from it containing the GLSL code of our shader
    std::ifstream file(filename);
    if(!file){
        std::cerr << "ERROR: Couldn't open shader file: " << filename << std::endl;
        return false;
    }
    std::string sourceString = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    const char* sourceCStr = sourceString.c_str();
    file.close();

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