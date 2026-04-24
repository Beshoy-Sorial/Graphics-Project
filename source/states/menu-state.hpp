#pragma once

#include <application.hpp>
#include <material/material.hpp>
#include <mesh/mesh.hpp>
#include <shader/shader.hpp>
#include <texture/texture-utils.hpp>
#include <texture/texture2d.hpp>

#include <array>
#include <functional>
#include "../common/tournament-manager.hpp"
#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>

namespace our
{
  extern int g_WeatherMode;
}

// This struct is used to store the location and size of a button and the code
// it should execute when clicked
struct Button
{
  // The position (of the top-left corner) of the button and its size in pixels
  glm::vec2 position, size;
  // The function that should be excuted when the button is clicked. It takes no
  // arguments and returns nothing.
  std::function<void()> action;

  // This function returns true if the given vector v is inside the button.
  // Otherwise, false is returned. This is used to check if the mouse is
  // hovering over the button.
  bool isInside(const glm::vec2 &v) const
  {
    return position.x <= v.x && position.y <= v.y &&
           v.x <= position.x + size.x && v.y <= position.y + size.y;
  }

  // This function returns the local to world matrix to transform a rectangle of
  // size 1x1 (and whose top-left corner is at the origin) to be the button.
  glm::mat4 getLocalToWorld() const
  {
    return glm::translate(glm::mat4(1.0f),
                          glm::vec3(position.x, position.y, 0.0f)) *
           glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));
  }
};

// This state shows how to use some of the abstractions we created to make a
// menu.
class Menustate : public our::State
{

  // A meterial holding the menu shader and the menu texture to draw
  our::TexturedMaterial *menuMaterial;
  // A material to be used to highlight hovered buttons (we will use blending to
  // create a negative effect).
  our::TintedMaterial *highlightMaterial;
  // A rectangle mesh on which the menu material will be drawn
  our::Mesh *rectangle;
  // A variable to record the time since the state is entered (it will be used
  // for the fading effect).
  float time;
  // An array of the button that we can interact with
  std::array<Button, 2> buttons;

  our::World world;
  our::ForwardRenderer renderer;

