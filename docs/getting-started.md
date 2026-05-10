# Быстрый старт

## Требования

- CMake 3.5+
- C++17 компилятор (Clang, GCC, MSVC)
- macOS: Xcode Command Line Tools (для Metal), или просто clang
- Windows: Visual Studio 2019+ или MinGW
- Linux: GCC 9+ или Clang 10+

## Сборка

```bash
git clone https://github.com/AlexVinu/CHEngine
cd CHEngine
cmake -B build -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON
cmake --build build --config Debug
```

### CMake-опции

| Опция | По умолчанию | Описание |
|-------|--------------|----------|
| `CHE_BUILD_SANDBOX` | ON | Собрать демо-приложение |
| `CHE_BUILD_OPENGL` | ON | Собрать модуль OpenGL |
| `CHE_BUILD_VULKAN` | OFF | Собрать модуль Vulkan |
| `CHE_BUILD_METAL` | ON (macOS) | Собрать модуль Metal |
| `CHE_BUILD_PHYSICS` | ON | Собрать модуль PhysX |

### Выходные файлы

```
bin/<Config>-<platform>-<arch>/
├── Sandbox              # Исполняемый файл
└── lib/
    ├── libCHEngine.dylib
    ├── libRendererOGL.dylib
    ├── libWindowGLFW.dylib
    ├── libImGuiOGL.dylib
    └── libPhysicsPhysX.dylib
```

## Выбор рендерера

Рендерер указывается через CLI-аргумент или сохраняется в `engine.json`:

```bash
./Sandbox --renderer=opengl
./Sandbox --renderer=vulkan
./Sandbox --renderer=metal
```

Движок запоминает последний выбор в `engine.json` рядом с исполняемым файлом.

## Создание приложения

Точка входа — функция `CreateApplication`, которую пользователь определяет сам:

```cpp
#include <CHEngine.h>

class MyLayer : public CHEngine::Layer {
public:
    void OnAttach() override {
        // Инициализация: загрузка ресурсов, создание сцены
    }

    void OnUpdate(CHEngine::Timestep dt) override {
        // Обновление каждый кадр
        if (CHEngine::Input::IsKeyPressed(CHEngine::Key::Escape))
            CHEngine::Application::Get().Close();
    }

    void OnImGuiRender() override {
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", 1.0f / dt);
        ImGui::End();
    }
};

class MyApp : public CHEngine::Application {
public:
    MyApp(const CHEngine::ApplicationConfig& config)
        : CHEngine::Application(config)
    {
        PushLayer(new MyLayer());
    }
};

CHEngine::Application* CHEngine::CreateApplication(const CHEngine::ApplicationConfig& config) {
    return new MyApp(config);
}
```

## Загрузка модели и рендеринг

```cpp
#include <CHEngine.h>
#include <CHEngine/ResourceManager/ResourceManager.h>

void OnAttach() override {
    auto& scene = CHEngine::Application::Get().GetWorld().GetScene();
    auto& rm    = CHEngine::ResourceManager::Instance();

    // Загрузить шейдер и модель (кэшируются — повторный вызов вернёт тот же хэндл)
    CHEngine::ShaderHandle shader = rm.Load<CHEngine::ShaderHandle>(
        "Mesh", "shaders/mesh.slang");
    CHEngine::ModelHandle model = rm.Load<CHEngine::ModelHandle>(
        "assets/models/cube.obj", shader);

    const CHEngine::LoadedModel* data = rm.GetModel(model);
    if (!data || data->meshes.empty())
        return;

    // Создать сущность и скопировать меши (GPU-буферы разделяются через MeshLoader)
    auto entity = scene.CreateEntity("Cube");
    entity.PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mc) {
        mc.Meshes     = data->meshes;
        mc.SourcePath = "assets/models/cube.obj";
    });

    // Настроить трансформ
    entity.PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& t) {
        t.Position = {0.0f, 0.0f, -3.0f};
        t.Scale    = {1.0f, 1.0f, 1.0f};
    });
}
```
