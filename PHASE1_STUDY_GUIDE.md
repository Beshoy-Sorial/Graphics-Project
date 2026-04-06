# Phase 1 Study Guide

This file summarizes what was implemented for Requirements 1 to 11, why those choices make sense, and what alternative approaches could have been used.

The goal of this guide is revision for the discussion, not just a changelog. So each section explains:

1. What the requirement is trying to teach.
2. What was implemented in this project.
3. Why that implementation is reasonable.
4. What other valid options existed.

---

## Requirement 1: Shader Program

### Main idea

This requirement is about the programmable GPU pipeline:

- a vertex shader generates positions and varyings,
- a fragment shader computes pixel color,
- and a shader program links both stages together.

### What was implemented

Relevant files:

- `source/common/shader/shader.hpp`
- `source/common/shader/shader.cpp`
- `assets/shaders/triangle.vert`
- `assets/shaders/color-mixer.frag`
- `assets/shaders/checkerboard.frag`

The shader program class now supports:

- creating an OpenGL program,
- attaching vertex or fragment shaders from files,
- compiling and reporting errors,
- linking the final program,
- binding the program with `use()`,
- setting uniforms through helper overloads such as `set("name", value)`.

The shader tasks were completed as follows:

- `triangle.vert` builds a triangle from `gl_VertexID`, assigns one color per vertex, and applies `scale` and `translation`.
- `color-mixer.frag` uses `dot` with `vec4(fs_in.color, 1.0)` to mix RGB channels through uniform vectors.
- `checkerboard.frag` computes tile coordinates from `gl_FragCoord` and alternates between two colors.

### Why this design was picked

Using `gl_VertexID` in `triangle.vert` is simple and fast for a fixed triangle because:

- no vertex buffer is needed,
- positions and colors are hardcoded,
- the shader becomes a minimal teaching example.

Using `dot` in `color-mixer.frag` was the cleanest choice because the requirement explicitly wanted the linear channel mix in vector form. It is shorter, clearer, and maps well to GPU math.

### Other valid options

- The triangle could have been drawn from a VBO instead of `gl_VertexID`, but that would add boilerplate and distract from the shader lesson.
- The channel mixer could have written the full scalar expression manually, but that is longer and harder to read.
- The checkerboard could have used normalized screen coordinates instead of `gl_FragCoord`, but `gl_FragCoord` is the most direct way for pixel-space tiling.

---

## Requirement 2: Mesh

### Main idea

This requirement teaches how mesh data is stored on the GPU using:

- a vertex buffer,
- an element buffer,
- and a vertex array object.

### What was implemented

Relevant file:

- `source/common/mesh/mesh.hpp`

The mesh constructor now:

- creates `VAO`, `VBO`, and `EBO`,
- uploads vertex data to `GL_ARRAY_BUFFER`,
- uploads index data to `GL_ELEMENT_ARRAY_BUFFER`,
- defines vertex attribute layouts for position, color, texture coordinates, and normal,
- stores the number of elements for drawing.

The `draw()` function:

- binds the VAO,
- calls `glDrawElements(GL_TRIANGLES, ...)`.

The destructor:

- deletes buffers and the vertex array object.

### Why this design was picked

Using indexed drawing with an element buffer is the standard choice because:

- repeated vertices can be shared,
- memory usage is reduced,
- it matches how imported meshes are commonly represented.

Storing the attribute layout inside the VAO is also the natural OpenGL design because the VAO remembers how to interpret the vertex buffer.

### Other valid options

- The mesh could have used `glDrawArrays` instead of `glDrawElements`, but that would duplicate shared vertices.
- The engine could have split the mesh code into `.hpp` and `.cpp`, but keeping the implementation inline here is acceptable for a compact teaching project.

---

## Requirement 3: Transform

### Main idea

This requirement teaches how to move from human-editable transform data to matrix-based rendering.

The three editable pieces are:

- position,
- rotation,
- scale.

### What was implemented

Relevant files:

- `source/common/ecs/transform.hpp`
- `source/common/ecs/transform.cpp`
- `assets/shaders/transform-test.vert`

The `Transform` struct now:

- deserializes `position`, `rotation`, and `scale`,
- converts rotation from degrees to radians during deserialization,
- builds a matrix using translation, rotation, and scale.

The matrix is built in the order:

- scale,
- then rotation,
- then translation.

In code that becomes `translation * rotation * scale`, which is correct for column-vector OpenGL math.

The shader `transform-test.vert` now multiplies vertex positions by the `transform` uniform.

### Why this design was picked

Using a single 4x4 matrix is the standard rendering choice because the GPU vertex shader expects positions to be transformed efficiently in one step.

