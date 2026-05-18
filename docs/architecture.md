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
│  UISubsystem               EventBus, Camera, Input          │
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
  - `IRenderFactory`, handle-типы (`ShaderHandle`, `TextureHandle`, `BufferHandle`, `PipelineHandle`)
  - `IWindowFactory`, `IWindow`
  - `IImGuiFactory`, `IImGuiLayer`
  - `IPhysicsFactory`, `IPhysicsWorld`, `IPhysicsBody`, `IPhysicsShape`
  - `IRenderGraph`, `IFrameGraphBackend`, `PassDesc`, `DrawDesc`
- **Утилиты** (`Core/src/`) — `Log`, `Handle<Tag>`, `HandlePool`, `Timestep`, `Scope`/`Ref`
- **`EngineContext`** — RAII-синглтон инициализации процесса (логгинг + аллокатор)

### 2. CHEngine — основная библиотека

Shared-библиотека (.dylib/.dll). Реализует:

- `Application` — главный класс, владеет всеми подсистемами через `Scope<T>`
- `ModuleManager` — загрузка/выгрузка плагинов
- `World` + `Scene` + `SystemScheduler` — ECS-симуляция
- `RenderSubsystem`, `PhysicsSubsystem`, `UISubsystem` — RAII-подсистемы
- `ResourceManager` — централизованный менеджер ресурсов (шейдеры, текстуры, модели)
- `Input`, `Camera`, `Layer`, `LayerStack`
- `Mesh`, `Material`, `MaterialInstance`
- `EventSystem`, `EventBus`

### 3. Модули — платформенные реализации

Каждый модуль — отдельная shared-библиотека. Экспортирует два символа:

```cpp
extern "C" IModuleFactory* CreateFactory();
extern "C" void DestroyFactory(IModuleFactory*);
```

## Ключевые паттерны

### RAII-подсистемы (заменили статические Facade)

`Application` владеет тремя RAII-подсистемами:

```cpp
// Доступ через Application::Get()
RenderSubsystem&  render  = Application::Get().Render();
PhysicsSubsystem* physics = Application::Get().Physics();  // nullptr если недоступна
UISubsystem*      ui      = Application::Get().UI();

// Проверка наличия опциональных подсистем
if (Application::Get().HasPhysics()) { ... }
if (Application::Get().HasUI())      { ... }
```

`RenderSubsystem` владеет `IRenderFactory*`, фрейм-графом и горячей перезагрузкой шейдеров.  
`PhysicsSubsystem` владеет `IPhysicsFactory*` и создаёт/уничтожает физические миры.  
`UISubsystem` владеет `IImGuiLayer*` и управляет `Begin()`/`End()`.

### Handle-based resource management

Все GPU-ресурсы идентифицируются хэндлами — `Handle<Tag>` (index + generation, 8 байт).  
Это исключает висячие указатели и обеспечивает type-safety:

```cpp
ShaderHandle  sh  = Application::Get().Render().CreateShaderFromFile("Mesh", "shaders/mesh.slang");
TextureHandle tex = Application::Get().Render().CreateTextureFromFile("textures/albedo.png");
```

`Handle<ShaderTag>` и `Handle<TextureTag>` — разные типы, компилятор не перепутает.  
`Handle::IsValid()` проверяет `index != 0xFFFFFFFF`.

### ResourceManager

Синглтон `ResourceManager::Instance()` хранит лоадеры:

| Лоадер | Тип хэндла | Кэш-ключ |
|--------|-----------|---------|
| `ShaderLoader` | `ShaderHandle` | путь к `.slang` файлу |
| `TextureLoader` | `TextureHandle` | путь к файлу текстуры |
| `ModelLoader` | `ModelHandle` | путь к `.obj`/`.gltf`/`.glb` |

### Декларативный фрейм-граф

