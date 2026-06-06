# Рендеринг

## Обзор

Рендеринг организован так:

```
RenderSubsystem (Application::Get().Render())
     │  владеет фабрикой, frame graph, дефолтными шейдерами, scene-камерой
     │
IRenderGraph / IFrameGraphBackend  +  IRenderFactory (интерфейсы из Core)
     │
Конкретный модуль: RendererOGL / RendererMetal / RendererVulkan
```

Рендеринг **управляется frame graph'ом**: пользователь не вызывает `Submit` вручную.
`RenderSystem` (фаза `Presentation`) сам обходит сущности с `MeshComponent` и
наполняет рендер-граф. Прикладному коду обычно достаточно создавать сущности.

## RenderSubsystem — API

Доступ — через `Application::Get().Render()` (всегда валиден после старта).

### Кадр и frame graph

Эти вызовы делает `Application::Run` — пользователю трогать их не нужно:

```cpp
RenderSubsystem& r = Application::Get().Render();
r.BeginFrame();
r.BeginFrameGraph();
//  ... Layer::OnUpdate → World::Update → RenderSystem строит граф ...
r.EndFrameGraph();
r.EndFrame();
```

### Scene-камера

```cpp
// Камера текущего кадра передаётся как заполненный UBO
UBOCamera cam = /* ... view-projection, позиция ... */;
Application::Get().Render().SetSceneCamera(cam);
```

Обычно камеру выставляет редактор/слой из активной `EditorCamera` либо `RenderSystem`
из `CameraComponent` с `Primary = true`.

## Управление ресурсами

Все ресурсы загружаются через `ResourceManager` (`Application::Get().Resources()`).
Подробнее — в [resource-management.md](resource-management.md).

### Шейдеры

