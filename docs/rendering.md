# Рендеринг

## Обзор

Рендеринг организован в трёх слоях:

```
RenderSubsystem (RAII-объект, принадлежит Application)
     │
BasicFrameGraphFrontend (IRenderGraph: AddPass → Compile → Execute)
     │
IFrameGraphBackend (FrameGraphBackendOGL / FrameGraphBackendMTL)
     │
Конкретный модуль: RendererOGL / RendererMetal / RendererVulkan
```

## RenderSubsystem — основной API

Доступ через `Application::Get().Render()`. Является RAII-владельцем `IRenderFactory*`.

### Управление кадром

```cpp
auto& render = Application::Get().Render();

render.BeginFrame();
render.BeginFrameGraph();    // graph->Reset()

// ... OnUpdate слоёв (RenderSystem добавляет PassDesc'ы в граф) ...

render.EndFrameGraph();      // Compile() + Execute(backend) → нативные команды
render.EndFrame();
```

### Шейдеры

```cpp
// Загрузить из файла (регистрируется в hot-reload)
ShaderHandle sh = render.CreateShaderFromFile("Mesh", "shaders/mesh.slang");

// Загрузить из строки
ShaderHandle sh = render.CreateShader(slangSourceCode, "vertMain", "fragMain");

render.DestroyShader(sh);
```

### Текстуры

```cpp
TextureHandle tex = render.CreateTextureFromFile("textures/albedo.png");
TextureHandle tex = render.CreateTexture(pixels, width, height, channels);
render.DestroyTexture(tex);
```

### Viewport

```cpp
render.SetViewportSize(1280, 720);
render.SetViewportOutputTexture(hdrTexHandle);  // что показывать в ImGui::Image
uint64_t nativeId = render.GetViewportColorTexID();
```

### Шейдеры по умолчанию

```cpp
render.SetDefaultMeshShader(shHandle);
ShaderHandle sh = render.GetDefaultMeshShader();
```

### Горячая перезагрузка шейдеров

```cpp
// Вызывается из Application::Run() раз в 0.5с:
render.PollShaders();   // FileWatcher → ReloadShader → PSO invalidation

// Принудительно:
render.ReloadShader(shHandle);
```

### Доступ к фабрике

```cpp
IRenderFactory* factory = render.GetRenderFactory();

// Создать GPU-буфер напрямую:
BufferHandle vb = factory->CreateBuffer(size, BufferUsage::Vertex, MemoryType::CpuToGpu, data, "VB");
factory->UpdateBuffer(vb, newData, offset);
factory->Delete(vb);
```

## Декларативный фрейм-граф

Вместо immediate-mode (`Submit/Clear`) движок использует декларативный граф. `RenderSystem` строит `PassDesc` и добавляет в граф каждый кадр. Backend исполняет граф после `EndFrameGraph()`.

### PassDesc — описание одного рендер-пасса

```cpp
PassDesc pass;
pass.Name             = "MainColor";
pass.Pipeline         = m_MeshPipeline;                  // PipelineHandle
pass.ColorAttachments = { m_HDRTarget };                 // TextureHandle[]
pass.DepthAttachment  = m_DepthTarget;
pass.ColorLoadOp      = ELoadOp::Clear;
pass.ClearColor       = { 0.18f, 0.18f, 0.20f, 1.0f };
pass.ViewportWidth    = render.GetViewportWidth();
pass.ViewportHeight   = render.GetViewportHeight();

// Per-pass UBO: camera + lighting (слоты 0 и 2)
pass.Uniforms = {
    { m_CameraUBO,   /*slot*/ 0, 0, 0 },
    { m_LightingUBO, /*slot*/ 2, 0, 0 },
};

// Per-draw: меш + per-object UBO
pass.Draws = BuildDrawList(scene);   // vector<DrawDesc>

// Зависимости (для топосорта)
pass.Writes = { m_HDRTarget };

Application::Get().Render().GetFrameGraph().AddPass(std::move(pass));
```

### DrawDesc — одна draw-команда

