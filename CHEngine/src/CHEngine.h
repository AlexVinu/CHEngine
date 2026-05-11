#pragma once

// For use by CHEngine application

// Core ---------------
#include "Memory/MemorySystem.h"
#include "Log/Log.h"
#include "PlatformAPICapabilities.h"
#include "Profiler.h"
// Core ===============

#include "CHEngine/Application.h"
#include "CHEngine/Layer/Layer.h"

// Events -------------------------
#include "CHEngine/Events/Event.h"
#include "CHEngine/Events/KeyEvent.h"
#include "CHEngine/Events/MouseEvent.h"
#include "CHEngine/Events/ApplicationEvent.h"
// ================================

// Camera -------------------------
#include "CHEngine/Camera/EditorCamera.h"
// ================================

// Mesh & Model Loading -----------
#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/ResourceManager/ResourceManager.h"
// ================================

// Scene --------------------------
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Light.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/SceneSerializer.h"
#include "CHEngine/Scene/RecentFiles.h"
// ================================

// Input --------------------------
#include "CHEngine/Input/Input.h"
#include <Input/KeyCodes.h>
// ================================

// Physics ------------------------
#include "CHEngine/Physics/PhysicsFacade.h"
// ================================

// Utils --------------------------
#include "CHEngine/Utils/FileDialog.h"
// ================================

// Project ------------------------
#include "CHEngine/Project/Project.h"
#include "CHEngine/Project/ProjectManager.h"
// ================================

// ImGui --------------------------
#include <imgui.h>
// ================================

// --Entry Point -------------------
// Only included in the ONE translation unit that defines main.
// Define CHE_INCLUDE_ENTRY_POINT before including CHEngine.h in your
// application's main .cpp (e.g. SandboxApp.cpp).
#ifdef CHE_INCLUDE_ENTRY_POINT
#include "CHEngine/EntryPoint.h"
#endif
// ---------------------------------