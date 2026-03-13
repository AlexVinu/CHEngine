#pragma once

// For use by CHEngine application

// Core ---------------
#include "Memory/MemorySystem.h"
// Core ===============

#include "CHEngine/Application.h"
#include "CHEngine/Layer/Layer.h"
#include "Log/Log.h"

// Events -------------------------
#include "CHEngine/Events/Event.h"
#include "CHEngine/Events/KeyEvent.h"
#include "CHEngine/Events/MouseEvent.h"
#include "CHEngine/Events/ApplicationEvent.h"
// ================================

// Camera -------------------------
#include "CHEngine/Camera/Camera.h"
// ================================

// Mesh & Model Loading -----------
#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/Mesh/ModelLoader.h"
// ================================

// Scene --------------------------
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/SceneObject.h"
// ================================

// Utils --------------------------
#include "CHEngine/Utils/FileDialog.h"
// ================================

// ImGui --------------------------
#include <imgui.h>
#include <ImGuizmo.h>
// ================================

// --Entry Point -------------------
// Only included in the ONE translation unit that defines main.
// Define CHE_INCLUDE_ENTRY_POINT before including CHEngine.h in your
// application's main .cpp (e.g. SandboxApp.cpp).
#ifdef CHE_INCLUDE_ENTRY_POINT
#include "CHEngine/EntryPoint.h"
#endif
// ---------------------------------