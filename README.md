
# 🧱 3D Model Renderer

A real-time 3D renderer built with C++, OpenGL, and ImGui, featuring modern rendering techniques such as **deferred shading**, **physically-based materials**, and **post-processing effects**. This project serves as a personal graphics engine prototype with a modular UI for experimentation and learning, following the lessons of the [learnopengl](https://learnopengl.com/) site.

---

## ✨ Features

### ✅ Rendering Pipeline
- **Forward Rendering**
- **Deferred Rendering** (G-Buffer setup and lighting pass)
- **Debug Views**: Inspect G-buffer contents like Albedo, Normals, Depth, Material Props, etc.

### ✅ Materials & PBR
- PBR workflows with support for:
  - Albedo
  - Metallic
  - Roughness
  - Normal Maps
  - Height Maps (Parallax Occlusion Mapping)
- Multiple material slots per model
- Real-time material editing UI

### ✅ Post-Processing Effects
- **Bloom** with configurable blur passes
- Exposure and Gamma correction

### ✅ Lighting System
- Directional and Point Lights
- Real-time UI controls (position, intensity, toggles)
- Visual debug of lighting properties

### ✅ UI (powered by ImGui)
- System information & OpenGL details
- FPS graph
- Scene overview & object transformations
- Material editor
- Post-processing toggles
- Camera control and debug panels

---

## 🖥️ Controls

### 🔧 General
- `2`: Cycle Render Modes (Forward → Debug FBO → Deferred)
- `3`: Cycle Debug Views (Albedo, Normals, Depth, etc.)
- `F`: Toggle between Free and Orbit camera modes

### 🎮 Camera
- `WASD`: Move
- `SPACE` / `CTRL`: Up / Down
- Right Mouse + Drag: Rotate
- Mouse Wheel: Zoom

---

## 🧪 Models Included

- Rifle with multiple PBR materials
- Survival backpack
- Patrick model
- Brick floor (with displacement)
- Procedural plane

> **Disclaimer**: I do not own the rights to the 3D models used in this project. They were downloaded for free and are used strictly for educational and non-commercial purposes.
