# Быстрый старт

## Требования

- CMake 3.5+
- C++20 компилятор (Clang, GCC, MSVC)
- macOS: Xcode Command Line Tools (для Metal), или просто clang
- Windows: Visual Studio 2019+ или MinGW
- Linux: GCC 9+ или Clang 10+

## Сборка

```bash
git clone https://github.com/AlexVinu/CHEngine
cd CHEngine

# macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_METAL=ON -DCHE_BUILD_PHYSICS=ON
cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)

# Windows
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_PHYSICS=ON
cmake --build build --config Debug
```

### CMake-опции

| Опция | По умолчанию | Описание |
|-------|--------------|----------|
| `CHE_BUILD_SANDBOX` | ON | Собрать демо-приложение |
| `CHE_BUILD_OPENGL` | ON | Собрать модуль OpenGL |
| `CHE_BUILD_VULKAN` | OFF | Собрать модуль Vulkan |
| `CHE_BUILD_METAL` | ON (macOS) | Собрать модуль Metal |
| `CHE_BUILD_PHYSICS` | ON | Собрать модуль PhysX (все платформы; на macOS — через o3de-форк) |

### Выходные файлы

```
bin/<Config>-<platform>-<arch>/Sandbox/
├── Sandbox              # Исполняемый файл
├── libCHEngine.dylib
├── libRendererOGL.dylib
├── libWindowGLFW.dylib
├── libImGuiOGL.dylib
└── libPhysicsPhysX.dylib   # если CHE_BUILD_PHYSICS=ON
```

Все `.dylib` лежат рядом с исполняемым файлом (не в подпапке `lib/`).

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

using namespace CHEngine;

class MyLayer : public Layer {
public:
    MyLayer() : Layer("MyLayer") {}

    void OnAttach() override {
        // Инициализация: загрузка ресурсов, создание сцены
        m_World.SetState(WorldState::Simulating);
    }

    void OnUpdate(Timestep dt) override {
        m_LastDt = dt;
        m_World.Update(dt);   // прогоняет фазы Simulation + Presentation
    }

    void OnImGuiRender() override {
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", m_LastDt > 0.0f ? 1.0f / m_LastDt : 0.0f);
        ImGui::End();
    }

private:
    WorldsList m_Worlds;
    World      m_World{ &m_Worlds };
    float      m_LastDt = 0.0f;
};

class MyApp : public Application {
public:
    MyApp(const ApplicationConfig& config) : Application(config) {
        PushLayer(new MyLayer());
    }
};

Application* CHEngine::CreateApplication(const ApplicationConfig& config) {
    return new MyApp(config);
}
```

> `Application` не владеет дефолтным `World`. Слой создаёт и обновляет свой `World`
> (так же, как редактор Sandbox хранит `World` внутри сессии `EditorWorldContext`).

## Загрузка модели и рендеринг

```cpp
#include <CHEngine.h>

using namespace CHEngine;

void OnAttach() override {
    Scene&           scene = *m_World.GetSceneRef();
    ResourceManager& rm    = Application::Get().Resources();

    // Загрузить шейдер и модель (кэшируются — повторный вызов вернёт тот же хэндл)
    ShaderHandle shader = rm.Load<ShaderHandle>("Mesh", "shaders/mesh.slang");
    ModelHandle  model  = rm.Load<ModelHandle>("assets/models/cube.obj", shader);

    const LoadedModel* data = rm.GetModel(model);
    if (!data || !data->mesh.IsValid())
        return;

    // CreateEntity возвращает EntityHandle; работаем через Entity-обёртку
    EntityHandle h = scene.CreateEntity("Cube");
    Entity*      e = scene.TryGetEntity(h);

    // MeshComponent хранит один MeshRef (GPU-буферы разделяются через MeshLoader)
    e->AddComponent<MeshComponent>(MeshComponent{
        data->mesh,
        scene.InternString("assets/models/cube.obj") });

    // Трансформ: Position/Rotation(Euler, градусы)/Scale внутри ObjectTransform
    auto& tc = e->AddComponent<TransformComponent>();
    tc.ObjectTransform.Position = { 0.0f, 0.0f, -3.0f };
    tc.MarkDirty();
}
```

> `SourcePath` в `MeshComponent` — это `StringID` из строкового пула сцены
> (`Scene::InternString` / `GetString`), а не `std::string`.
