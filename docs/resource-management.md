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

После загрузки можно получить `LoadedModel` — структуру с одним `MeshRef` (содержащим все субмеши):

```cpp
ModelHandle handle = rm.Load<ModelHandle>("assets/cube.obj", meshShader);
const LoadedModel* data = rm.GetModel(handle);
if (data && data->mesh.IsValid())
{
    // MeshRef copy — AddRef'ит общий GpuRecord через MeshLoader
    entity.GetComponent<MeshComponent>().Mesh = data->mesh;
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

Загружает OBJ и GLTF/GLB файлы. Результат — `LoadedModel` с одним `MeshRef`.
Все примитивы/шейпы объединяются в единый VB+IB; каждый материальный бакет становится отдельным субмешем.
Кэширование по пути: повторная загрузка того же файла возвращает уже существующий хэндл.

Поддерживаемые форматы: `.obj`, `.gltf`, `.glb`.

---

## MeshLoader — GPU-буферы меша

`MeshLoader` — контент-адресуемый кэш GPU-буферов меша. Доступ через `ResourceManager`:

```cpp
MeshLoader* ml = Application::Get().Resources().GetMeshLoader();
```

Ключевые свойства:
- **Контент-адресуемость**: одинаковые `(vertices, indices)` → один `MeshGpuRecord` (VB/IB)
- **Субмеши**: `MeshGpuRecord` хранит `std::vector<SubMesh>` с диапазонами индексов
- **Refcount**: `MeshGpuRecord` удаляется только когда последний `Mesh`, ссылающийся на него, уничтожен
- **Независимые материалы**: каждый `Mesh` хранит свой вектор материалов (per-submesh), не влияя на shared GpuRecord

```cpp
// Краткая форма: один субмеш, один материал
MeshHandle h = ml->GetOrCreate(vertices, indices, mat);

// Полная форма: явные субмеши
std::vector<SubMesh> subs = {{ /*indexCount=*/600, /*startIndex=*/0, /*baseVertex=*/0 }};
MeshHandle h = ml->GetOrCreate(vertices, indices, subs, { mat });

// MeshRef управляет временем жизни автоматически
MeshRef ref{ h };
MeshRef copy = ref;  // AddRef GpuRecord, независимые материалы

// Заменить материал субмеша (не затрагивает GpuRecord и другие Mesh)
ml->SetMaterial(h, /*submeshIndex=*/0, newMat);

// Обновить UV для субмеша (uvs.size() == кол-во уникальных вершин субмеша)
ml->UpdateVertexUVs(h, /*submeshIndex=*/0, uvSpan);
```

---

## Время жизни ресурсов

| Тип | Создание | Удаление |
|-----|----------|----------|
| Шейдер (из файла) | `rm.Load<ShaderHandle>(...)` | `rm.Unload(handle)` |
| Текстура (из файла) | `rm.Load<TextureHandle>(...)` | `rm.Unload(handle)` |
| Текстура (из пикселей) | `RenderFacade::CreateTexture(...)` | `RenderFacade::DestroyTexture(handle)` |
| Модель | `rm.Load<ModelHandle>(...)` | `rm.Unload(handle)` *(освобождает LoadedModel + Mesh'и)* |
| GPU-буфер меша | `MeshLoader::GetOrCreate(...)` | Автоматически при удалении последнего MeshRef |

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
    if (!model || !model->mesh.IsValid())
    {
        CHE_CORE_ERROR("Failed to load model: {}", path);
        return;
    }

    // Создаём сущность
    auto entity = scene.CreateEntity(model->name);
    entity.PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mc) {
        mc.Mesh       = model->mesh;  // MeshRef copy — AddRef GpuRecord
        mc.SourcePath = path;
    });
}
```
