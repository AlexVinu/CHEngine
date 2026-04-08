# CHЁngine
<img width="2000" height="400" alt="84956" src="https://github.com/user-attachments/assets/ececca6f-cde5-4221-bba6-d378dbd6d750" />

Кроссплатформенный игровой движок на **C++20** с модульной архитектурой, ECS, физикой и горячей перезагрузкой шейдеров и плагинов.

---

## Возможности

- 🎨 **Мульти-рендеринг** — OpenGL 4.1, Metal (Apple Silicon / Intel Mac), Vulkan; выбор в рантайме
- 🧩 **Модульная архитектура** — рендерер, окно, UI и физика — отдельные `.dylib`/`.dll`, загружаются динамически
- 🔁 **Горячая перезагрузка** — шейдеры и ImGui-модуль обновляются без перезапуска
- 🌍 **ECS** — Entity Component System на базе [entt](https://github.com/skypjack/entt) с поддержкой фазовой симуляции
- ⚙️ **World / SystemScheduler** — детерминированные фазы Initialization → Simulation → Presentation
- 💥 **Физика** — NVIDIA PhysX + Blast (опционально)
- 📦 **Загрузка моделей** — OBJ и GLTF/GLB из коробки
- 🖥️ **ImGui** — встроенный UI-оверлей, независимо обновляемый модуль
- 🪵 **Логирование** — spdlog с уровнями Debug / Release / Dist

---

## Платформы

| Платформа | Компилятор | Рендереры | Статус |
|-----------|-----------|-----------|--------|
| macOS (Apple Silicon / Intel) | Clang | Metal, OpenGL | ✅ |
| Windows | MSVC 2022 | OpenGL, Vulkan, DirectX11/12 | ✅ |
| Linux | GCC / Clang | OpenGL, Vulkan | ✅ |

---

## Структура проекта

```
CHEngine/
├── Core/                        # Разделяемые интерфейсы и утилиты
│   ├── src/                     # Log, FileSystem, Memory, Handle, Timestep
│   └── Interfaces/              # IRenderFactory, IWindowFactory, IPhysicsFactory, IImGuiFactory
│
├── CHEngine/                    # Основная shared-библиотека (.dylib / .dll)
│   └── src/CHEngine/
│       ├── Application.*        # Главный класс, главный цикл
│       ├── ModuleManager.*      # Загрузка / горячая перезагрузка плагинов
│       ├── Scene/               # Scene, Entity, Components (ECS)
│       ├── World/               # World, SystemScheduler, CommandBuffer, ISystem
│       ├── Render/              # RenderFacade, RenderResourceManager
│       ├── Physics/             # PhysicsFacade
│       ├── Mesh/                # Mesh, Material, ModelLoader (OBJ / GLTF)
│       ├── Camera/              # Camera
│       ├── Input/               # Input (polling)
│       ├── UI/                  # UIFacade
│       ├── Layer/               # Layer, LayerStack
│       └── Events/              # Event, EventDispatcher, KeyEvent, MouseEvent…
│
├── Modules/                     # Динамически загружаемые плагины
│   ├── Rendering/
│   │   ├── RendererOGL/         # OpenGL 4.1
│   │   ├── RendererMetal/       # Metal (macOS / iOS)
│   │   └── RendererVulkan/      # Vulkan
│   ├── Window/WindowGLFW/       # GLFW окно
│   ├── UI/
│   │   ├── ImGuiOGL/
│   │   ├── ImGuiMTL/
│   │   └── ImGuiVK/
│   └── Physics/PhysicsPhysX/   # NVIDIA PhysX + Blast
│
├── Sandbox/                     # Демонстрационное приложение
├── docs/                        # Подробная документация
└── CMakeLists.txt
```

---

## Зависимости

| Библиотека | Назначение |
|------------|-----------|
| [entt](https://github.com/skypjack/entt) | ECS |
| [glm](https://github.com/g-truc/glm) | Математика (vec3, mat4, quat) |
| [GLFW](https://www.glfw.org/) | Окно, контекст, ввод |
| [GLAD](https://glad.dav1d.de/) | Загрузчик OpenGL |
| [spdlog](https://github.com/gabime/spdlog) | Логирование |
| [Dear ImGui](https://github.com/ocornut/imgui) | Редакторский UI |
| [tinygltf](https://github.com/syoyo/tinygltf) | GLTF / GLB модели |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ модели |
| [PhysX + Blast](https://github.com/NVIDIA-Omniverse/PhysX) | Физический движок (опц.) |
| [boost](https://www.boost.org/) | UUID |

Все зависимости — **vendored**, ничего устанавливать вручную не нужно.

---

## Сборка

### Требования

- CMake **3.5+**
- C++**20** компилятор: Clang 12+, GCC 10+, MSVC 2022

### Быстрый старт

```bash
git clone https://github.com/AlexVinu/CHEngine
cd CHEngine

cmake -B build \
  -DCHE_BUILD_SANDBOX=ON \
  -DCHE_BUILD_OPENGL=ON \
  -DCHE_BUILD_METAL=ON     # только macOS

cmake --build build --config Debug -j$(nproc)
```

### CMake-опции

| Опция | По умолчанию | Описание |
|-------|-------------|---------|
| `CHE_BUILD_SANDBOX` | `ON` | Собрать демо-приложение |
| `CHE_BUILD_OPENGL` | `ON` | Модуль OpenGL |
| `CHE_BUILD_VULKAN` | `OFF` | Модуль Vulkan |
| `CHE_BUILD_METAL` | `ON` (macOS) | Модуль Metal |
| `CHE_BUILD_PHYSICS` | `ON` | Модуль PhysX (если доступен) |

### Windows (MSVC)

```bat
cmake -B build -G "Visual Studio 17 2022" -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON
cmake --build build --config Debug
```

### macOS (Clang)

```bash
cmake -B build -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_METAL=ON
cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)
```

### Linux (GCC / Clang)

```bash
cmake -B build -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON
cmake --build build --config Debug -j$(nproc)
```

---

## Запуск

После сборки бинарник и все `.dylib`/`.dll` находятся рядом:

```
bin/Debug-macos-x64/Sandbox/
├── Sandbox                  ← исполняемый файл
├── CHEngine.dylib
├── CHEngine_CORE.dylib
├── libRendererMTL.dylib
├── libRendererOGL.dylib
├── libWindowGLFW.dylib
├── libImGuiMTL.dylib
├── shaders/
└── assets/
```

```bash
# macOS / Linux
./bin/Debug-macos-x64/Sandbox/Sandbox

# Windows
.\bin\Debug-windows-x64\Sandbox\Sandbox.exe
```

### Выбор рендерера

```bash
./Sandbox --renderer=metal    # Metal  (macOS)
./Sandbox --renderer=opengl   # OpenGL (все платформы)
./Sandbox --renderer=vulkan   # Vulkan
```

Выбор сохраняется в `engine.json` рядом с исполняемым файлом — при следующем запуске аргумент можно не указывать.

---

## Создание приложения

```cpp
#include <CHEngine.h>

class GameLayer : public CHEngine::Layer {
public:
    void OnAttach() override {
        // Загрузка ресурсов, создание сцены
        auto& scene = CHEngine::Application::Get().GetWorld().GetScene();
        auto entity = scene.CreateEntity("Cube");

        auto meshes = CHEngine::ModelLoader::Load("assets/cube.obj");
        entity.GetComponent<CHEngine::MeshComponent>().Meshes = meshes;
        entity.GetComponent<CHEngine::TransformComponent>().Position = {0, 0, -3};
    }

    void OnUpdate(CHEngine::Timestep dt) override {
        if (CHEngine::Input::IsKeyPressed(CHEngine::Key::Escape))
            CHEngine::Application::Get().Close();
    }

    void OnImGuiRender() override {
        ImGui::Begin("Stats");
        ImGui::Text("dt: %.2f ms", dt * 1000.f);
        ImGui::End();
    }
};

class MyApp : public CHEngine::Application {
public:
    MyApp(const CHEngine::ApplicationConfig& cfg) : Application(cfg) {
        PushLayer(new GameLayer());
    }
};

// Точка входа — определяется пользователем
CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& cfg) {
    return new MyApp(cfg);
}
```

---

## Документация

Полная документация по всем подсистемам находится в папке [`docs/`](docs/):

| Раздел | Описание |
|--------|---------|
| [Архитектура](docs/architecture.md) | Схема системы, паттерны, последовательность запуска |
| [Быстрый старт](docs/getting-started.md) | Сборка, первый проект |
| [ECS / Scene / World](docs/ecs.md) | Сущности, компоненты, системы, CommandBuffer |
| [Рендеринг](docs/rendering.md) | RenderFacade, шейдеры, текстуры, UBO |
| [Физика](docs/physics.md) | PhysX-интеграция, RigidBody, режимы синхронизации |
| [Модули](docs/modules.md) | Горячая перезагрузка, создание своего модуля |
| [Ввод и события](docs/input-events.md) | Input polling, EventDispatcher, Layer |

Или читай на [GitHub Wiki](https://github.com/AlexVinu/CHEngine/wiki).

---

## Лицензия

Смотри файл [LICENSE](LICENSE).