Using `glm::yawPitchRoll(rotation.y, rotation.x, rotation.z)` was a good match for the assignment wording because the project stores rotation as yaw, pitch, and roll components.

### Other valid options

- Rotation could have been stored using quaternions instead of Euler angles. That is usually better for interpolation and avoiding gimbal lock, but Euler angles are easier for beginners to type in JSON.
- The transform could have been built from separate matrices every frame or cached after deserialization. Caching is more efficient, but the current version is simpler and clear.

---

## Requirement 4: Pipeline State

### Main idea

This requirement teaches that shaders are not enough on their own. OpenGL also has fixed pipeline state such as:

- face culling,
- depth testing,
- blending,
- color masks,
- depth mask.

### What was implemented

Relevant files:

- `source/common/material/pipeline-state.hpp`
- `source/common/material/pipeline-state.cpp`

The `PipelineState` struct now supports:

- deserializing culling, depth test, and blending settings from JSON,
- mapping strings like `GL_LEQUAL` or `GL_SRC_ALPHA` to actual OpenGL enums,
- applying those options in `setup()`.

This allows each material to carry its own rendering state.

### Why this design was picked

Encapsulating pipeline settings into one struct is a strong design choice because:

- materials can describe how they want to be drawn,
- renderer code stays cleaner,
- JSON scenes can configure rendering behavior without hardcoding it.

### Other valid options

- Pipeline state could have been stored directly inside materials only, without a separate struct. That would work, but it would mix concerns and make state reuse harder.
- The engine could have tried to cache the previous OpenGL state and skip redundant calls. That can be faster in larger engines, but it adds complexity and was unnecessary here.

---

## Requirement 5: Texture

### Main idea

This requirement teaches how images become GPU textures and how fragment shaders sample them.

### What was implemented

Relevant files:

- `source/common/texture/texture2d.hpp`
- `source/common/texture/texture-utils.cpp`

The texture system now supports:

- creating a `Texture2D` OpenGL object,
- binding and unbinding it,
- loading images with `stb_image`,
- uploading them as `GL_RGBA8`,
- optionally generating mipmaps.

Later in Requirement 11, the same utility was extended with `texture_utils::empty(...)` to create render targets for framebuffers.

### Why this design was picked

Loading everything as RGBA is a practical teaching choice because:

- shaders can assume a consistent format,
- alpha support is always available,
- code stays simpler than handling many channel-count combinations.

### Other valid options

- The loader could have preserved original channel counts and uploaded `GL_RGB`, `GL_RG`, or `GL_RED` when appropriate. That is more memory-efficient in some cases, but more complicated.
- The project could have wrapped texture creation and loading into one larger asset class, but keeping the raw texture object and utility functions separate is clearer.

---

## Requirement 6: Sampler

### Main idea

This requirement teaches that texture image data and sampling behavior are different concepts.

The texture stores pixels.
The sampler defines how those pixels are read.

### What was implemented

Relevant files:

- `source/common/texture/sampler.hpp`
- `source/common/texture/sampler.cpp`

The sampler system now supports:

- creating an OpenGL sampler object,
- binding it to a texture unit,
- setting filtering options,
- setting wrapping options,
- setting anisotropy and border color,
- deserializing those values from JSON.

### Why this design was picked

Separating texture and sampler was a good choice because:

- one texture can be reused with different sampling behaviors,
- scene config can switch between smooth and pixelated sampling without duplicating texture data,
- it matches modern OpenGL usage nicely.

### Other valid options

- Sampler settings could have been stored directly on the texture object using `glTexParameteri`. That is valid and common in older examples, but separate sampler objects are cleaner and more flexible.

---

## Requirement 7: Material

### Main idea

This requirement combines everything learned so far into a material abstraction.

A material answers:

- which shader to use,
- which pipeline state to use,
- which uniform data to send,
- whether the object is transparent.

### What was implemented

Relevant files:

- `source/common/material/material.hpp`
- `source/common/material/material.cpp`
- `assets/shaders/tinted.vert`
- `assets/shaders/tinted.frag`
- `assets/shaders/textured.vert`
- `assets/shaders/textured.frag`

The base `Material` now:

- stores `pipelineState`,
- stores the `shader`,
- stores a `transparent` flag,
- applies state and binds the shader in `setup()`.

`TintedMaterial` extends `Material` by:

- storing a `tint`,
- sending `tint` to the shader in `setup()`.

`TexturedMaterial` extends `TintedMaterial` by:

- storing a `texture`,
- storing a `sampler`,
- storing `alphaThreshold`,
- binding texture unit 0,
- binding the sampler on unit 0,
- sending `tex` and `alphaThreshold` uniforms.

The shaders were completed so that:

