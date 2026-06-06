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
│  RenderSubsystem           Scene (entt ECS)                 │
│  PhysicsSubsystem          DeferredOps                      │
│  UISubsystem               EventBus, WorldState             │
│  ResourceManager           Camera, Input                    │
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
  - `IRenderFactory`, `IRenderer`, `IRenderApi`, `IShader`, `IVertexArray`, `ITexture`
  - `IWindowFactory`, `IWindow`
  - `IImGuiFactory`, `IImGuiLayer`
  - `IPhysicsFactory` (единственный публичный интерфейс физики), `IPhysicsContactListener`; шейпы — через `PhysShapeDesc`/handle
- **Утилиты** (`Core/src/`) — `Log`, `FileSystem`, `Handle`, `Timestep`, `Scope`/`Ref`

### 2. CHEngine — основная библиотека

Shared-библиотека (.dylib/.dll). Загружается при старте приложения. Реализует:

- `Application` — главный класс, управляет жизненным циклом, владеет подсистемами
- `ModuleManager` — загрузка/выгрузка/горячая перезагрузка плагинов
- `World` + `Scene` + `SystemScheduler` — ECS-симуляция
- `RenderSubsystem`, `PhysicsSubsystem`, `UISubsystem` — подсистемы (доступ через `Application::Get().Render()` / `.Physics()` / `.UI()`)
- `ResourceManager` — централизованный менеджер ресурсов (`Application::Get().Resources()`)
- `MeshLoader` — контент-адресуемый кэш GPU-буферов геометрии (живёт внутри `ResourceManager`)
- `Input`, `InputSystem`, `Camera`, `Layer`, `LayerStack`
- `Mesh`, `Material`, `MaterialInstance`
- `EventBus`

### 3. Модули — платформенные реализации

Каждый модуль — отдельная shared-библиотека. Экспортирует два символа:

```cpp
extern "C" IModuleFactory* CreateFactory();
extern "C" void DestroyFactory(IModuleFactory*);
```

Движок загружает модуль, вызывает `CreateFactory()` и получает конкретный объект-фабрику.

## Ключевые паттерны

### Subsystem (бывшие Facade)

`RenderSubsystem`, `PhysicsSubsystem`, `UISubsystem` — объекты, которыми владеет
`Application` (как `Scope<>`), с детерминированным порядком уничтожения (RAII).
Они скрывают внутреннее устройство (фабрики, пулы ресурсов, frame graph) от
пользовательского кода. Доступ — через `Application::Get()`:

```cpp
RenderSubsystem&  render = Application::Get().Render();    // всегда валиден после старта
ResourceManager&  res    = Application::Get().Resources();
PhysicsSubsystem* phys   = Application::Get().Physics();   // nullptr если физика выключена
UISubsystem*      ui     = Application::Get().UI();        // nullptr в Player (ImGui-less)
```

`Physics()` и `UI()` опциональны — проверяй `HasPhysics()` / `HasUI()` перед вызовом.
Геометрию вручную сабмитить не нужно — этим занимается `RenderSystem` через frame graph.

### Handle-based resource management

Все GPU-ресурсы идентифицируются хэндлами — `Handle<Tag>` (index + generation, 8 байт).
Это исключает висячие указатели и обеспечивает type-safety:

```cpp
ResourceManager& rm = Application::Get().Resources();
ShaderHandle  shader  = rm.Load<ShaderHandle>("Mesh", "shaders/mesh.slang");
TextureHandle texture = rm.Load<TextureHandle>("textures/albedo.png");
ModelHandle   model   = rm.Load<ModelHandle>("assets/cube.obj", shader);
```

`Handle<ShaderTag>` и `Handle<TextureTag>` — разные типы, компилятор не перепутает.
`Handle::IsValid()` проверяет `index != 0xFFFFFFFF`.

Загрузка всех файловых ресурсов проходит через `ResourceManager` — он кэширует по пути
и разделяет один GPU-ресурс между всеми пользователями. Прямые вызовы
`RenderSubsystem::CreateShaderFromFile / CreateTextureFromFile` допустимы только внутри
самих лоадеров.

### ResourceManager

`Application::Get().Resources()` хранит три файловых лоадера:

| Лоадер | Тип хэндла | Кэш-ключ |
|--------|-----------|---------|
| `ShaderLoader` | `ShaderHandle` | путь к `.slang` файлу |
| `TextureLoader` | `TextureHandle` | путь к файлу текстуры |
| `ModelLoader` | `ModelHandle` | путь к `.obj`/`.gltf`/`.glb` |

