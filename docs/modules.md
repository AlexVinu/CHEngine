# Модульная система

## Концепция

Все платформенные реализации (OpenGL, Metal, Vulkan, PhysX, GLFW, ImGui) — отдельные shared-библиотеки.  
`ModuleManager` загружает их в рантайме через `dlopen`/`LoadLibrary`.

Это даёт:
- **Выбор рендерера** в рантайме (`--renderer=opengl` / `--renderer=metal`)
- **Опциональные модули** (физика может отсутствовать или не собираться)
- **Изоляцию** — замена рендерера не трогает остальной код

## Интерфейс модуля

Каждый модуль экспортирует ровно два символа:

```cpp
extern "C" CHE_API IModuleFactory* CreateFactory();
extern "C" CHE_API void DestroyFactory(IModuleFactory* factory);
```

`IModuleFactory::GetType()` возвращает `ModuleType` — движок определяет по нему тип модуля.

| Интерфейс | Модули |
|-----------|--------|
| `IRenderFactory` | RendererOGL, RendererVulkan, RendererMetal |
| `IWindowFactory` | WindowGLFW |
| `IImGuiFactory` | ImGuiOGL, ImGuiVK, ImGuiMTL |
| `IPhysicsFactory` | PhysicsPhysX |

## Горячая перезагрузка

### Модули

Горячая перезагрузка модулей в текущей версии **не реализована** (FileWatcher убран из `ModuleManager`).  
Смена рендерера требует перезапуска приложения (`Application::RequestRestart()`).

| Модуль | Горячая перезагрузка |
|--------|---------------------|
| RendererOGL/Metal/Vulkan | Нет — держит GL/Metal контекст |
| WindowGLFW | Нет — держит окно ОС |
| ImGuiOGL/MTL/VK | Нет (упрощено) |
| PhysicsPhysX | Нет — требует пересоздания мира |

### Шейдеры (отдельный механизм)

Шейдеры перезагружаются через `RenderSubsystem::PollShaders()` (внутри `FileWatcher`):

- Опрос каждые **0.5 секунды** (вызывается из `Application::Run()`)
- При изменении `.slang` файла — перекомпиляция шейдера через `SlangBackend`
- Зависимые `PipelineHandle` перестраиваются автоматически (PSO invalidation chain)
- При ошибке компиляции — дамп GLSL в `glsl_dump_fail.glsl` рядом с бинарником

```cpp
// Принудительная перезагрузка конкретного шейдера:
Application::Get().Render().ReloadShader(shaderHandle);
```

## Выбор рендерера и engine.json

Приоритет: `--renderer=` CLI → `engine.json` (`renderer_pending`) → `renderer` → OpenGL по умолчанию.

`engine.json`:
```json
{
  "renderer": "opengl",
  "renderer_pending": "metal"
}
```

- `renderer` — зафиксированный рендерер (записывается после успешного старта)
- `renderer_pending` — ожидающая смена (записывается при выборе в UI, применяется при рестарте)

При краше после смены рендерера — откат к `renderer`.

## Создание собственного модуля

```cpp
// MyRenderer/src/MyRenderer.cpp
#include <Core/Interfaces/Render/IRenderFactory.h>

class MyRenderFactory : public IRenderFactory {
public:
    ModuleType GetType() const override { return ModuleType::Render; }
    ERenderAPI GetRenderApi() const override { return ERenderAPI::OPENGL; }

    void Init(const RendererInitInfo& info) override { /* init API */ }
    void Shutdown() override { /* cleanup */ }
    bool CheckIsWorking() const override { return true; }

    BufferHandle   CreateBuffer(size_t sz, BufferUsage, MemoryType, ...) override { ... }
    ShaderHandle   CreateShader(...) override { ... }
    TextureHandle  CreateTexture(...) override { ... }
    PipelineHandle CreatePipeline(const PipelineDesc&) override { ... }

    std::unique_ptr<IFrameGraphBackend> CreateFrameGraphBackend() override { ... }
    // ...
};

extern "C" CHE_API IModuleFactory* CreateFactory() { return new MyRenderFactory(); }
extern "C" CHE_API void DestroyFactory(IModuleFactory* f) { delete f; }
```

CMakeLists.txt для модуля:

```cmake
add_library(MyRenderer SHARED src/MyRenderer.cpp)
target_link_libraries(MyRenderer PRIVATE CHEngine_CORE)
target_compile_definitions(MyRenderer PRIVATE CHE_BUILD_MODULE_DLL)
set_target_properties(MyRenderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}"
)
```

## Типы модулей

```cpp
enum class ModuleType {
    Render,   // IRenderFactory
    Window,   // IWindowFactory
    ImGui,    // IImGuiFactory
    Physics,  // IPhysicsFactory
};
```