Шейдеры написаны исключительно на [Slang](https://shader-slang.org/) — единый источник компилируется в GLSL 4.1 (OpenGL), MSL (Metal) или SPIR-V (Vulkan) через `SlangBackend`.  
Отдельных `.vert`/`.frag`/`.metal` файлов нет — только `.slang`.  
Все файлы находятся в `Sandbox/shaders/`.

```cpp
ResourceManager& rm = Application::Get().Resources();

// Загрузить шейдер (кэшируется по пути)
ShaderHandle shader = rm.Load<ShaderHandle>("Mesh", "shaders/mesh.slang");

// Выгрузить
rm.Unload(shader);
```

**Структура Slang-шейдера:**
```slang
import common;  // CameraUBO, ObjectUBO, MaterialUBO, LightingUBO

ConstantBuffer<CameraUBO> camera;
ConstantBuffer<ObjectUBO> object;

struct VSIn {
    [[vk::location(0)]] float3 Position  : POSITION;
    [[vk::location(1)]] float3 Normal    : NORMAL;
    [[vk::location(2)]] float2 TexCoords : TEXCOORD0;
    [[vk::location(3)]] float3 Color     : COLOR;
};

[shader("vertex")]
VSOut vertMain(VSIn input) { ... }

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target { ... }
```

**Горячая перезагрузка шейдеров**: движок опрашивает файлы каждые 0.5 секунды.
При изменении `.slang` файла шейдер автоматически перекомпилируется.

### Текстуры

```cpp
ResourceManager& rm = Application::Get().Resources();

// Из файла (кэшируется по пути)
TextureHandle tex = rm.Load<TextureHandle>("textures/diffuse.png");
rm.Unload(tex);

// Из сырых пикселей (минуя кэш — например при загрузке OBJ/GLTF)
RenderSubsystem& r = Application::Get().Render();
TextureHandle tex2 = r.CreateTexture(pixels, width, height, channels);
r.DestroyTexture(tex2);
```

### GPU-буферы меша

Меши создаются через `MeshLoader::GetOrCreate` — напрямую или через загрузчики моделей.
`MeshLoader` кэширует идентичную геометрию и считает ссылки.

```cpp
MeshLoader* ml = Application::Get().Resources().GetMeshLoader();

// Краткая форма: один субмеш, один материал
MeshHandle h = ml->GetOrCreate(vertices, indices, mat);

// Полная форма: несколько субмешей с явными диапазонами
// materials.size() должен совпадать с subMeshes.size()
MeshHandle h = ml->GetOrCreate(vertices, indices, subMeshes, materials);

// MeshRef автоматически AddRef/Release при копировании/уничтожении
MeshRef ref{ h };
```

Стандартный лейаут вершин (11 float на вершину, stride = 44 байта):

| Атрибут | Offset | Формат |
|---------|--------|--------|
| Position | 0 | Float3 |
| Normal | 12 | Float3 |
| TexCoords | 24 | Float2 |
| Color | 32 | Float3 |

## Материалы

`Material` — это **PBR-описание ассета** (шейдер + набор PBR-полей и карт), а не
произвольный uniform-словарь. `MaterialInstance` — экземпляр с переопределениями
поверх базового материала; именно он хранится в `Mesh` (по одному на субмеш).

```cpp
// Базовый материал
auto base = MakeRef<Material>(shaderHandle);
base->Roughness  = 0.4f;
base->Metallic   = 0.0f;
base->DiffuseMap = albedoTexture;       // TextureHandle

// Экземпляр с переопределениями
Ref<MaterialInstance> inst = MaterialInstance::FromBase(base);
inst->Metallic = 0.8f;                  // переопределяет базовое значение

// Назначить материал субмешу меша
meshLoader->SetMaterial(meshHandle, /*submeshIndex=*/0, inst);
```

Поля: `DiffuseMap`, `SpecularMap`, `NormalMap`, `ORMmap`, `EmissiveMap` (+ их пути),
`Shininess`, `SpecularScale`, `Roughness`, `Metallic`, `AO`, `EmissiveColor`.
Материал заполняет `UBOMaterial` через `FillUBOMaterial`.

## UBO (Uniform Buffer Object)

Общие UBO описаны в `Sandbox/shaders/common.slang`: `CameraUBO`, `ObjectUBO`,
`MaterialUBO`, `LightingUBO`. Камера заполняется один раз за кадр через
`RenderSubsystem::SetSceneCamera(UBOCamera)` — пользователю не нужно вручную
передавать матрицу вида/проекции в каждый сабмит.

```slang
import common;
ConstantBuffer<CameraUBO>   camera;     // [[buffer(0)]] на Metal
ConstantBuffer<ObjectUBO>   object;     // [[buffer(1)]]
ConstantBuffer<LightingUBO> lighting;   // [[buffer(2)]]
ConstantBuffer<MaterialUBO> material;   // [[buffer(3)]]
```

## RenderSystem — встроенная система рендеринга

`RenderSystem` (фаза Presentation) автоматически рендерит все сущности с `MeshComponent`:

```
ForEach<MeshComponent, TransformComponent, VisibilityComponent>:
  if Visible && Mesh.IsValid():
    rec = MeshLoader::GetGpuRecord(Mesh.Handle())
    for s in 0..Mesh->GetSubMeshCount():
      item.vertexBuffer = rec->vb
      item.indexBuffer  = rec->ib
      item.indexCount   = rec->subMeshes[s].indexCount
      item.firstIndex   = rec->subMeshes[s].startIndex
      item.baseVertex   = rec->subMeshes[s].baseVertex
      item.material     = Mesh->GetMaterial(s)
      DrawList.push(item)
```

Чтобы отключить объект:

```cpp
scene.TryGetEntity(handle)->GetComponent<VisibilityComponent>().Visible = false;
```

## Загрузка моделей

Модели загружаются через `ResourceManager`. `ModelLoader` не вызывается напрямую.

```cpp
ResourceManager& rm = Application::Get().Resources();
ShaderHandle meshShader = Application::Get().Render().GetDefaultMeshShader();

// Загрузить OBJ или GLTF/GLB (формат определяется по расширению)
ModelHandle handle = rm.Load<ModelHandle>("assets/models/scene.obj", meshShader);

const LoadedModel* model = rm.GetModel(handle);
if (model && model->mesh.IsValid())
{
    // MeshRef copy — AddRef'ит общий GpuRecord через MeshLoader
    scene.TryGetEntity(h)->GetComponent<MeshComponent>().Mesh = model->mesh;
}
```

Все примитивы/шейпы модели объединяются в **один** `MeshRef` с несколькими субмешами.
Каждый субмеш имеет свой диапазон индексов и независимый материал.

Поддерживаемые форматы: `.obj`, `.gltf`, `.glb`.  
Поддерживаются: меши, материалы (PBR/диффуз/specular), текстуры, UV-развёртка.

> Повторная загрузка того же пути возвращает кэшированный `ModelHandle` без I/O.

## Выбор рендерера

| Рендерер | Платформа | Примечание |
|----------|-----------|-----------|
| OpenGL 4.1 | Windows, macOS, Linux | Универсальный, рекомендуется для разработки |
| Metal | macOS, iOS | Нативный Apple, лучшая производительность на Mac |
| Vulkan | Windows, Linux | Максимальный контроль, требует настройки |

Рендерер выбирается при старте:
```bash
./App --renderer=opengl
./App --renderer=metal
./App --renderer=vulkan
```

или сохраняется в `engine.json`.

## Отладка рендеринга

Базовая проверка через ImGui:

```cpp
void OnUIUpdate() override {
    ImGui::Begin("Renderer Info");
    ImGui::Text("API: %d", (int)Application::Get().GetRenderAPIType());
    // Превью результата сцены (выходная текстура viewport'а)
    uint64_t texId = Application::Get().Render().GetViewportColorTexID();
    ImGui::Image((ImTextureID)texId, ImVec2(320, 180));
    ImGui::End();
}
```

> В редакторе сцена рендерится в offscreen-текстуру (`RenderSubsystem::GetViewportOutputTexture`),
> которую `EditorViewport` показывает через `ImGui::Image`. В Player та же текстура
> блитится в backbuffer через `IRenderFactory::PresentToBackbuffer`.