  void onInitialize() override
  {
    // First, we create a material for the menu's background
    menuMaterial = new our::TexturedMaterial();
    // Here, we load the shader that will be used to draw the background
    menuMaterial->shader = new our::ShaderProgram();
    menuMaterial->shader->attach("assets/shaders/textured.vert",
                                 GL_VERTEX_SHADER);
    menuMaterial->shader->attach("assets/shaders/textured.frag",
                                 GL_FRAGMENT_SHADER);
    menuMaterial->shader->link();
    // Menu background: Solid Red
    menuMaterial->tint = glm::vec4(0.8f, 0.1f, 0.1f, 1.0f);
    menuMaterial->alphaThreshold = 0.0f;
    menuMaterial->texture = nullptr; // No texture needed

    // Second, we create a material to highlight the hovered buttons
    highlightMaterial = new our::TintedMaterial();
    // Since the highlight is not textured, we used the tinted material shaders
    highlightMaterial->shader = new our::ShaderProgram();
    highlightMaterial->shader->attach("assets/shaders/tinted.vert",
                                      GL_VERTEX_SHADER);
    highlightMaterial->shader->attach("assets/shaders/tinted.frag",
                                      GL_FRAGMENT_SHADER);
    highlightMaterial->shader->link();
    // The tint is white since we will subtract the background color from it to
    // create a negative effect.
    highlightMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    // To create a negative effect, we enable blending, set the equation to be
    // subtract, and set the factors to be one for both the source and the
    // destination.
    highlightMaterial->pipelineState.blending.enabled = true;
    highlightMaterial->pipelineState.blending.equation = GL_FUNC_SUBTRACT;
    highlightMaterial->pipelineState.blending.sourceFactor = GL_ONE;
    highlightMaterial->pipelineState.blending.destinationFactor = GL_ONE;

    // Then we create a rectangle whose top-left corner is at the origin and its
    // size is 1x1. Note that the texture coordinates at the origin is
    // (0.0, 1.0) since we will use the projection matrix to make the origin at
    // the the top-left corner of the screen.
    rectangle = new our::Mesh(
        {
            {{0.0f, 0.0f, 0.0f},
             {255, 255, 255, 255},
             {0.0f, 1.0f},
             {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f},
             {255, 255, 255, 255},
             {1.0f, 1.0f},
             {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0f},
             {255, 255, 255, 255},
             {1.0f, 0.0f},
             {0.0f, 0.0f, 1.0f}},
            {{0.0f, 1.0f, 0.0f},
             {255, 255, 255, 255},
             {0.0f, 0.0f},
             {0.0f, 0.0f, 1.0f}},
        },
        {
            0,
            1,
            2,
            2,
            3,
            0,
        });

    // Reset the time elapsed since the state is entered.
    time = 0;

    // Fill the positions, sizes and actions for the menu buttons
    // Note that we use lambda expressions to set the actions of the buttons.
    // A lambda expression consists of 3 parts:
    // - The capture list [] which is the variables that the lambda should
    // remember because it will use them during execution.
    //      We store [this] in the capture list since we will use it in the
    //      action.
    // - The argument list () which is the arguments that the lambda should
    // receive when it is called.
    //      We leave it empty since button actions receive no input.
    // - The body {} which contains the code to be executed.
    buttons[0].position = {-1000.0f, -1000.0f}; // Move off screen
    buttons[0].size = {0.0f, 0.0f};
    buttons[0].action = []() {};

    buttons[1].position = {-1000.0f, -1000.0f};
    buttons[1].size = {0.0f, 0.0f};
    buttons[1].action = []() {};

    auto &config = getApp()->getConfig()["scene"];
    if (config.contains("assets"))
    {
      our::deserializeAllAssets(config["assets"]);
    }
    if (config.contains("world"))
    {
      world.deserialize(config["world"]);
    }

    auto size = getApp()->getFrameBufferSize();
    renderer.initialize(size, config["renderer"]);

    // Ensure mouse is unlocked for character selection
    getApp()->getMouse().unlockMouse(getApp()->getWindow());
  }

