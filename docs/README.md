# CHEngine — Документация

CHEngine — игровой движок на C++17 с модульной архитектурой, поддержкой нескольких графических API (OpenGL, Vulkan, Metal) и горячей перезагрузкой модулей.

## Содержание

| Раздел | Описание |
|--------|----------|
| [Архитектура](architecture.md) | Общая структура, модули, паттерны |
| [Быстрый старт](getting-started.md) | Сборка, запуск, первый проект |
| [ECS / Scene / World](ecs.md) | Система сущностей, компоненты, симуляция |
| [Рендеринг](rendering.md) | RenderFacade, ресурсы, шейдеры |
| [Физика](physics.md) | PhysX-интеграция, компоненты, режимы синхронизации |
| [Модульная система](modules.md) | Загрузка плагинов, горячая перезагрузка |
| [Ввод и события](input-events.md) | Input polling, event system, слои |

## Структура репозитория

```
CHEngine-main-2/
├── Core/                  # Разделяемые интерфейсы и утилиты (логирование, память, FileSystem)
│   ├── src/               # Реализация: Log, FileSystem, STL-обёртки, Handle
│   └── Interfaces/        # Абстракции: IRenderFactory, IWindowFactory, IPhysicsFactory, IImGuiFactory
├── CHEngine/              # Основная shared-библиотека движка (.dylib/.dll/.so)
│   └── src/CHEngine/      # Scene, ECS, World, Render/Physics/UI Facade, Input, Camera, Layer
├── Modules/               # Динамически загружаемые плагины
│   ├── Rendering/         # RendererOGL, RendererVulkan, RendererMetal
│   ├── Window/            # WindowGLFW
│   ├── UI/                # ImGuiOGL, ImGuiVK, ImGuiMTL
│   └── Physics/           # PhysicsPhysX
├── Sandbox/               # Демонстрационное приложение
└── docs/                  # Эта документация
```

## Зависимости

| Библиотека | Назначение |
|------------|-----------|
| [entt](https://github.com/skypjack/entt) | Entity Component System |
| [glm](https://github.com/g-truc/glm) | Математика (vec3, mat4, quat) |
| [GLFW](https://www.glfw.org/) | Окно, контекст, ввод |
| [GLAD](https://glad.dav1d.de/) | Загрузчик OpenGL |
| [spdlog](https://github.com/gabime/spdlog) | Логирование |
| [imgui](https://github.com/ocornut/imgui) | UI |
| [tinygltf](https://github.com/syoyo/tinygltf) | Загрузка GLTF-моделей |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | Загрузка OBJ-моделей |
| [PhysX + Blast](https://github.com/NVIDIA-Omniverse/PhysX) | Физический движок |
| [boost](https://www.boost.org/) | UUID, утилиты |
