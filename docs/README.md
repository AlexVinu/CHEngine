# CHEngine — Документация

CHEngine — игровой движок на C++17 с модульной архитектурой, поддержкой нескольких графических API (OpenGL, Metal, Vulkan), RAII-подсистемами и Lua-скриптингом.

## Содержание

| Раздел | Описание |
|--------|----------|
| [Архитектура](architecture.md) | Общая структура, RAII-подсистемы, паттерны, цикл |
| [Быстрый старт](getting-started.md) | Сборка, запуск, первый проект |
| [ECS / Scene / World](ecs.md) | Система сущностей, компоненты, World, SystemScheduler, Lua |
| [Рендеринг](rendering.md) | RenderSubsystem, фрейм-граф, шейдеры, материалы |
| [Физика](physics.md) | PhysX-интеграция, компоненты, режимы синхронизации |
| [Модульная система](modules.md) | Загрузка плагинов, горячая перезагрузка шейдеров |
| [Ввод и события](input-events.md) | Input polling, event system, слои |

## Структура репозитория

```
CHEngine/
├── Core/                  # Разделяемые интерфейсы и утилиты
│   ├── src/               # Log, EngineContext, Handle, FileSystem, STL-обёртки
│   └── Interfaces/        # IRenderFactory, IWindowFactory, IPhysicsFactory, IImGuiFactory
│                          # IRenderGraph, IFrameGraphBackend, PassDesc, Handles
├── CHEngine/              # Основная shared-библиотека движка (.dylib/.dll)
│   └── src/CHEngine/      # Application, Scene, ECS, World, Subsystems, ResourceManager
├── Modules/               # Динамически загружаемые плагины
│   ├── Rendering/         # RendererOGL, RendererVulkan, RendererMetal + SlangBackend
│   ├── Window/            # WindowGLFW
│   ├── UI/                # ImGuiOGL, ImGuiVK, ImGuiMTL
│   └── Physics/           # PhysicsPhysX
├── Sandbox/               # Редактор (SceneView, панели, script editor, экспорт)
├── Player/                # Рантайм-плеер (.chepak → запуск сцены без редактора)
└── docs/                  # Эта документация
```

## Зависимости

| Библиотека | Назначение |
|------------|-----------|
| [entt](https://github.com/skypjack/entt) | Entity Component System |
| [glm](https://github.com/g-truc/glm) | Математика (vec3, mat4, quat) |
| [GLFW](https://www.glfw.org/) | Окно, контекст, ввод |
| [GLAD](https://glad.dav1d.de/) | Загрузчик OpenGL |
| [Slang](https://shader-slang.org/) | Кросс-компилятор шейдеров (GLSL/MSL/SPIR-V) |
| [spdlog](https://github.com/gabime/spdlog) | Логирование |
| [imgui](https://github.com/ocornut/imgui) | UI |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | 3D-гизмо в редакторе |
| [sol2](https://github.com/ThePhD/sol2) / [Lua](https://www.lua.org/) | Lua-скриптинг |
| [tinygltf](https://github.com/syoyo/tinygltf) | Загрузка GLTF-моделей |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | Загрузка OBJ-моделей |
| [PhysX](https://github.com/NVIDIA-Omniverse/PhysX) | Физический движок (Windows only) |
| [boost](https://www.boost.org/) | UUID |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON (engine.json, сериализация сцен) |
| [stb_image](https://github.com/nothings/stb) | Загрузка текстур |
