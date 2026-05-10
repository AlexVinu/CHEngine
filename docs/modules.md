# Модульная система

## Концепция

Все платформенные реализации (OpenGL, Vulkan, Metal, PhysX, GLFW, ImGui) — отдельные shared-библиотеки.  
`ModuleManager` загружает их в рантайме через `dlopen`/`LoadLibrary`.

Это даёт:
- **Горячую перезагрузку** без перезапуска приложения
- **Выбор рендерера** в рантайме (`--renderer=opengl`)
- **Опциональные модули** (физика может отсутствовать)
- **Изоляцию** — замена рендерера не трогает остальной код

## Интерфейс модуля

Каждый модуль экспортирует ровно два символа:

```cpp
extern "C" CHE_API IModuleFactory* CreateFactory();
extern "C" CHE_API void DestroyFactory(IModuleFactory* factory);
```

`IModuleFactory` — базовый класс. Конкретные фабрики:

| Интерфейс | Модули |
|-----------|--------|
| `IRenderFactory` | RendererOGL, RendererVulkan, RendererMetal |
| `IWindowFactory` | WindowGLFW |
| `IImGuiFactory` | ImGuiOGL, ImGuiVK, ImGuiMTL |
| `IPhysicsFactory` | PhysicsPhysX |

## ModuleManager API

```cpp
// Загрузить модуль
ModuleHandle handle = ModuleManager::LoadModule("lib/libRendererOGL.dylib");

// Получить фабрику
auto* factory = ModuleManager::GetFactory<IRenderFactory>(handle);

// Подписаться на горячую перезагрузку
ModuleManager::Watch(ModuleType::ImGui, {
    .OnBeforeReload = []() {
        // Уничтожить ImGui-слой и все объекты старого модуля
        UIFacade::Shutdown();
    },
    .OnAfterReload = [](IModuleFactory* newFactory) {
        // Пересоздать через новую фабрику
        UIFacade::Init(static_cast<IImGuiFactory*>(newFactory));
    }
});

// Выгрузить модуль
ModuleManager::UnloadModule(handle);
```

## Горячая перезагрузка

Движок опрашивает файлы модулей каждые **1 секунду**.  
При изменении `.dylib`/`.dll` — автоматическая перезагрузка.

### Последовательность перезагрузки

```
1. FileWatcher обнаружил изменение lib/libImGuiOGL.dylib
2. Вызов OnBeforeReload() — пользователь уничтожает старые объекты
3. dlclose(старый модуль)
4. [Windows] удалить старую теневую копию
5. [Windows] скопировать новый файл → lib/libImGuiOGL_temp.dll
6. dlopen(новый файл / теневая копия)
7. Вызов OnAfterReload(newFactory) — пользователь пересоздаёт объекты
8. При ошибке: откат к резервной копии _prev.dll
```

### Windows: теневые копии

На Windows загруженную DLL нельзя перезаписать — она заблокирована.  
Движок работает с теневой копией `path_temp.dll`, что позволяет пересобирать оригинал в любой момент.

### Что перезагружается

| Модуль | Горячая перезагрузка |
|--------|---------------------|
| RendererOGL/Metal/Vulkan | Нет — держит GL/Metal контекст |
| WindowGLFW | Нет — держит окно ОС |
| ImGuiOGL/MTL/VK | **Да** — полная перезагрузка |
| PhysicsPhysX | Нет — требует пересоздания мира |

### Шейдеры (отдельный механизм)

Шейдеры имеют собственный механизм горячей перезагрузки через `RenderResourceManager`:

- Опрос каждые **0.5 секунды**
- При изменении файла — перекомпиляция шейдера
- Работает независимо от перезагрузки модулей

## Создание собственного модуля

Минимальный пример модуля рендерера:

```cpp
// MyRenderer/src/MyRenderer.cpp

#include <Core/Interfaces/Render/IRenderFactory.h>
#include <Core/Interfaces/Render/IRenderer.h>

class MyRenderer : public IRenderer {
public:
    void BeginScene(const SceneData& data) override { /* ... */ }
    void EndScene() override { /* ... */ }
    void Submit(IShader*, IVertexArray*, const glm::mat4&) override { /* ... */ }
    void BeginFrame() override { /* ... */ }
    void EndFrame() override { /* ... */ }
    void Clear() override { /* ... */ }
};

class MyRenderFactory : public IRenderFactory {
public:
    IRenderer* CreateRenderer() override { return new MyRenderer(); }
    ERenderAPI GetAPI() const override { return ERenderAPI::OPENGL; }
    // ... CreateShader, CreateVertexArray, CreateTexture, CreateFramebuffer
};

extern "C" CHE_API IModuleFactory* CreateFactory() {
    return new MyRenderFactory();
}

extern "C" CHE_API void DestroyFactory(IModuleFactory* f) {
    delete f;
}
```

CMakeLists.txt для модуля:

```cmake
add_library(MyRenderer SHARED
    src/MyRenderer.cpp
)

target_link_libraries(MyRenderer PRIVATE CHEngine_CORE)
target_compile_definitions(MyRenderer PRIVATE CHE_BUILD_MODULE_DLL)

# Вывод рядом с основным приложением
set_target_properties(MyRenderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}/lib"
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

## Render module resolver

Платформенный mapping API -> имена модулей даёт `RenderModuleResolver`.
В startup-пути финальная валидация рендера делается по факту загрузки модулей
и вызова `IRenderFactory::CheckIsWorking()`, а не через глобальный capability-store.