  void onDraw(double deltaTime) override
  {
    // Get a reference to the keyboard object
    auto &keyboard = getApp()->getKeyboard();

    if (keyboard.justPressed(GLFW_KEY_SPACE))
    {
      // If the space key is pressed in this frame, go to the bracket state
      getApp()->changeState("bracket");
    }
    else if (keyboard.justPressed(GLFW_KEY_ESCAPE))
    {
      // If the escape key is pressed in this frame, exit the game
      getApp()->close();
    }

    // Get a reference to the mouse object and get the current mouse position
    auto &mouse = getApp()->getMouse();
    glm::vec2 mousePosition = mouse.getMousePosition();

    // If the mouse left-button is just pressed, check if the mouse was inside
    // any menu button. If it was inside a menu button, run the action of the
    // button.
    if (mouse.justPressed(0))
    {
      for (auto &button : buttons)
      {
        if (button.isInside(mousePosition))
          button.action();
      }
    }

    // Get the framebuffer size to set the viewport and the create the
    // projection matrix.
    glm::ivec2 size = getApp()->getFrameBufferSize();
    // Make sure the viewport covers the whole size of the framebuffer.
    glViewport(0, 0, size.x, size.y);

    // The view matrix is an identity (there is no camera that moves around).
    // The projection matrix applys an orthographic projection whose size is the
    // framebuffer size in pixels so that the we can define our object locations
    // and sizes in pixels. Note that the top is at 0.0 and the bottom is at the
    // framebuffer height. This allows us to consider the top-left corner of the
    // window to be the origin which makes dealing with the mouse input easier.
    glm::mat4 VP =
        glm::ortho(0.0f, (float)size.x, (float)size.y, 0.0f, 1.0f, -1.0f);
    // The local to world (model) matrix of the background which is just a
    // scaling matrix to make the menu cover the whole window. Note that we
    // defind the scale in pixels.
    glm::mat4 M = glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));

    // Render the 3D world as a background instead of a solid red clear
    renderer.render(&world);

    // Apply the fading effect logic if needed.
    time += (float)deltaTime;

    // For every button, check if the mouse is inside it. If the mouse is
    // inside, we draw the highlight rectangle over it.
    for (auto &button : buttons)
    {
      if (button.isInside(mousePosition))
      {
        highlightMaterial->setup();
        highlightMaterial->shader->set("transform",
                                       VP * button.getLocalToWorld());
        rectangle->draw();
      }
    }
  }

  void onImmediateGui() override
  {
    // Character Selection Window
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 560), ImGuiCond_Always);

    ImGui::Begin("Select Your Fighter", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse);

    auto &tm = our::TournamentManager::getInstance();
    const auto &chars = tm.characters;

    ImGui::Columns(4, "CharacterGrid", false);
    for (int i = 0; i < (int)chars.size(); ++i)
    {
      ImGui::PushID(i);

      // Button background color based on character signature color
      ImVec4 col = ImVec4(chars[i].buttonColor.r, chars[i].buttonColor.g,
                          chars[i].buttonColor.b, 1.0f);
      ImGui::PushStyleColor(ImGuiCol_Button, col);
      ImVec4 hoverCol = ImVec4(col.x * 1.2f, col.y * 1.2f, col.z * 1.2f, 1.0f);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverCol);

      // If this character is selected, show a border or highlight
      bool isSelected = (tm.selectedCharacterIndex == i);
      if (isSelected)
      {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1));
      }

      if (ImGui::Button(chars[i].name.c_str(), ImVec2(130, 130)))
      {
        tm.selectedCharacterIndex = i;
      }

      if (isSelected)
      {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
      }

      ImGui::PopStyleColor(2);
      ImGui::PopID();
      ImGui::NextColumn();
    }
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const auto &selected = tm.getSelectedCharacter();
    ImGui::Text("Selected: %s", selected.name.c_str());
    ImGui::Text("Stats: Strength %.1fx, Speed %.1fx", selected.strength,
                selected.speed);

    ImGui::Spacing();
    ImGui::Text("Select Weather Condition:");
    const char *weatherItems[] = {"Sunny", "Rainy", "Snowy"};
    int weatherIndex = our::g_WeatherMode;
    if (ImGui::Combo("##weather", &weatherIndex, weatherItems, IM_ARRAYSIZE(weatherItems)))
    {
      our::g_WeatherMode = weatherIndex;
    }

    ImGui::Spacing();
    ImGui::Text("Select Difficulty:");
    const char *difficultyItems[] = {"Easy", "Medium", "Hard", "Difficult"};
    int difficultyIndex = static_cast<int>(tm.selectedDifficulty);
    if (ImGui::Combo("##difficulty", &difficultyIndex, difficultyItems, IM_ARRAYSIZE(difficultyItems)))
    {
      tm.selectedDifficulty = static_cast<our::DifficultyLevel>(difficultyIndex);
    }

    ImGui::Spacing();
    if (ImGui::Button("START TOURNAMENT", ImVec2(-1, 50)))
    {
      tm.reset(); // Clear old progress for a new run
      this->getApp()->changeState("bracket");
    }

    ImGui::End();
  }

  void onDestroy() override
  {
    // Destroy 3D scene resources manually loaded for menu preview
    renderer.destroy();
    world.clear();
    our::clearAllAssets();

    // Delete all the allocated resources
    delete rectangle;
    if (menuMaterial->texture)
      delete menuMaterial->texture;
    if (menuMaterial->sampler)
      delete menuMaterial->sampler;
    delete menuMaterial->shader;
    delete menuMaterial;
    delete highlightMaterial->shader;
    delete highlightMaterial;
  }
};