Рендеринг описывается декларативно через `PassDesc` (без callback'ов).  
Backend сам разворачивает список пассов в нативные команды API.

```cpp
// Пример: RenderSystem строит пасс каждый кадр
PassDesc pass;
pass.Pipeline         = m_MeshPipeline;
pass.ColorAttachments = { m_HDRTarget };
pass.DepthAttachment  = m_DepthTarget;
pass.Uniforms         = { {m_CameraUBO, 0}, {m_LightingUBO, 2} };
pass.Draws            = BuildDrawList(scene);

Application::Get().Render().GetFrameGraph().AddPass(std::move(pass));
```

### Фазовая симуляция

Системы (`ISystem`) делятся на фазы:

| Фаза | Назначение | Примеры |
|------|-----------|---------|
| `Simulation` | Физика, скрипты, логика, lifetime | `PhysicsSystem`, `LuaScriptSystem`, `LifetimeSystem` |
| `Presentation` | Рендеринг | `RenderSystem`, `UIRenderSystem` |

Внутри фазы системы сортируются по приоритету (`uint8_t`, 0 = первым).

### DeferredOps — отложенные операции

Во время итерации по ECS нельзя создавать/удалять сущности — это испортит итераторы.  
`DeferredOps` накапливает операции и применяет их после всех фаз:

```cpp
// Внутри системы:
void Run(World& world, DeferredOps& ops, Timestep dt) override {
    scene.ForEach<ProjectileComponent>([&](EntityHandle h, ..., ProjectileComponent& p) {
        if (p.HitSomething)
            ops.DestroyEntity(h);  // отложено до Flush()
    });
}
```

### EventBus (внутри World)

Типизированная шина событий для pub/sub между системами:

```cpp
world.GetEventBus().Publish<CollisionEvent>(SystemPhase::Simulation, entityA, entityB);

ops.GetEventBus().ConsumePhase<CollisionEvent>(SystemPhase::Simulation, [](CollisionEvent& e) {
    // обработка
});
```

### ModuleManager — загрузка плагинов

Горячая перезагрузка модулей в текущей версии **упрощена** — работает только для шейдеров.  
Renderer, Window, Physics и ImGui модули не перезагружаются в рантайме.

## Последовательность запуска

```
main() [EntryPoint.h]
  │
  ├─ Парсинг CLI (--renderer=...)
  ├─ Загрузка engine.json (renderer_pending / renderer)
  ├─ EngineContext (RAII-синглтон: логгинг + аллокатор)
  ├─ CreateApplication(config)   ← пользовательский код
  │
  └─ Application::Application()
       ├─ ModuleManager::Load(WindowGLFW)
       ├─ ModuleManager::Load(RendererXXX)     ← RendererOGL / RendererMTL
       ├─ ModuleManager::Load(ImGuiXXX)
       ├─ ModuleManager::Load(PhysicsPhysX)    [опционально, Windows only]
       ├─ Window::Create(windowFactory, api)
       ├─ m_Render   = Scope<RenderSubsystem>(factory, initInfo)
       ├─ m_UI       = Scope<UISubsystem>(imguiFactory, layer)
       └─ m_Physics  = Scope<PhysicsSubsystem>(physicsFactory)  [если доступна]
```

При API-переключении: `Application::RequestRestart()` → `execv` без `--renderer=` → новый процесс читает `renderer_pending` из `engine.json`.

## Главный цикл

```cpp
Application::Run() {
    while (m_Running) {
        // Shader hot-reload — раз в 0.5с
        if (shaderPollAcc >= 0.5f) {
            m_Render->PollShaders();
            shaderPollAcc = 0;
        }

        Input::BeginFrame(platformWindow);

        m_Render->BeginFrame();
        m_Render->BeginFrameGraph();     // graph->Reset()

        for (Layer* layer : m_LayerStack)
            layer->OnUpdate(dt);         // World::Update внутри (Simulation + Presentation)

        m_Render->EndFrameGraph();       // Compile() + Execute(backend)

        if (m_UI) {
            m_UI->Begin();
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender();
            m_UI->End();
        }

        m_Render->EndFrame();
        m_Window->OnUpdate();            // PollEvents + SwapBuffers
    }
}
```