- the vertex shaders multiply by `transform`,
- `tinted.frag` outputs `tint * vertexColor`,
- `textured.frag` outputs `tint * vertexColor * textureColor`,
- `textured.frag` discards fragments below `alphaThreshold`.

### Why this design was picked

This inheritance chain was a good fit for the assignment because:

- `TexturedMaterial` really is a tinted material plus texture-specific data,
- code reuse stays high,
- the renderer only needs to call `material->setup()`.

Using texture unit `0` is also a perfectly reasonable choice for this stage because each material only needs one texture.

### Other valid options

- A composition-based material system could store a list of uniforms or texture slots instead of using inheritance. That is more flexible in larger engines, but much more complex for this project.
- The engine could have used multiple texture units and more generic binding logic, but one unit keeps the teaching example focused.

---

## Requirement 8: Entities and Components

### Main idea

This requirement introduces an ECS-style scene structure:

- entities are containers,
- components hold data,
- systems do the work.

### What was implemented

Relevant files:

- `source/common/ecs/entity.hpp`
- `source/common/ecs/entity.cpp`
- `source/common/ecs/world.hpp`
- `source/common/ecs/world.cpp`
- `source/common/components/camera.hpp`
- `source/common/components/camera.cpp`
- `source/common/components/mesh-renderer.hpp`
- `source/common/components/mesh-renderer.cpp`
- `source/common/components/component-deserializer.hpp`
- `source/states/entity-test-state.hpp`

The ECS implementation now supports:

- creating entities through `World::add()`,
- storing components inside each entity,
- adding components with `addComponent<T>()`,
- finding them with `getComponent<T>()`,
- deleting them by type, index, or pointer,
- recursively deserializing worlds and child entities from JSON,
- computing local-to-world transforms through parent chains.

The `CameraComponent` now:

- deserializes camera settings,
- builds a view matrix using the owner entity transform,
- builds either a perspective or orthographic projection matrix.

The `MeshRendererComponent` now:

- looks up the mesh and material from the asset loader.

The component deserializer now knows how to create a mesh renderer component.

The entity test state now:

- finds the camera,
- computes `VP`,
- loops through entities,
- draws entities that have a mesh renderer.

### Why this design was picked

Keeping transform directly on the entity instead of as a separate component was explicitly aligned with the assignment and simplifies hierarchy handling.

Using recursion in `getLocalToWorldMatrix()` is easy to explain and correct for parent-child hierarchies.

Using `dynamic_cast` in `getComponent<T>()` is also a straightforward educational choice:

- easy to write,
- easy to understand,
- enough for a small engine.

### Other valid options

- A more advanced ECS would store components in separate contiguous arrays for cache efficiency. That is faster in large games, but much harder to teach and maintain.
- The view matrix could have been computed by directly inverting the entity world matrix instead of using `glm::lookAt`. Inversion is elegant, but `lookAt` makes the camera basis logic easier to discuss in an oral exam.

---

## Requirement 9: Forward Renderer System

### Main idea

This requirement moves rendering logic out of test states and into a reusable rendering system.

It also introduces transparent-object sorting.

### What was implemented

Relevant files:

- `source/common/systems/forward-renderer.hpp`
- `source/common/systems/forward-renderer.cpp`
- `source/states/renderer-test-state.hpp`

The forward renderer now:

- searches the world for one camera,
- gathers draw commands from all mesh renderers,
- separates opaque and transparent objects,
- computes camera forward direction from the camera entity transform,
- sorts transparent commands from far to near along that forward direction,
- computes `VP`,
- sets viewport and clear state,
- clears color and depth,
- draws opaque commands first,
- draws transparent commands last.

### Why this design was picked

Opaque objects should be drawn before transparent objects because:

- opaque objects write depth normally,
- transparent objects depend on correct background data behind them,
- drawing transparency last is the standard forward rendering approach.

Sorting transparent objects using distance along the camera forward direction was chosen because the requirement asked for it specifically. It is cheap and matches the assignment’s intended method.

### Other valid options

- Transparent objects could be sorted by full Euclidean distance from the camera. That is common, but the assignment explicitly wants distance projected onto camera forward.
- A renderer could also batch by material to reduce state changes, but that was not the focus here.

---

## Requirement 10: Sky Rendering

### Main idea

This requirement adds a sky sphere that always surrounds the camera and appears behind all objects.

### What was implemented

Relevant file:

- `source/common/systems/forward-renderer.cpp`

The sky path now:

- creates a sphere mesh,
- loads a sky texture,
- creates a textured material for the sky,
- uses a pipeline state with depth testing enabled and function `GL_LEQUAL`,
- enables face culling and culls front faces so the inside of the sphere is visible,
- places the sphere at the camera position every frame,
- multiplies by an extra matrix that forces sky depth to NDC `z = 1`,
- draws the sky after opaque objects and before transparent objects.