```cpp
DrawDesc draw;
draw.VertexBuffer  = mesh.GetVertexBuffer();
draw.IndexBuffer   = mesh.GetIndexBuffer();
draw.IdxFormat     = IndexFormat::Uint32;
draw.IndexCount    = mesh.GetIndexCount();
draw.InstanceCount = 1;

// Per-draw UBO (object transform, ring buffer offset)
draw.Uniforms = { { m_ObjectUBO, /*slot*/ 1, byteOffset, alignedSize } };

pass.Draws.push_back(std::move(draw));
```

### PipelineHandle

```cpp
PipelineDesc pd;
pd.Shader       = defaultShader;
pd.VertexLayout = GetStandardMeshLayout();   // pos3+normal3+uv2+color3, stride 44
pd.Depth.Test   = true;
pd.Depth.Write  = true;
pd.Raster.Cull  = CullMode::Back;

PipelineHandle pipeline = factory->CreatePipeline(pd);
```

При `ReloadShader` зависимые Pipeline'ы перестраиваются автоматически — handle остаётся валидным.

## Шейдеры (Slang)

Все шейдеры написаны на [Slang](https://shader-slang.org/) и компилируются в GLSL / MSL / SPIR-V через `SlangBackend`.  
Файлы: `Sandbox/shaders/*.slang`.

```slang
import common;  // CameraUBO, ObjectUBO, LightingUBO, MaterialUBO

ConstantBuffer<CameraUBO> camera;    // slot 0
ConstantBuffer<ObjectUBO> object;    // slot 1

[shader("vertex")]
VSOut vertMain(VSIn input) { ... }

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target { ... }
```

Общие UBO-структуры определены в `Sandbox/shaders/common.slang` и `Core/Interfaces/Render/UniformBlocks.h`.

### Стандартный лейаут вершин (stride = 44 байта)

| Атрибут | Offset | Формат |
|---------|--------|--------|
| Position | 0 | Float3 |
| Normal | 12 | Float3 |
| TexCoords | 24 | Float2 |
| Color | 32 | Float3 |

## Материалы

```cpp
// Material привязывает шейдер + UniformBinding
auto mat = MakeRef<Material>(shaderHandle);

// MaterialInstance — конкретные значения (UBOMaterial)
auto inst = MakeRef<MaterialInstance>(mat);
inst->SetColor({1, 0.5f, 0, 1});
```

## Загрузка моделей

```cpp
ShaderHandle meshShader = Application::Get().Render().GetDefaultMeshShader();
auto& rm = Application::Get().Resources();

ModelHandle handle = rm.Load<ModelHandle>("assets/models/scene.obj", meshShader);

const LoadedModel* model = rm.GetModel(handle);
if (model) {
    entity.GetComponent<MeshComponent>().Meshes = model->meshes;
}
```

Поддерживаемые форматы: `.obj`, `.gltf`, `.glb`.

## Выбор рендерера

| Рендерер | Платформа | Примечание |
|----------|-----------|-----------|
| OpenGL 4.1 | Windows, macOS, Linux | Универсальный |
| Metal 3.2 | macOS | Нативный Apple, предпочтительнее на Mac |
| Vulkan | Windows, Linux | Не собирается по умолчанию |

```bash
./Sandbox --renderer=opengl
./Sandbox --renderer=metal
```

Или через `engine.json` (`renderer_pending`).

## RenderSystem flow

`RenderSystem` (Presentation, prio 10) выполняет за кадр:

1. `ResolveCamera` — находит Primary `CameraComponent` или `World::m_Camera`, заполняет `UBOCamera`
2. `CollectLighting` — собирает `LightComponent` сущностей в `UBOLighting`
3. `EnsureGPUResources` — лениво создаёт/пересоздаёт при resize: `m_HDRTarget`, `m_DepthTarget`, `m_MeshPipeline`, `m_CameraUBO`, `m_LightingUBO`
4. `EnsureObjectUBO` — растит ring buffer при нужде
5. Заливает Camera/Lighting UBO через `factory->UpdateBuffer`
6. Собирает `vector<DrawDesc>` из ECS (`MeshComponent` + `TransformComponent` + `VisibilityComponent`)
7. Строит `PassDesc MainColorPass` и добавляет в `GetFrameGraph()`
