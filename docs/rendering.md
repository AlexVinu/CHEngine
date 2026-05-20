# Рендеринг

## Обзор

Рендеринг организован в трёх слоях:

```
RenderFacade (статический API для пользователя)
     │
RenderResourceManager (пулы ресурсов, шейдеры, UBO)
     │
IRenderer / IRenderApi (интерфейсы из Core)
     │
Конкретный модуль: RendererOGL / RendererMetal / RendererVulkan
```

## RenderFacade — основной API

Все вызовы рендеринга проходят через статический класс `RenderFacade`.

### Управление кадром

```cpp
RenderFacade::BeginFrame();
RenderFacade::Clear();          // Очистить буфер цвета и глубины
// ... рендеринг ...
RenderFacade::EndFrame();       // Финализировать кадр
```

### Сцена и камера

```cpp
// Установить камеру для текущего кадра (заполняет UBO)
RenderFacade::SetSceneCamera(viewMatrix, projMatrix, cameraPosition);

RenderFacade::BeginScene();
// ... Submit() вызовы ...
RenderFacade::EndScene();
```

### Отправка геометрии

```cpp
// Стандартный сабмит
RenderFacade::Submit(shaderHandle, vaoHandle, modelMatrix);

// С кастомным шейдером (переопределяет шейдер материала)
RenderFacade::Submit(customShader, vaoHandle, modelMatrix);
```

## Управление ресурсами

Все ресурсы загружаются через `ResourceManager`. Подробнее — в [resource-management.md](resource-management.md).

### Шейдеры

Шейдеры написаны исключительно на [Slang](https://shader-slang.org/) — единый источник компилируется в GLSL 4.1 (OpenGL), MSL (Metal) или SPIR-V (Vulkan) через `SlangBackend`.  
Отдельных `.vert`/`.frag`/`.metal` файлов нет — только `.slang`.  
Все файлы находятся в `Sandbox/shaders/`.

```cpp
// Загрузить шейдер (кэшируется по пути)
ShaderHandle shader = ResourceManager::Instance().Load<ShaderHandle>(
    "Mesh", "shaders/mesh.slang");

// Выгрузить
ResourceManager::Instance().Unload(shader);
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
// Из файла (кэшируется по пути)
TextureHandle tex = ResourceManager::Instance().Load<TextureHandle>("textures/diffuse.png");
ResourceManager::Instance().Unload(tex);

// Из сырых пикселей (минуя кэш — например при загрузке OBJ/GLTF)
TextureHandle tex = RenderFacade::CreateTexture(pixels, width, height, channels);
RenderFacade::DestroyTexture(tex);
```

### GPU-буферы меша

Меши не создаются вручную — они строятся через `Mesh::Build()`.
`MeshLoader` кэширует идентичную геометрию и считает ссылки:

```cpp
Mesh mesh;
mesh.Build(vertices, indices); // Загружает в GPU через MeshLoader::GetOrCreate
// Деструктор Mesh автоматически вызывает MeshLoader::Release
```

Стандартный лейаут вершин (11 float на вершину, stride = 44 байта):

| Атрибут | Offset | Формат |
|---------|--------|--------|
| Position | 0 | Float3 |
| Normal | 12 | Float3 |
| TexCoords | 24 | Float2 |
| Color | 32 | Float3 |

## Материалы

`Material` привязывает шейдер к набору uniform-переменных и текстур.

```cpp
Material mat(shaderHandle);
mat.SetVec4("u_Color",     {1.0f, 0.5f, 0.0f, 1.0f});
mat.SetFloat("u_Metallic", 0.8f);
mat.SetTexture("u_Albedo", albedoTexture);

// Применить перед сабмитом
mat.Bind();
RenderFacade::Submit(mat.GetShader(), vao, transform);
mat.Unbind();
```

## UBO (Uniform Buffer Object)

Движок автоматически заполняет UBO с данными камеры:

```glsl
// В шейдере (привязывается автоматически к точке 0):
layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
    vec3 u_CameraPosition;
};
```

Пользователю не нужно вручную передавать матрицу вида/проекции — она устанавливается через `RenderFacade::SetSceneCamera()` один раз за кадр.

## RenderSystem — встроенная система рендеринга

`RenderSystem` (фаза Presentation) автоматически рендерит все сущности с `MeshComponent`:

```
ForEach<MeshComponent, TransformComponent, VisibilityComponent>:
  if Visible:
    for Mesh in MeshComponent.Meshes:
      RenderFacade::Submit(mesh.shader, mesh.vao, transform.GetMatrix())
```

Чтобы отключить объект:

```cpp
entity.GetComponent<VisibilityComponent>().Visible = false;
```

## Загрузка моделей

Модели загружаются через `ResourceManager`. `ModelLoader` не вызывается напрямую.

```cpp
auto& rm = ResourceManager::Instance();
ShaderHandle meshShader = RenderFacade::GetDefaultMeshShader();

// Загрузить OBJ или GLTF/GLB (формат определяется по расширению)
ModelHandle handle = rm.Load<ModelHandle>("assets/models/scene.obj", meshShader);

const LoadedModel* model = rm.GetModel(handle);
if (model && !model->meshes.empty())
{
    // Копирование — GPU-буферы разделяются через MeshLoader (refcount)
    entity.GetComponent<MeshComponent>().Meshes = model->meshes;
}
```

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
void OnImGuiRender() override {
    ImGui::Begin("Renderer Info");
    ImGui::Text("API: %s", RenderFacade::GetAPIName());
    // Превью фреймбуфера
    ImGui::Image((ImTextureID)fb->GetColorAttachment(0)->GetRendererID(),
                 ImVec2(320, 180));
    ImGui::End();
}
```