### Why this design was picked

Drawing the sky after opaque objects is a smart choice because:

- many pixels may already fail depth and not need sky shading,
- the sky should still appear behind everything else.

Using `GL_LEQUAL` is important because the sky is forced to the farthest depth, so it should still pass where depth is exactly at the far plane.

Using front-face culling is correct because the camera is inside the sphere and we want to see its interior faces.

### Other valid options

- A skybox cube could have been used instead of a sphere. Cubes are more common in some engines, but the assignment specifically asks for a sky sphere.
- The sky could have been rendered first with depth writes disabled. That can work, but drawing it later is usually more efficient.

---

## Requirement 11: Postprocessing

### Main idea

This requirement teaches rendering the scene into an off-screen framebuffer, then drawing that result to the screen with a fullscreen effect shader.

### What was implemented

Relevant files:

- `source/common/systems/forward-renderer.cpp`
- `source/common/texture/texture-utils.cpp`
- `assets/shaders/fullscreen.vert`
- `assets/shaders/postprocess/vignette.frag`
- `assets/shaders/postprocess/chromatic-aberration.frag`

The renderer now supports postprocessing by:

- creating a framebuffer,
- creating a color texture target with `GL_RGBA8`,
- creating a depth texture target with `GL_DEPTH_COMPONENT24`,
- attaching both textures to the framebuffer,
- rendering the whole scene into that framebuffer,
- returning to the default framebuffer,
- drawing a fullscreen triangle that samples the color target through a postprocess material.

The empty texture helper was updated so it correctly handles:

- color targets using `GL_RGBA8` with base format `GL_RGBA`,
- depth targets using `GL_DEPTH_COMPONENT24` with base format `GL_DEPTH_COMPONENT`.

The postprocess shaders were completed as follows:

- `vignette.frag` converts `tex_coord` from `[0,1]` to NDC `[-1,1]`, computes squared distance from the center, and darkens corners by dividing color by `1 + dot(ndc, ndc)`.
- `chromatic-aberration.frag` samples red slightly to the left, blue slightly to the right, and keeps green from the center sample.

### Why this design was picked

Rendering to a texture first is the standard postprocessing pipeline because image effects operate on the final scene as a 2D image.

Using a fullscreen triangle instead of a fullscreen quad is a very good choice because:

- it uses only 3 vertices,
- it avoids a diagonal seam issue that can happen with quads,
- it is a common modern rendering technique.

Creating the color and depth targets as textures instead of renderbuffers is also useful because textures are sampleable later if needed.

### Other valid options

- A renderbuffer could have been used for depth instead of a depth texture. That is valid if the depth buffer only needs to be written, not sampled.
- A fullscreen quad could have been used instead of a fullscreen triangle, but the triangle is simpler and slightly better.
- The chromatic aberration shader could have offset all channels radially from the screen center for a more realistic effect, but the simple horizontal version is cheaper and matches the assignment.

---

## Bigger Design Story Across Requirements 1 to 11

The project builds in layers:

1. Shaders define how vertices and fragments are processed.
2. Meshes provide geometry.
3. Transforms place geometry in the scene.
4. Pipeline state controls OpenGL behavior around the shaders.
5. Textures provide image data.
6. Samplers control how textures are read.
7. Materials bundle shader, state, and uniforms.
8. ECS organizes scene data cleanly.
9. The forward renderer turns ECS data into draw calls.
10. Sky rendering improves the background and depth behavior.
11. Postprocessing adds screen-space effects after the scene is rendered.

This layering is worth mentioning in the discussion because it shows that the engine was not built as isolated features. Each requirement prepares the next one.

---

## Good Discussion Points

If you are asked "why did you do it this way?", these are strong answers:

- We used helper abstractions like `Material`, `PipelineState`, and `ForwardRenderer` to reduce duplication and keep responsibilities clear.
- We kept data-driven scene setup through JSON so shaders, materials, textures, samplers, and entities can be swapped without recompiling code.
- We chose simple and readable implementations where possible because this is an educational engine, not a production AAA renderer.
- When there were multiple valid options, we usually picked the one that matched the assignment goals and was easiest to explain and verify.

---

## Final Advice For The Discussion

Do not only memorize file names.

Be ready to explain:

- the flow of data from JSON to assets to materials to renderer,
- the difference between local space, world space, view space, clip space, and NDC,
- why transparent objects need sorting,
- why the sky is forced to depth 1,
- why postprocessing needs an off-screen framebuffer,
- why textures and samplers are separate,
- why materials are useful as a rendering abstraction.

If you can explain that flow confidently, you will sound like you understand the project rather than just having completed TODOs.
