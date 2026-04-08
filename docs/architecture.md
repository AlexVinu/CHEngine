# Архитектура CHEngine

## Общая схема

```
┌─────────────────────────────────────────────────────────────┐
│                      Пользовательский код                    │
│          (CreateApplication / Layer / ISystem)              │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│                         CHEngine.dll                         │
│                                                             │
│  Application  →  LayerStack  →  World  →  SystemScheduler  │
│       │                           │                         │
│  RenderFacade              Scene (entt ECS)                 │
│  PhysicsFacade             CommandBuffer                    │
│  UIFacade                  Camera, Input                    │
└──────┬──────────────────────────────────────────────────────┘
       │  динамическая загрузка (ModuleManager)
┌──────▼──────────────────────────────────────────────────────┐
│                         Модули (плагины)                    │
│                                                             │
│  RendererOGL  │  RendererMetal  │  RendererVulkan           │
│  WindowGLFW                                                 │
│  ImGuiOGL     │  ImGuiMTL       │  ImGuiVK                  │
│  PhysicsPhysX                                               │
└─────────────────────────────────────────────────────────────┘
```

## Слои системы

### 1. Core — разделяемые примитивы

`Core/` не зависит ни от чего внутри движка. Содержит:

- **Интерфейсы** (`Core/Interfaces/`) — чистые абстракции для всех модулей:
  - `IRenderFactory`, `IRenderer`, `IRenderApi`, `IShader`, `IVertexArray`, `ITexture`, `IFramebuffer`
  - `IWindowFactory`, `IWindow`
  - `IImGuiFactory`, `IImGuiLayer`
  - `IPhysicsFactory`, `IPhysicsWorld`, `IPhysicsBody`, `IPhysicsShape`
- **Утилиты** (`Core/src/`) — `Log`, `FileSystem`, `Handle`, `Timestep`, `Scope`/`Ref`

### 2. CHEngine — основная библиотека

Shared-библиотека (.dylib/.dll). Загружается при старте приложения. Реализует:

- `Application` — главный класс, управляет жизненным циклом
- `ModuleManager` — загрузка/выгрузка/горячая перезагрузка плагинов
- `World` + `Scene` + `SystemScheduler` — ECS-симуляция
- `RenderFacade`, `PhysicsFacade`, `UIFacade` — статические фасады
- `Input`, `Camera`, `Layer`, `LayerStack`
- `ModelLoader` (OBJ/GLTF), `Mesh`, `Material`
- `EventSystem`

### 3. Модули — платформенные реализации

Каждый модуль — отдельная shared-библиотека. Экспортирует два символа:

```cpp
extern "C" IModuleFactory* CreateFactory();
extern "C" void DestroyFactory(IModuleFactory*);
```

Движок загружает модуль, вызывает `CreateFactory()` и получает конкретный объект-фабрику.

## Ключевые паттерны

### Facade

`RenderFacade`, `PhysicsFacade`, `UIFacade` — статические классы без состояния.  
Скрывают внутреннее устройство (фабрики, пулы ресурсов) от пользовательского кода.

```cpp
// Пользователь просто вызывает:
RenderFacade::Submit(shader, vao, transform);

// Внутри фасад знает про RenderResourceManager, IRenderer, UBO и т.д.
```

### Handle-based resource management

Ресурсы (шейдеры, VAO, текстуры) возвращаются как `Handle<Tag>` — обёртка над `uint32_t`.  
Это исключает висячие указатели и обеспечивает type-safety:

```cpp
ShaderHandle    shader  = RenderFacade::CreateShader("shaders/pbr.glsl");
TextureHandle   texture = RenderFacade::CreateTexture("textures/albedo.png");
VertexArrayHandle vao   = RenderFacade::CreateVertexArray(layout, vb, ib);
```

`Handle<ShaderTag>` и `Handle<TextureTag>` — разные типы, компилятор не перепутает.

### Фазовая симуляция

Системы (`ISystem`) делятся на фазы:

| Фаза | Назначение | Примеры |
|------|-----------|---------|
| `Initialization` | Однократная инициализация | — |
| `Simulation` | Физика, логика, lifetime | `PhysicsSystem`, `LifetimeSystem` |
| `Presentation` | Рендеринг | `RenderSystem` |

Внутри фазы системы сортируются по приоритету (`uint8_t`). Это гарантирует детерминированный порядок.

### CommandBuffer — отложенные операции

Во время итерации по ECS нельзя создавать/удалять сущности — это испортит итераторы.  
`CommandBuffer` накапливает команды и применяет их после завершения всех фаз:

```cpp
commandBuffer.destroyEntity(entityHandle);  // Запомнить
// ... после World::update():
commandBuffer.flush(scene);                 // Применить
```

### ModuleManager — горячая перезагрузка

На macOS/Linux: файл просто перезагружается через `dlopen`.  
На Windows: файл копируется во временный путь (`path_temp.dll`), чтобы обойти блокировку файла, после чего загружается копия.

При изменении файла модуля:
1. `OnBeforeReload()` — пользователь уничтожает объекты старого модуля
2. Старый модуль выгружается
3. Новый модуль загружается
4. `OnAfterReload(newFactory)` — пользователь пересоздаёт объекты

## Последовательность запуска

```
main() [EntryPoint.h]
  │
  ├─ Парсинг CLI (--renderer=...)
  ├─ Загрузка engine.json
  ├─ CreateApplication(config)   ← пользовательский код
  │
  └─ Application::Application()
       ├─ ModuleManager::Load(WindowGLFW)
       ├─ ModuleManager::Load(RendererXXX)
       ├─ ModuleManager::Load(ImGuiXXX)
       ├─ ModuleManager::Load(PhysicsPhysX)  [опционально]
       ├─ Window::Create(windowFactory, api)
       ├─ RenderFacade::InitRenderer(initInfo)
       ├─ UIFacade::Init(imguiFactory)
       ├─ PhysicsFacade::Init(physicsFactory)
       └─ Создать дефолтный World

Application::Run()
  loop:
    ├─ Input::BeginFrame()
    ├─ RenderFacade::BeginFrame() + Clear()
    ├─ Layer::OnUpdate(dt)
    ├─ World::update(dt)         ← Simulation + Presentation фазы
    ├─ UIFacade::Begin()
    ├─ Layer::OnImGuiRender()
    ├─ UIFacade::End()
    ├─ RenderFacade::EndFrame()
    └─ Window::OnUpdate()
```
