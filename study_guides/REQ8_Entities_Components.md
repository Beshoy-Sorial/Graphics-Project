# Requirement 8: Entities and Components (ECS) — Complete Study Guide

---

## 🧠 What is the Entity-Component-System (ECS) Pattern?

ECS is a way to organize a game or 3D application by separating **what exists** from **what it does** from **how it behaves**:

| ECS Part | Role | Analogy |
|---|---|---|
| **Entity** | A "thing" in the world — just a container | A person |
| **Component** | Data attached to an entity — defines *what it is* | Attributes: height, job, name... |
| **System** | Logic that processes entities with certain components | HR system processes all employees |

**Example:**
- Entity: "Player"
  - CameraComponent → this entity is the camera point of view
  - MeshRendererComponent → this entity has a 3D model to draw
- Entity: "Tree01"
  - MeshRendererComponent → has a 3D model
  - Transform → has position, rotation, scale

A **World** is just an organized collection of all entities.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/ecs/entity.hpp` | Entity class: component management, transform, parent |
| `source/common/ecs/entity.cpp` | `getLocalToWorldMatrix()` and `deserialize()` |
| `source/common/ecs/world.hpp` | World class: holds all entities, add/remove |
| `source/common/ecs/world.cpp` | `deserialize()` for loading entities from JSON |
| `source/common/components/camera.hpp` & `.cpp` | Camera component: view + projection matrices |
| `source/common/components/mesh-renderer.hpp` & `.cpp` | MeshRenderer component: mesh + material |
| `source/common/components/component-deserializer.hpp` | Creates the right component type from JSON |

---

## 📖 Understanding `entity.hpp` — The Entity Class

```cpp
class Entity {
    World* world;                     // Which world owns this entity
    std::list<Component*> components; // List of attached components
    friend World;                     // Only World can create Entities!
    Entity() = default;               // Private constructor

public:
    std::string name;         // Human-readable name (e.g., "Player", "Tree01")
    Entity* parent;           // Parent entity (or nullptr if root)
    Transform localTransform; // This entity's position/rotation/scale relative to parent

    World* getWorld() const { return world; }

