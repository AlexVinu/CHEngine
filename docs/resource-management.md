# Управление ресурсами (ResourceManager)

## Обзор

`ResourceManager` — централизованная система загрузки и кэширования ресурсов движка.
Все шейдеры, текстуры и модели должны загружаться через неё — это обеспечивает кэширование,
корректное время жизни и единую точку учёта.

Доступ — через синглтон:

```cpp
#include <CHEngine/ResourceManager/ResourceManager.h>

auto& rm = CHEngine::ResourceManager::Instance();
```

Синглтон инициализируется при первом обращении. Обращаться к нему допустимо только **после**
инициализации рендерера (`RenderFacade::InitRenderer`).

---

## API

### Загрузка ресурсов

```cpp
// Шейдер (имя + путь к .slang файлу)
ShaderHandle shader = rm.Load<ShaderHandle>("Mesh", "shaders/mesh.slang");

// Текстура (путь к файлу)
TextureHandle texture = rm.Load<TextureHandle>("assets/textures/diffuse.png");

// 3D-модель (путь к файлу + шейдер для материалов)
ModelHandle model = rm.Load<ModelHandle>("assets/models/cube.obj", shader);

// GPU-буферы меша (вершины + индексы)
MeshHandle mesh = rm.Load<MeshHandle>(vertices, indices);
```

Повторный вызов `Load` с тем же аргументом возвращает закэшированный хэндл — GPU-ресурс не создаётся заново.

### Выгрузка ресурсов

```cpp
rm.Unload(shader);     // ShaderHandle
rm.Unload(texture);    // TextureHandle
rm.Unload(model);      // ModelHandle
rm.Unload(meshHandle); // MeshHandle (уменьшает refcount)
```

### Доступ к данным модели

После загрузки можно получить `LoadedModel` — структуру с мешами:

```cpp
ModelHandle handle = rm.Load<ModelHandle>("assets/cube.obj", meshShader);
const LoadedModel* data = rm.GetModel(handle);
if (data && !data->meshes.empty())
{
    // Копируем меши в сцену (Mesh copy-ctor AddRef'ит GPU-буферы через MeshLoader)
    std::vector<Mesh> sceneMeshes = data->meshes;
    entity.GetComponent<MeshComponent>().Meshes = std::move(sceneMeshes);
}
```

---

## Архитектура лоадеров

ResourceManager хранит массив лоадеров, индексированный по типу ресурса (`ELoaderResourceType`).
Все лоадеры невидимы пользователю — доступ только через шаблонный `Load<T>`/`Unload<T>`.

```
ResourceManager
├── ShaderLoader   — кэш шейдеров (bimap путь ↔ ShaderHandle)
├── TextureLoader  — кэш текстур  (bimap путь ↔ TextureHandle)
└── ModelLoader    — кэш моделей  (bimap путь ↔ ModelHandle, HandlePool<LoadedModel>)
```

`MeshLoader` управляется отдельно — это контент-адресуемый кэш GPU-буферов для геометрии.

### ShaderLoader

Кэширует шейдеры по пути к файлу. При повторном запросе возвращает уже загруженный хэндл.

Под капотом вызывает `RenderFacade::CreateShaderFromFile(name, path)`.

### TextureLoader

Кэширует текстуры по пути к файлу.

> **Важно:** текстуры, созданные из сырых пикселей (при загрузке OBJ/GLTF моделей),
> **не** проходят через TextureLoader и управляются напрямую через Material/MaterialInstance.

### ModelLoader

Загружает OBJ и GLTF/GLB файлы. Результат — `LoadedModel` с вектором `Mesh`.
Кэширование по пути: повторная загрузка того же файла возвращает уже существующий хэндл.

Поддерживаемые форматы: `.obj`, `.gltf`, `.glb`.

---

## MeshLoader — GPU-буферы меша

`MeshLoader` — отдельный синглтон для совместного использования GPU-буферов одинаковой геометрии.

```cpp
MeshLoader& ml = MeshLoader::Instance();
```

Ключевые свойства:
- **Контент-адресуемость**: одинаковые `(vertices, indices)` → один VBO/IBO
- **Refcount**: буферы освобождаются только когда последний `Mesh` с этим хэндлом уничтожен
- **Автоматическое управление**: `Mesh::Build()`, copy/move-конструкторы и деструктор вызывают `AddRef`/`Release` автоматически

```cpp
// Пользователь работает с Mesh — MeshLoader невидим:
Mesh mesh;
mesh.Build(vertices, indices);  // GetOrCreate под капотом

Mesh copy = mesh;               // AddRef GPU-буфера, не дублирует данные
```

---

## Время жизни ресурсов

| Тип | Создание | Удаление |
|-----|----------|----------|
| Шейдер (из файла) | `rm.Load<ShaderHandle>(...)` | `rm.Unload(handle)` |
| Текстура (из файла) | `rm.Load<TextureHandle>(...)` | `rm.Unload(handle)` |
| Текстура (из пикселей) | `RenderFacade::CreateTexture(...)` | `RenderFacade::DestroyTexture(handle)` |
| Модель | `rm.Load<ModelHandle>(...)` | `rm.Unload(handle)` *(освобождает LoadedModel + Mesh'и)* |
| GPU-буфер меша | `Mesh::Build()` (через MeshLoader) | Автоматически при удалении последнего Mesh |

> Если шейдер или текстура загружены через ResourceManager — вызывать
> `RenderFacade::DestroyShader/DestroyTexture` напрямую не нужно.

---

## Пример: загрузка и размещение модели в сцене

```cpp
#include <CHEngine/ResourceManager/ResourceManager.h>

void PlaceModelInScene(CHEngine::Scene& scene, const std::string& path)
{
    auto& rm = CHEngine::ResourceManager::Instance();
    CHEngine::ShaderHandle meshShader = CHEngine::RenderFacade::GetDefaultMeshShader();

    // Загружаем модель (или получаем из кэша)
    CHEngine::ModelHandle handle = rm.Load<CHEngine::ModelHandle>(path, meshShader);
    const CHEngine::LoadedModel* model = rm.GetModel(handle);
    if (!model || model->meshes.empty())
    {
        CHE_CORE_ERROR("Failed to load model: {}", path);
        return;
    }

    // Копируем меши (GPU-буферы разделяются через MeshLoader)
    std::vector<CHEngine::Mesh> meshes = model->meshes;

    // Создаём сущность
    auto entity = scene.CreateEntity(model->name);
    entity.PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mc) {
        mc.Meshes     = std::move(meshes);
        mc.SourcePath = path;
    });
}
```
