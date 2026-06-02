# CHЁngine
<img width="2000" height="400" alt="84956" src="https://github.com/user-attachments/assets/ececca6f-cde5-4221-bba6-d378dbd6d750" />

Кроссплатформенный игровой движок на **C++20** с модульной архитектурой, ECS, шейдерами на Slang и горячей перезагрузкой шейдеров.

---

## Возможности

- **Мульти-рендеринг** — OpenGL 4.1, Metal (Apple Silicon / Intel Mac), Vulkan; переключение API в рантайме без перезапуска
- **Модульная архитектура** — рендерер, окно, UI и физика — отдельные `.dylib`/`.dll`, загружаются динамически через `ModuleManager`
- **Shaders на [Slang](https://shader-slang.org/)** — единый `.slang` исходник компилируется в GLSL, MSL или SPIR-V; горячая перезагрузка каждые 0.5 с
- **ECS** — Entity Component System на базе [entt](https://github.com/skypjack/entt); `DeferredOps` для безопасных структурных операций во время итерации
- **World / SystemScheduler** — фазы `Simulation` и `Presentation`; `WorldState` управляет активными фазами (Edit / Play / Background)
- **EventBus** — типизированная шина событий между системами, привязанная к фазе
- **Frame graph рендеринг** — `RenderSystem` сам обходит сцену и строит рендер-граф; пользователь не вызывает `Submit` вручную
- **Lua-скрипты** — `ScriptComponent` + `LuaScriptSystem`; скрипты можно добавлять и редактировать прямо из редактора
- **AI-ассистент** — встроенный чат-агент для генерации Lua-скриптов и управления сценой
- **2D / 3D UI система** — `UIOverlayCanvasComponent` (Screen Space) и `UIWorldCanvasComponent` (World Space) + Image / Text / Panel / Button / Slider
- **Физика** — NVIDIA PhysX (опционально; на macOS через o3de-форк с поддержкой ARM64/Intel); полностью handle-based API
- **Загрузка моделей** — OBJ и GLTF/GLB; кэш через `ResourceManager`
- **Редактор (Sandbox)** — orbit-камера, ImGuizmo гизмо, иерархия сцены, Play/Edit режимы, undo, экспорт проекта в `.chepak`
- **Player** — отдельный рантайм, запускающий упакованную сцену (`.chepak`) без редактора и ImGui

---

## Платформы

| Платформа | Компилятор | Рендереры | Статус |
|-----------|-----------|-----------|--------|
| macOS (Apple Silicon / Intel) | Clang | Metal, OpenGL | ✅ |
| Windows | MSVC 2022 | OpenGL, Vulkan | ✅ |
| Linux | GCC / Clang | OpenGL, Vulkan | ✅ |

---

## Структура проекта

```
CHEngine/
├── Core/           — интерфейсы и утилиты (IRenderFactory, IWindowFactory, Handle, Log…)
├── CHEngine/       — основная shared-библиотека (.dylib / .dll)
│   └── src/CHEngine/
│       ├── Application.*    — главный класс и цикл
│       ├── ModuleManager.*  — загрузка плагинов
│       ├── Scene/           — Scene, Entity, Components (ECS)
│       ├── World/           — World, SystemScheduler, DeferredOps, EventBus, Systems
│       ├── Render/          — RenderSubsystem, frame graph
│       ├── Physics/         — PhysicsSubsystem (прокси над IPhysicsFactory)
│       ├── ResourceManager/ — Shader/Texture/Model/Mesh лоадеры, AssetPack (.chepak)
│       └── …
├── Modules/        — динамически загружаемые плагины
│   ├── Rendering/  — RendererOGL, RendererMetal, RendererVulkan
│   ├── Window/     — WindowGLFW
│   ├── UI/         — ImGuiOGL, ImGuiMTL, ImGuiVK
│   └── Physics/    — PhysicsPhysX
├── Sandbox/        — редактор / демо-приложение
├── Player/         — рантайм для запуска упакованной сцены (.chepak)
├── docs/           — подробная документация
└── CMakeLists.txt
```

---

## Зависимости

| Библиотека | Назначение |
|------------|-----------|
| [entt](https://github.com/skypjack/entt) | ECS |
| [glm](https://github.com/g-truc/glm) | Математика (vec3, mat4, quat) |
| [Slang](https://shader-slang.org/) | Компилятор шейдеров |
| [GLFW](https://www.glfw.org/) | Окно, контекст, ввод |
| [GLAD](https://glad.dav1d.de/) | Загрузчик OpenGL |
| [spdlog](https://github.com/gabime/spdlog) | Логирование |
| [Dear ImGui](https://github.com/ocornut/imgui) | Редакторский UI |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | Гизмо (трансформации в viewport) |
| [tinygltf](https://github.com/syoyo/tinygltf) | GLTF / GLB модели |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ модели |
| [PhysX + Blast](https://github.com/NVIDIA-Omniverse/PhysX) | Физический движок (опц.) |
| [boost](https://www.boost.org/) | UUID |
| [sol2](https://github.com/ThePhD/sol2) + LuaJIT | Lua-скрипты |

Все зависимости — **vendored**, ничего устанавливать вручную не нужно.

---

## Сборка

### Требования

- CMake **3.5+**
- C++**20** компилятор: Clang 12+, MSVC 2022

### macOS

```bash
git clone https://github.com/AlexVinu/CHEngine
cd CHEngine

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_METAL=ON -DCHE_BUILD_PHYSICS=ON

cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)
```

### Windows (MSVC)

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug ^
  -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_PHYSICS=ON

cmake --build build --config Debug
```

### CMake-опции

| Опция | Описание |
|-------|---------|
| `CHE_BUILD_SANDBOX` | Редактор / демо-приложение |
| `CHE_BUILD_OPENGL` | Модуль OpenGL |
| `CHE_BUILD_VULKAN` | Модуль Vulkan |
| `CHE_BUILD_METAL` | Модуль Metal (только macOS) |
| `CHE_BUILD_PHYSICS` | Модуль PhysX (Windows, macOS, Linux) |

---

## Запуск

Бинарник и все `.dylib`/`.dll` собираются в одну папку:

```
bin/Debug-macos-x64/Sandbox/
├── Sandbox
├── CHEngine.dylib
├── libRendererMTL.dylib
├── libRendererOGL.dylib
├── libWindowGLFW.dylib
├── libImGuiMTL.dylib
├── shaders/
└── assets/
```

```bash
cd bin/Debug-macos-x64/Sandbox
./Sandbox                    # читает engine.json
./Sandbox --renderer=metal   # принудительно Metal (рекомендуется на Mac)
./Sandbox --renderer=opengl
```

Выбор рендерера сохраняется в `engine.json` — при следующем запуске аргумент можно не указывать.

---

## Создание приложения

```cpp
#include <CHEngine.h>

using namespace CHEngine;

class GameLayer : public Layer {
public:
    GameLayer() : Layer("GameLayer") {}

    void OnAttach() override {
        ResourceManager& rm = Application::Get().Resources();

        // Шейдеры/модели кэшируются по пути
        ShaderHandle shader = rm.Load<ShaderHandle>("Mesh", "shaders/mesh.slang");
        ModelHandle  model  = rm.Load<ModelHandle>("assets/cube.obj", shader);
        const LoadedModel* data = rm.GetModel(model);

        // m_World — собственный World слоя (Application не владеет дефолтным миром)
        Scene& scene = *m_World.GetSceneRef();
        EntityHandle h = scene.CreateEntity("Cube");
        Entity* e = scene.TryGetEntity(h);

        if (data && data->mesh.IsValid())
            e->AddComponent<MeshComponent>(MeshComponent{ data->mesh });

        auto& tc = e->AddComponent<TransformComponent>();
        tc.ObjectTransform.Position = { 0.0f, 0.0f, -3.0f };
    }

    void OnUpdate(Timestep dt) override {
        m_World.Update(dt);   // прогоняет фазы Simulation + Presentation
    }

private:
    WorldsList m_Worlds;
    World      m_World{ &m_Worlds };
};

class MyApp : public Application {
public:
    MyApp(const ApplicationConfig& cfg) : Application(cfg) {
        PushLayer(new GameLayer());
    }
};

Application* CHEngine::CreateApplication(const ApplicationConfig& cfg) {
    return new MyApp(cfg);
}
```

> Рендеринг происходит автоматически: `RenderSystem` (фаза `Presentation`) обходит
> сущности с `MeshComponent` и строит рендер-граф. Ручного `Submit` нет.

---

## Документация

| Раздел | Описание |
|--------|---------|
| [Архитектура](docs/architecture.md) | Схема системы, паттерны, последовательность запуска |
| [Быстрый старт](docs/getting-started.md) | Сборка, первый проект |
| [ECS / Scene / World](docs/ecs.md) | Сущности, компоненты, системы, DeferredOps, EventBus |
| [Рендеринг](docs/rendering.md) | RenderSubsystem, frame graph, Slang-шейдеры, текстуры, UBO |
| [Физика](docs/physics.md) | Handle-based PhysX-интеграция, RigidBody3DComponent |
| [Управление ресурсами](docs/resource-management.md) | ResourceManager, лоадеры, MeshLoader, AssetPack |
| [Модули](docs/modules.md) | Горячая перезагрузка, создание своего модуля |
| [Ввод и события](docs/input-events.md) | Input polling, EventDispatcher, Layer |

---

## Лицензия

Смотри файл [LICENSE](LICENSE).