    glm::mat4 getLocalToWorldMatrix() const;  // Compute world-space transform
    void deserialize(const nlohmann::json&);  // Load from JSON config
```

---

### Component Management Template Methods

These are **template** functions — they work for any component type `T`:

#### `addComponent<T>()` — Add a New Component

```cpp
template<typename T>
T* addComponent(){
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    T* component = new T();       // Create a new component of type T
    component->owner = this;      // Tell the component who owns it
    components.push_back(component); // Add to the list
    return component;             // Return a pointer to it
}
```

**Usage:**
```cpp
entity->addComponent<CameraComponent>();        // Add a camera
entity->addComponent<MeshRendererComponent>();  // Add a mesh renderer
```

#### `getComponent<T>()` — Find a Component by Type

```cpp
template<typename T>
T* getComponent(){
    for(auto component : components){
        if(auto casted = dynamic_cast<T*>(component)){
            return casted;  // Found it!
        }
    }
    return nullptr;  // Not found
}
```

`dynamic_cast<T*>(component)` tries to cast a `Component*` to `T*`. If `component` is actually of type `T` (or inherits from it), it succeeds; otherwise it returns `nullptr`.

**Usage:**
```cpp
auto camera = entity->getComponent<CameraComponent>();
if(camera) { /* use it */ }
```

#### `deleteComponent<T>()` — Remove and Delete a Component

```cpp
template<typename T>
void deleteComponent(){
    for(auto it = components.begin(); it != components.end(); ++it){
        if(dynamic_cast<T*>(*it)){
            delete *it;           // Free the memory
            components.erase(it); // Remove from list
            return;
        }
    }
}
```

#### The Destructor — Entity Owns Its Components

```cpp
~Entity(){
    for(auto component : components){
        delete component;  // Entity is responsible for cleaning up its components
    }
}
```

---

## 📖 Understanding `entity.cpp` — The Implementation

### `getLocalToWorldMatrix()` — The Key Function

```cpp
glm::mat4 Entity::getLocalToWorldMatrix() const {
    glm::mat4 localToParent = localTransform.toMat4();  // This entity's local transform
    if(parent){
        // Recursively get parent's world transform and combine
        return parent->getLocalToWorldMatrix() * localToParent;
    }
    return localToParent;  // Root entity: local IS world
}
```

**Why recursive?**

Entities form a **hierarchy** (tree structure):
```
World
├── Car entity (position: (5, 0, 0), no parent)
│   ├── Wheel entity (position: (1, 0, 0), parent: Car)
│   └── Door entity (position: (0.5, 0, 0), parent: Car)
```

The Wheel's world position = Car's world position + Wheel's local position = (5,0,0) + (1,0,0) = (6,0,0).

In matrix form:
```
wheelWorldMatrix = carWorldMatrix × wheelLocalMatrix
```

This is recursive because the Car itself might have a parent, and that parent might have a parent, etc.

**Formula:**
```
LocalToWorld = Parent.LocalToWorld × This.LocalTransform.toMat4()
```

If no parent: `LocalToWorld = This.LocalTransform.toMat4()`

---

### `deserialize()` — Loading Entity from JSON

```cpp
void Entity::deserialize(const nlohmann::json& data){
    if(!data.is_object()) return;
    name = data.value("name", name);          // Read name
    localTransform.deserialize(data);          // Read transform (position, rotation, scale)
    if(data.contains("components")){
        const auto& components = data["components"];
        if(components.is_array()){
            for(auto& component: components){
                deserializeComponent(component, this);  // Create + load each component
            }
        }
    }
}
```

**Example entity JSON:**
```json
{
    "name": "Camera",
    "position": [0, 2, 5],
    "rotation": [-15, 0, 0],
    "components": [
        {
            "type": "Camera",
            "cameraType": "perspective",
            "fovY": 60,
            "near": 0.01,
            "far": 100
        }
    ]
}
```

---

## 📖 Understanding `world.hpp` — The World Class

```cpp
class World {
    std::unordered_set<Entity*> entities;          // All entities
    std::unordered_set<Entity*> markedForRemoval;  // Entities to delete later
public:
    Entity* add();                            // Create and add a new entity
    const std::unordered_set<Entity*>& getEntities();  // Get all entities
    void markForRemoval(Entity* entity);      // Schedule for deletion
    void deleteMarkedEntities();              // Actually delete them
    void clear();                             // Delete ALL entities
    void deserialize(const nlohmann::json& data, Entity* parent = nullptr);
};
```

**Why `markedForRemoval` instead of deleting immediately?**
During a game loop, you might destroy an enemy while iterating over all entities. If you delete it immediately, the iterator breaks. Marking for removal is safe — you delete them all after iteration ends.

---

### `World::add()` — Create an Entity

```cpp
Entity* add() {
    Entity* entity = new Entity();    // Create entity
    entity->world = this;             // Set its world
    entity->parent = nullptr;         // No parent by default
    entities.insert(entity);          // Add to the set
    return entity;
}
```

Note: only `World` can create entities (entity's constructor is private and World is a `friend`).

---

### `World::deserialize()` — Loading a Scene from JSON

```cpp
void World::deserialize(const nlohmann::json& data, Entity* parent){
    if(!data.is_array()) return;
    for(const auto& entityData : data){
        Entity* entity = add();          // Create new entity
        entity->parent = parent;         // Set its parent (or nullptr)
        entity->deserialize(entityData); // Load its data

        // If it has children, recursively load them
        if(entityData.contains("children")){
            deserialize(entityData["children"], entity);
        }
    }
}
```

**Example world JSON:**
```json
[
    {
        "name": "Root",
        "children": [
            { "name": "Camera", "components": [ { "type": "Camera" } ] },
            { "name": "Cube", "components": [ { "type": "Mesh Renderer", "mesh": "cube" } ] }
        ]
    }
]
```

---

## 📖 Understanding `CameraComponent`

### What it stores

```cpp
class CameraComponent : public Component {
public:
    CameraType cameraType;  // PERSPECTIVE or ORTHOGRAPHIC
    float near;             // Near clipping plane distance
    float far;              // Far clipping plane distance
    float fovY;             // Field of view (Y axis), in radians
    float orthoHeight;      // Height of the orthographic view
};
```

**Note:** The camera **position and direction** are NOT stored here — they come from the owning entity's transform!

### `getViewMatrix()` — Where the Camera Is

```cpp
glm::mat4 CameraComponent::getViewMatrix() const {
    auto owner = getOwner();
    auto M = owner->getLocalToWorldMatrix();   // Entity's world transform

    // Extract camera position: transform (0,0,0) to world space
    glm::vec3 eye = glm::vec3(M * glm::vec4(0, 0, 0, 1));

    // Extract look direction: where is (0,0,-1) in world space?
    // In OpenGL camera space, looking forward = -Z direction
    glm::vec3 center = glm::vec3(M * glm::vec4(0, 0, -1, 1));

    // Extract up direction: where is (0,1,0) in world space?
    // w=0 for direction vectors (translation doesn't apply)
    glm::vec3 up = glm::vec3(M * glm::vec4(0, 1, 0, 0));

    return glm::lookAt(eye, center, up);  // Build the view matrix
}
```

**`glm::lookAt(eye, center, up)`:**
- `eye` = camera position.
- `center` = point the camera is looking at.
- `up` = which direction is "up" for the camera.

This builds a matrix that transforms world space into camera space.

### `getProjectionMatrix()` — Perspective vs Orthographic

```cpp
glm::mat4 CameraComponent::getProjectionMatrix(glm::ivec2 viewportSize) const {
    float aspectRatio = float(viewportSize.x) / float(viewportSize.y);

    if(cameraType == CameraType::ORTHOGRAPHIC){
        float halfHeight = orthoHeight * 0.5f;
        float halfWidth = halfHeight * aspectRatio;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, near, far);
    }
    // Perspective: objects appear smaller when far away
    return glm::perspective(fovY, aspectRatio, near, far);
}
```

**Perspective:** Objects at distance look smaller (like real life). Defined by field of view angle.

**Orthographic:** No perspective — an object's size on screen doesn't change with distance. Used in 2D games and CAD software.

---

## 📖 Understanding `MeshRendererComponent`

```cpp
class MeshRendererComponent : public Component {
public:
    Mesh* mesh;         // The 3D shape to draw
    Material* material; // How to draw it (shader, textures, etc.)

