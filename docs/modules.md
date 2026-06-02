# Модульная система

## Концепция

Все платформенные реализации (OpenGL, Vulkan, Metal, PhysX, GLFW, ImGui) — отдельные shared-библиотеки.  
`ModuleManager` загружает их в рантайме через `dlopen`/`LoadLibrary`.

Это даёт:
- **Выбор рендерера** в рантайме (`--renderer=opengl`; смена API — через рестарт процесса)
- **Опциональные модули** (физика и ImGui могут отсутствовать)
- **Изоляцию** — замена рендерера не трогает остальной код
- **Горячую перезагрузку шейдеров** (hot-reload самих модулей сейчас отключён — см. ниже)

## Интерфейс модуля

Каждый модуль экспортирует ровно два символа:

```cpp
extern "C" CHE_MODULE_API IModuleFactory* CreateFactory();
extern "C" CHE_MODULE_API void DestroyFactory(IModuleFactory* factory);
```

Обычно их не пишут вручную — для этого есть макросы `DECLARE_MODULE_FACTORY()` /
`IMPLEMENT_MODULE_FACTORY(FactoryType)` из `IModuleFactory.h`.

`IModuleFactory` — базовый класс. Конкретные фабрики:

| Интерфейс | Модули |
|-----------|--------|
| `IRenderFactory` | RendererOGL, RendererVulkan, RendererMetal |
| `IWindowFactory` | WindowGLFW |
| `IImGuiFactory` | ImGuiOGL, ImGuiVK, ImGuiMTL |
| `IPhysicsFactory` | PhysicsPhysX |

## ModuleManager API

`ModuleManager` — нестатический объект, которым владеет `Application`. По одному
загруженному модулю на `ModuleType` (хранятся в `unordered_map<ModuleType, ModuleData>`).

```cpp
// Загрузить модуль (вызывает CreateFactory(), регистрирует по ModuleType из GetType())
bool ok = moduleManager.LoadModule("libRendererOGL.dylib");

// Получить фабрику по типу модуля
IRenderFactory* factory =
    moduleManager.GetModule<IRenderFactory>(ModuleType::Render);

// Выгрузить все модули (вызывает DestroyFactory + dlclose/FreeLibrary)
moduleManager.UnloadAll();
```

Загрузку модулей по выбранному API делает `Application` в конструкторе; прикладному
коду обычно работать с `ModuleManager` напрямую не нужно.

## Горячая перезагрузка

> **Текущее состояние:** hot-reload *модулей* в движке **упрощён и не функционирует**
> (`FileWatcher` для модулей убран). Модули загружаются один раз при старте и живут до
> завершения процесса. Работает только **горячая перезагрузка шейдеров** (см. ниже).

Концептуально модули делятся на перезагружаемые и нет: Renderer/Window/Physics держат
OS-хэндлы (GL/Metal-контекст, окно, физический мир) и не предназначены для перезагрузки;
кандидатами на hot-reload были только ImGui-модули. Сейчас этот путь отключён.

### Шейдеры (рабочий механизм)

Шейдеры имеют собственный механизм горячей перезагрузки в `RenderSubsystem`:

- `Application::Run` вызывает `RenderSubsystem::PollShaders()` каждые **0.5 секунды**
- При изменении `.slang` файла (по `FileWatcher`) шейдер перекомпилируется (`ReloadShader`)
- Работает независимо от модулей; список шейдеров — `RenderSubsystem::GetShaderEntries()`

## Контракт модуля

Каждый модуль реализует один из заводских интерфейсов (`IRenderFactory`,
`IWindowFactory`, `IImGuiFactory`, `IPhysicsFactory`). Все они наследуют
`IModuleFactory`, у которого единственный обязательный метод:

```cpp
struct IModuleFactory {
    virtual ~IModuleFactory() = default;
    virtual ModuleType GetType() const = 0;   // по нему ModuleManager раскладывает модуль
};
```

Экспортируемых символа по-прежнему два — их объявляет/реализует пара макросов из
`IModuleFactory.h` (тип линковки — `CHE_MODULE_API`):

```cpp
DECLARE_MODULE_FACTORY()             // в заголовке: extern "C" CreateFactory/DestroyFactory
IMPLEMENT_MODULE_FACTORY(MyFactory)  // в .cpp: new MyFactory() / delete factory
```

## Создание собственного модуля

`IRenderFactory` — это и есть «рендерер»: отдельного `IRenderer` нет. Фабрика владеет
всем backend-состоянием и реализует **полный** интерфейс (буферы, шейдеры, текстуры,
пайплайны, frame-graph backend, present и т.д.) — он большой, проще взять за основу
готовый `Modules/Rendering/RendererOGL`. Скелет фабрики:

```cpp
// MyRenderer/src/MyRenderFactory.h
#include <Render/IRenderFactory.h>

class MyRenderFactory : public CHEngine::IRenderFactory {
public:
    CHEngine::ModuleType GetType() const override { return CHEngine::ModuleType::Render; }
    CHEngine::ERenderAPI GetRenderApi() override   { return CHEngine::ERenderAPI::OPENGL; }
    bool CheckIsWorking() override                 { return true; }

    void Init(const CHEngine::RendererInitInfo& init) override { /* загрузить loader, выделить state */ }
    void Shutdown() override { /* освободить ресурсы */ }

    // + CreateBuffer / CreateShader / CreateTexture / CreatePipeline /
    //   CreateFrameGraphBackend / Delete(...) / UpdateBuffer / ReloadShader /
    //   GetTextureNativeID / PresentToBackbuffer / GetClipSpaceCorrection / ...
};

DECLARE_MODULE_FACTORY()
```

```cpp
// MyRenderer/src/MyRenderFactory.cpp
#include "MyRenderFactory.h"
IMPLEMENT_MODULE_FACTORY(MyRenderFactory)
```

CMakeLists.txt модуля (по образцу `Modules/Window/WindowGLFW/CMakeLists.txt`):

```cmake
project(MyRenderer LANGUAGES CXX)
file(GLOB_RECURSE CHE_SOURCES "src/*.cpp")

add_library(${PROJECT_NAME} SHARED ${CHE_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(${PROJECT_NAME} PRIVATE CHE_BUILD_MODULE_DLL)
target_include_directories(${PROJECT_NAME} PRIVATE ${CHE_INTERFACES_DIR} ${CHE_CORE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE CHEngine_CORE)

# Вывод + POST_BUILD копирование рядом с бинарником (см. WindowGLFW для деталей)
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_PREFIX}/$<CONFIG>${OUTPUT_SYSTEM_PREFIX}/lib")
```

## Типы модулей

```cpp
// ВАЖНО: значения заданы явно — вставка нового элемента в середину сдвинула бы
// остальные, и уже скомпилированные DLL начали бы возвращать неверный тип.
enum class ModuleType {
    Window        = 0,   // IWindowFactory
    WindowHandler = 1,
    Render        = 2,   // IRenderFactory
    ImGui         = 3,   // IImGuiFactory
    Physics       = 4,   // IPhysicsFactory
    None          = 5
};
```

## Render module resolver

Платформенный mapping API -> имена модулей даёт `RenderModuleResolver`.
В startup-пути финальная валидация рендера делается по факту загрузки модулей
и вызова `IRenderFactory::CheckIsWorking()`, а не через глобальный capability-store.
