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

### Шейдеры

```cpp
ShaderHandle shader = RenderFacade::CreateShader("assets/shaders/pbr.glsl");
RenderFacade::DestroyShader(shader);
```

**Формат glsl-файла** (единый файл, разделённый тегами):
```glsl
#type vertex
#version 410 core
layout(location = 0) in vec3 a_Position;
// ...

#type fragment
#version 410 core
out vec4 FragColor;
// ...
```

**Горячая перезагрузка шейдеров**: движок опрашивает файлы шейдеров каждые 0.5 секунды.  
При изменении файла шейдер автоматически перекомпилируется.

### Вершинные массивы (VAO)

```cpp
// Описание лейаута вершин
BufferLayout layout = {
    { ShaderDataType::Float3, "a_Position" },
    { ShaderDataType::Float3, "a_Normal"   },
    { ShaderDataType::Float2, "a_TexCoord" },
};

// Создать вершинный буфер и индексный буфер
VertexBufferHandle vb = RenderFacade::CreateVertexBuffer(vertices.data(), vertices.size() * sizeof(Vertex));
IndexBufferHandle  ib = RenderFacade::CreateIndexBuffer(indices.data(), indices.size());

// Собрать VAO
VertexArrayHandle vao = RenderFacade::CreateVertexArray(layout, vb, ib);

// После использования
RenderFacade::DestroyVertexArray(vao);
```

### Текстуры

```cpp
TextureHandle albedo = RenderFacade::CreateTexture("assets/textures/albedo.png");
// Привязать к шейдеру
shader->SetTexture("u_Albedo", albedo, 0);

RenderFacade::DestroyTexture(albedo);
```

### Фреймбуферы

```cpp
FramebufferSpec spec;
spec.Width      = 1280;
spec.Height     = 720;
spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };

FramebufferHandle fb = RenderFacade::CreateFramebuffer(spec);

// Рендерить в фреймбуфер
fb->Bind();
// ... Submit() ...
fb->Unbind();

// Получить текстуру (для ImGui::Image, постобработки, etc.)
TextureHandle colorAttachment = fb->GetColorAttachment(0);
```

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

### OBJ

```cpp
std::vector<Mesh> meshes = ModelLoader::Load("assets/models/scene.obj");
entity.GetComponent<MeshComponent>().Meshes = meshes;
```

### GLTF/GLB

```cpp
std::vector<Mesh> meshes = ModelLoader::Load("assets/models/character.gltf");
// Поддерживаются: меши, материалы (PBR), текстуры, UV-развёртка
```

`ModelLoader` автоматически определяет формат по расширению файла.

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