    static std::string getID() { return "Mesh Renderer"; }
    void deserialize(const nlohmann::json& data) override;
};
```

Very simple: it just pairs a mesh with a material. The renderer system (Requirement 9) will loop over all entities with this component and draw them.

---

## 📖 Understanding `component-deserializer.hpp`

```cpp
inline void deserializeComponent(const nlohmann::json& data, Entity* entity){
    std::string type = data.value("type", "");  // e.g., "Camera", "Mesh Renderer"
    Component* component = nullptr;

    if(type == CameraComponent::getID())               // "Camera"
        component = entity->addComponent<CameraComponent>();
    else if(type == MeshRendererComponent::getID())    // "Mesh Renderer"
        component = entity->addComponent<MeshRendererComponent>();
    else if (type == FreeCameraControllerComponent::getID())
        component = entity->addComponent<FreeCameraControllerComponent>();
    else if (type == MovementComponent::getID())
        component = entity->addComponent<MovementComponent>();

    if(component) component->deserialize(data);  // Load its specific data
}
```

This is a **factory + dispatcher**: reads the `"type"` string from JSON, creates the right component class, and immediately loads its data.

---

## 🗺️ Entity Hierarchy Example

```
World
├── Entity "Player" (position: 0,0,0)
│   ├── CameraComponent (fovY: 60°)
│   └── MeshRendererComponent (mesh: "character", material: "soldier-skin")
│
├── Entity "Car" (position: 10, 0, 0)
│   └── MeshRendererComponent (mesh: "car", material: "red-paint")
│       └── Entity "Wheel_FL" (position: 1, 0, 1, parent: Car)
│           └── MeshRendererComponent (mesh: "wheel")
│
└── Entity "Sun" (position: 0, 100, 0)
    └── MeshRendererComponent (mesh: "sphere", material: "emissive")
```

When we render "Wheel_FL":
- `getLocalToWorldMatrix()` → `Car.WorldMatrix × Wheel.LocalMatrix`
- Result: wheel appears at (11, 0, 1) in world space

---

## ✅ Key Things to Remember

| Concept | Details |
|---|---|
| Entity | Container for components + transform + parent pointer |
| World | Set of all entities; only World can create entities |
| `addComponent<T>()` | Adds a new component of type T to the entity |
| `getComponent<T>()` | Finds the first component of type T on the entity |
| `getLocalToWorldMatrix()` | Recursively combines transforms up the parent chain |
| `dynamic_cast<T*>(ptr)` | Safe runtime type check (returns nullptr if wrong type) |
| `friend World` | Allows World to access Entity's private constructor |
| `CameraComponent` | Stores camera settings; position comes from entity transform |
| `MeshRendererComponent` | Pairs a mesh + material for rendering |
| `deserializeComponent()` | Factory function: reads "type" from JSON, creates correct component |
| Parent hierarchy | Child's world position = Parent.WorldMatrix × Child.LocalMatrix |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/entity-test/test-1.jsonc' -f=10
```

Compare output in `screenshots/entity-test/` with `expected/entity-test/`.