`MeshLoader` — отдельный синглтон, не входит в массив лоадеров.
Кэширует GPU-буферы (VBO/IBO) по хэшу содержимого. Refcount: буферы живут пока
на них ссылается хотя бы один `Mesh` объект.

### Фазовая симуляция

Системы (`ISystem`) делятся на две фазы:

| Фаза | Назначение | Системы (по умолчанию) |
|------|-----------|---------|
| `Simulation` | Физика, логика, lifetime, скрипты, UI-ввод | `LuaScriptSystem`, `UIInputSystem`, `LifetimeSystem`, `ComponentValidationSystem`, `PhysicsSystem` |
| `Presentation` | Рендеринг | `RenderSystem`, `UIRenderSystem` |

Внутри фазы системы сортируются по приоритету (`uint8_t`). Это гарантирует детерминированный порядок. Регистрируются системы через `World::RegisterDefaultSystems()` → `SystemScheduler::EmplaceSystem<T>()`.

`WorldState` определяет, какие фазы запускаются:
- `Presenting` — только Presentation (Edit-режим)
- `Simulating` — Simulation + Presentation (Play-режим)
- `SimulatingWithoutPresenting` — только Simulation

### DeferredOps — отложенные операции

Во время итерации по ECS нельзя создавать/удалять сущности — это испортит итераторы.  
`DeferredOps` накапливает команды и применяет их после завершения всех фаз:

```cpp
// Внутри ISystem::Run(World& world, DeferredOps& ops, Timestep dt):
ops.DestroyEntity(entityHandle);      // Запомнить удаление
ops.CreateEntity("Name");             // Запомнить создание
ops.AddComponent<T>(handle, args...); // Отложить добавление компонента
// ... после всех фаз DeferredOps автоматически применяется (Flush)
```

### ModuleManager — загрузка модулей

`ModuleManager` загружает по одному модулю на `ModuleType` (`LoadModule(path)`),
получает фабрику через `CreateFactory()` и хранит её; `GetModule<T>(ModuleType)` отдаёт
типизированный указатель, `UnloadAll()` освобождает всё (вызывает `DestroyFactory` +
`dlclose`/`FreeLibrary`).

> Hot-reload **модулей** в текущей версии упрощён и не функционирует (`FileWatcher`
> для модулей убран). Работает только горячая перезагрузка **шейдеров** — см.
> [модули](modules.md) и `RenderSubsystem::PollShaders`.

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
       ├─ ModuleManager::Load(ImGuiXXX)        [если ImGuiEnabled]
       ├─ ModuleManager::Load(PhysicsPhysX)    [если PhysicsEnabled]
       ├─ Window::Create(windowFactory, api)
       ├─ m_Render    = Scope<RenderSubsystem>(factory, initInfo)
       ├─ m_UI        = Scope<UISubsystem>(imguiFactory)      [опц.]
       ├─ m_Physics   = Scope<PhysicsSubsystem>(physicsFactory) [опц.]
       └─ m_Resources = Scope<ResourceManager>()

Application::Run()
  loop (while m_Running):
    ├─ Window::PollEvents()
    ├─ RenderSubsystem::PollShaders()    каждые 0.5 с (hot-reload)
    ├─ Input::BeginFrame() + InputSystem::BeginFrame()
    ├─ RenderSubsystem::BeginFrame()
    ├─ Layer::OnUpdate(dt)               логика — ВСЕГДА; слой зовёт World::Simulate(dt)+PostUpdate()
    ├─ BeginFrameGraph()
    ├─ Layer::OnRenderUpdate(dt)         запись GPU-работы; слой зовёт World::Render(dt)
    ├─ RenderSubsystem::EndFrameGraph()
    ├─ UISubsystem::Begin() / Layer::OnUIUpdate() / UISubsystem::End()
    │     (в Player без ImGui — RenderFactory::PresentToBackbuffer)
    ├─ RenderSubsystem::EndFrame()
    └─ Window::OnUpdate()
```

`Application` сам **не создаёт и не обновляет** World — это делает прикладной слой
(в Sandbox — `SceneViewLayer` через `EditorWorldContext`/`WorldsList`). `World::Update(dt)`
прогоняет фазы `Simulation` + `Presentation` и затем `DeferredOps::Flush`.
