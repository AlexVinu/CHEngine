# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Правила написания кода (обязательно)

### RAII — строгое соблюдение
Все ресурсы (память, файлы, хендлы, мьютексы, потоки, GPU-объекты) **обязаны** управляться через RAII:
- Используй `std::unique_ptr` / `std::shared_ptr` вместо сырых owning-указателей
- Используй `std::lock_guard` / `std::unique_lock` — никогда не лочи/анлочи вручную
- Деструктор класса, владеющего ресурсом, должен его освобождать
- Никаких `new`/`delete` напрямую — только через RAII-обёртки или `MakeRef`/`MakeScope`
- `std::thread` — всегда `join()` или `detach()` в деструкторе, никогда не оставлять joinable
- Никаких `std::system()` с пользовательскими путями — только `std::filesystem::permissions()`
- Ресурсы GPU (текстуры, буферы, шейдеры) — только через Handle-систему движка

### Прочие обязательные практики
- Не коммитить и не пушить без явного разрешения пользователя
- Коммиты только на русском языке (человеческий стиль, не AI-слог)

## Build commands

```bash
# Configure (Windows — OpenGL + Physics)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_PHYSICS=ON

# Configure (macOS — OpenGL + Metal + Physics через o3de-форк)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCHE_BUILD_SANDBOX=ON -DCHE_BUILD_OPENGL=ON -DCHE_BUILD_METAL=ON -DCHE_BUILD_PHYSICS=ON

# Build everything
cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)

# Build отдельные таргеты
cmake --build build --target Sandbox
cmake --build build --target RendererOGL
cmake --build build --target RendererMTL
cmake --build build --target SlangBackend

# Run (cwd должен быть папка с бинарником)
cd bin/Debug-macos-x64/Sandbox
./Sandbox                      # читает engine.json
./Sandbox --renderer=metal     # принудительно Metal (рекомендуется на Mac)
./Sandbox --renderer=opengl    # принудительно OpenGL
```

Бинарники: `bin/Debug-macos-x64/<target>/`. Все `.dylib` лежат рядом с исполняемым.

CMake опции: `CHE_BUILD_SANDBOX`, `CHE_BUILD_OPENGL`, `CHE_BUILD_METAL`, `CHE_BUILD_VULKAN`, `CHE_BUILD_PHYSICS`.
**PhysX на macOS** собирается через o3de-форк (`CHE_VENDOR_DIR/PhysX/o3de_physx`), поддерживает ARM64 и Intel.

При изменении `.slang` шейдеров нужно либо пересобрать Sandbox (тогда они скопируются автоматически через POST_BUILD), либо вручную:
```bash
cp Sandbox/shaders/*.slang bin/Debug-macos-x64/Sandbox/shaders/
```

Тестирование: автотестов нет, проверка — запуск Sandbox и наблюдение за ImGui.

---

## Архитектура

### Трёхуровневая структура

```
Core/           — чистые интерфейсы + утилиты (без зависимостей от CHEngine)
CHEngine/       — основная shared-библиотека (.dylib/.dll)
Modules/        — платформенные плагины, загружаемые в рантайме
Sandbox/        — демо/редактор (приложение поверх движка)
```

`Core` экспортирует только абстрактные интерфейсы (`IRenderFactory`, `IWindowFactory`, `IImGuiFactory`, `IPhysicsFactory`) и утилиты (`Log`, `Handle<Tag>`, `Timestep`). Ноль зависимостей от CHEngine.

### Система модулей (runtime plugins)

Каждая платформенная реализация — отдельный `.dylib`/`.dll`, загружаемый через `dlopen`/`LoadLibrary`. Каждый модуль экспортирует ровно два символа:

```cpp
extern "C" IModuleFactory* CreateFactory();
extern "C" void DestroyFactory(IModuleFactory*);
```

`ModuleManager` в CHEngine загружает/выгружает модули. Тип определяется через `IModuleFactory::GetType()` → `ModuleType` (`Render`, `Window`, `ImGui`, `Physics`).

Горячая перезагрузка работает **только для ImGui-модулей** (OGL/MTL/VK). Renderer, Window, Physics — нет, т.к. владеют OS-хэндлами. Шейдеры перезагружаются отдельно через `RenderResourceManager` (polling каждые 0.5с).

### Выбор рендерера и запуск

Приоритет: `--renderer=` CLI → `engine.json` (`renderer_pending` / `renderer`) → OpenGL по умолчанию.

`engine.json` хранит `renderer_pending` (пишется при смене API из UI) и `renderer` (коммитится только после успешного старта). При краше после смены — откат к предыдущему.

**Важно для API switching:** при рестарте через `execv` аргумент `--renderer=` убирается из argv, чтобы новый процесс читал `renderer_pending` из `engine.json`. Логика в `EntryPoint.h`.

### ECS / World / SystemScheduler

Scene использует **entt** внутри. Доступ — только через `Scene`, никогда напрямую через `entt::registry`:

```cpp
EntityHandle h = scene.CreateEntity("name");
scene.TryGetComponent<TransformComponent>(h);   // T* или nullptr
scene.ForEach<MeshComponent, TransformComponent>([](EntityHandle, UUID, MeshComponent&, TransformComponent&) { ... });
```

`EntityHandle` = `Handle<EntityTag>` (index + generation, типобезопасный). Сущности также индексируются по `UUID` (boost uuid).

`World` содержит `SystemScheduler`, `DeferredOps` и `EventBus`. Фазы (после рефактора коллеги):
- `Simulation` — физика, логика
- `Presentation` — рендеринг

`WorldState` enum управляет тем, что работает:
- `Presenting` — только рендер (Edit-режим редактора)
- `Simulating` — физика + рендер (Play-режим)
- `SimulatingWithoutPresenting` — только физика (неактивные сессии)

**DeferredOps** (заменил старый CommandBuffer) — откладывает операции (destroy entity и т.п.) до конца всех фаз. Никогда не уничтожай сущности внутри `ForEach`.

**EventBus** — типизированная шина событий внутри World. Заменил часть ивент-диспатчинга CommandBuffer.

### Facade паттерн

`RenderFacade`, `PhysicsFacade`, `UIFacade` — статические классы (удалённые конструкторы). Не инстанциировать, проксируют вызовы к загруженному модулю.

### Handle-based ресурсы

Все GPU-ресурсы (шейдеры, VAO, текстуры, фреймбуферы) идентифицируются через `Handle<Tag>`:

```cpp
ShaderHandle sh = RenderFacade::CreateShaderFromFile("name", "shaders/mesh.slang");
// Handle<ShaderTag> и Handle<TextureTag> — разные типы, компилятор не перепутает
```

`Handle::IsValid()` — проверяет `index != 0xFFFFFFFF`.

### Mesh / Submesh архитектура

**Модель данных:** один `MeshGpuRecord` хранит единый VB + IB для всей модели. Отдельные части описываются массивом `SubMesh`:

```cpp
struct SubMesh {
    uint32_t indexCount  = 0;
    uint32_t startIndex  = 0;  // смещение в IB (не в байтах, в индексах)
    int32_t  baseVertex  = 0;  // прибавляется к каждому индексу при отрисовке
};
```

`Mesh` — логический объект поверх `MeshGpuRecord`: хранит хэндл записи + вектор материалов (по одному на субмеш). Буферы и диапазоны лежат в `MeshGpuRecord`, материалы — в `Mesh`.

**`MeshComponent`** (ECS) содержит **один** `MeshRef Mesh` (не вектор):
```cpp
struct MeshComponent {
    MeshRef     Mesh;
    std::string SourcePath;
};
```

**`LoadedModel`** тоже хранит один `MeshRef mesh`:
```cpp
struct LoadedModel {
    MeshRef     mesh;
    std::string name;
    bool        success = false;
    std::string error;
};
```

**GLTF / OBJ загрузчики** объединяют все примитивы/шейпы в один VB+IB; каждый материальный бакет становится отдельным `SubMesh`.

**API `MeshLoader`:**

```cpp
// Полная форма: явные диапазоны субмешей (materials.size() == subMeshes.size())
MeshHandle GetOrCreate(vertices, indices, subMeshes, materials);

// Краткая форма: один субмеш на весь IB, один материал
MeshHandle GetOrCreate(vertices, indices, mat = nullptr);

// Замена материала конкретного субмеша (не трогает GpuRecord)
SetMaterial(MeshHandle h, uint32_t submeshIndex, Ref<MaterialInstance> mat);

// Обновление UV субмеша: uvs.size() == кол-во уникальных вершин этого субмеша
UpdateVertexUVs(MeshHandle h, uint32_t submeshIndex, std::span<const glm::vec2> uvs);

// Доступ к буферам, субмешам, CPU-копии вершин
const MeshGpuRecord* GetGpuRecord(MeshHandle h) const;
```

**`DrawItem`** (передаётся в `RenderSystem`) содержит `firstIndex` и `baseVertex` — заполняются из `SubMesh` при обходе субмешей.

**RenderSystem** итерирует субмеши так:
```cpp
const MeshGpuRecord* rec = meshLoader->GetGpuRecord(meshComp.Mesh.Handle());
for (uint32_t s = 0; s < mesh->GetSubMeshCount(); ++s) {
    const SubMesh& sm = rec->subMeshes[s];
    item.indexCount  = sm.indexCount;
    item.firstIndex  = sm.startIndex;
    item.baseVertex  = sm.baseVertex;
    item.material    = mesh->GetMaterial(s);
}
```

---

## Шейдерная система (Slang)

### Slang вместо GLSL/MSL

**Все шейдеры переписаны на [Slang](https://shader-slang.org/)** — единый источник компилируется в GLSL (OpenGL), MSL (Metal) или SPIR-V (Vulkan) через `SlangBackend`.

Шейдеры живут в `Sandbox/shaders/*.slang`. При добавлении шейдера — **только один `.slang` файл** (не нужно отдельных `.vert`/`.frag`/`.metal`).

Общие типы UBO: `Sandbox/shaders/common.slang` — `CameraUBO`, `ObjectUBO`, `MaterialUBO`, `LightingUBO`.

Пример шейдера:
```slang
import common;

ConstantBuffer<CameraUBO> camera;
ConstantBuffer<ObjectUBO> object;

[shader("vertex")]
VSOut vertMain(VSIn input) { ... }

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target { ... }
```

### SlangBackend

`Modules/Rendering/Common/SlangBackend/` — общая обёртка над Slang SDK:
- `SlangBackend::GetForApi(ERenderAPI)` — получить/создать бэкенд для API
- `SlangBackend::Compile(source, vertEntry, fragEntry, path)` → `CompiledShader`
- Для Metal использует `getTargetCode()` (единый MSL для обоих стейджей)
- Для OpenGL/Vulkan использует `getEntryPointCode()` отдельно для vertex и fragment
- Профиль для Metal: `metal_3_2` (нужен для fragmentprocessing capabilities)
- Профиль для OpenGL: `glsl_410` (macOS ограничен OpenGL 4.1)
- Матрицы: `MatrixLayoutColumn = true` (совпадает с glm column-major)

### macOS OpenGL 4.1 — патчинг GLSL

macOS поддерживает только OpenGL 4.1. Slang генерирует GLSL 4.5+. В `ShaderOGL::CompileSlangProgram` применяется **`PatchGlslForGL41()`**:

1. `#version 450` → `#version 410 core`
2. Убирает `layout(row_major) buffer;` (SSBO, требует 4.30) — **НО оставляет** `layout(row_major) uniform;` (валидно с GLSL 1.40 и нужно для корректного layout матриц)
3. Убирает `layout(binding = N)` как отдельную строку перед `layout(std140) uniform Block` (требует 4.20)

**Также применяется `NormalizeGlslVaryings()`** — Slang генерирует разные имена varying'ов для vertex и fragment стейджей:
- Vertex out: `entryPointParam_vertMain_Color_0`  
- Fragment in: `input_Color_0`

OpenGL 4.1 на macOS матчит varying'и по имени, не по location. Функция переименовывает оба в `v_Color_0`.

### Metal — buffer layout

**Критично:** Slang для Metal назначает ConstantBuffer'ы с `[[buffer(0)]]`, `[[buffer(1)]]` и т.д. Вертексные данные нельзя класть на те же слоты.

Текущий layout:
```
[[buffer(0)]] = CameraUBO
[[buffer(1)]] = ObjectUBO
[[buffer(2)]] = LightingUBO
[[buffer(3)]] = MaterialUBO
[[buffer(10)]] = Vertex data (stage_in через vertex descriptor)
```

В `ShaderMTL::FlushUniforms` используй atIndex: 0, 1, 2, 3.
В `RenderApiMTL::DrawIndexed` вертексный буфер: `atIndex:10`.
В `ShaderMTL::GetOrCreatePipelineState` vertex descriptor: `bufferIndex=10`, `layouts[10]`.

### Metal — entry points

Slang добавляет суффикс `_0` к именам функций в MSL (`vertMain` → `vertMain_0`, `fragMain` → `fragMain_0`). `ShaderMTL` ищет сначала точное имя, потом с `_0`.

### macOS/Windows OpenGL — дополнительный патчинг GLSL

Помимо `PatchGlslForGL41()` и `NormalizeGlslVaryings()`, в `ShaderOGL::CompileSlangProgram` применяется ещё один патч:

- **C-style struct initializer → GLSL конструктор**: Slang иногда генерирует `TypeName var = { a, b, c };` при передаче UBO-backed структуры по значению. GLSL требует `TypeName var = TypeName(a, b, c);`. Regex `(\w+)\s+(\w+)\s*=\s*\{([^}]+)\}` → `$1 $2 = $1($3)` правит это автоматически.

При ошибке компиляции GLSL исходник дампится в `glsl_dump_fail.glsl` рядом с бинарником для диагностики.

### Ограничения Slang для Metal

- `fwidth(float2)` — **не поддерживается** для Metal (векторный overload). Использовать `float2(fwidth(x.x), fwidth(x.y))`.
- `discard` в фрагментном шейдере — **не поддерживается** в базовом Metal профиле. Заменить на `return float4(0, 0, 0, 0)`.

---

## Sandbox / Редактор

### Структура Sandbox (после рефактора)

```
Sandbox/src/
├── App/                    SandboxApp.cpp — точка входа
├── Core/
│   ├── EditorWorldContext  — per-session состояние (сцена + мир + камера + undo)
│   ├── SceneSession        — базовая структура сессии
│   └── SetTransformCommand — команда трансформации для undo
├── Editor/
│   ├── EditorCameraController  — orbit-камера (Blender-style)
│   ├── EditorCameraState       — состояние орбиты (target, dist)
│   └── EditorViewport          — FBO, grid, ImGui::Image viewport
├── Input/
│   ├── InputActions        — централизованный маппинг input → action
│   ├── InputContext        — enum Editor/Game (контекстный стек)
│   ├── ActionMap           — хранение биндингов
│   └── ActionBinding       — описание одного биндинга (key/mouse + mods + trigger)
├── SceneView/
│   ├── SceneViewLayer      — главный Layer (OnUpdate / OnImGuiRender / OnEvent)
│   ├── SceneViewLayerHost  — хост для панелей, API switching, undo
│   └── SceneViewLayer_*    — отдельные части логики (IO, Camera, Play, Render)
├── UI/
│   ├── GizmoSystem         — ImGuizmo гизмо
│   └── Panels/             — отдельные панели (Toolbar, SceneHierarchy, Properties, Camera, Profiler, ContentBrowser)
└── Play/
    └── SceneSerializer     — сериализация сцены
```

### Основной цикл рендера (SceneViewLayer)

```cpp
// OnUpdate:
m_Viewport.BeginSceneRender(active);  // bind FBO, SetSceneCamera
active->Update(dt);                    // World::Update (RenderSystem)
m_Viewport.DrawEditorOverlays(active); // grid (только в Edit-режиме)
m_Viewport.EndSceneRender();           // unbind FBO

// OnImGuiRender:
RunSceneViewImGuiFrame(*this);  // DrawImGui → ImGui::Image(FBO texture)
```

### Сессии / SceneSession

Каждая вкладка — `EditorWorldContext` (наследник `SceneSession`). Содержит:
- `EditorScene` — оригинальная сцена редактора
- `ActiveScene` — рабочая копия (для Play-режима)
- `RuntimeWorld` — World со своим RenderSystem
- `ViewportCamera` — `EditorCamera` (orbit, pitch=-30° по умолчанию)

### EditorCamera и сетка

Начальная позиция камеры: orbit target=(0,0,0), dist=8, pitch=-30°. Камера оказывается ниже Y=0 плоскости (y≈-4). Сетка рендерится на Y=0. **Это нормально** — Slang-шейдер grid.slang корректно обрабатывает лучи снизу плоскости.

### API switching (UI)

Клик на "API: Metal" → `OnRendererApiSelected(api)` → `EngineConfig::SaveRendererPreference(api)` записывает `renderer_pending` в `engine.json` → `Application::RequestRestart()` → `execv` без `--renderer=` аргумента → новый процесс читает `renderer_pending`.

### InputActions — централизованный input

Все горячие клавиши редактора определены в `Sandbox/config/keybindings.json` и загружаются при старте через `InputActions::LoadFromJson`. Биндинги декларативные: ключ + модификаторы + trigger (`Pressed`/`Down`/`Released`/`Drag`).

```cpp
// В начале кадра (SceneViewLayer::OnUpdate):
Sandbox::InputActions::BeginFrame();

// Запрос действия:
if (InputActions::Triggered("Editor.Gizmo.Translate")) { ... }  // одиночное нажатие
if (InputActions::Down("Editor.Camera.OrbitRmb"))       { ... }  // удерживание
float wheel = InputActions::GetAxis(InputActions::Axis::MouseWheel);
```

**Контексты** (`InputContext::Editor` / `InputContext::Game`): при переходе в Play-режим `PushContext(Game)` активирует game-биндинги. `PopContext(Game)` возвращает Editor при остановке. Это позволяет одному ключу иметь разный смысл в редакторе и в игре.

Все горячие клавиши Sandbox используют `InputActions` — прямые вызовы `ImGui::IsKeyPressed` в редакторе запрещены (кроме WantTextInput-блока).

### SceneSerializer — компонент-опциональная сериализация

`SaveToFile` и `SerializeToJson` теперь итерируют **все** сущности через `ForEach<IDComponent>` (вместо фильтра по набору компонентов). Каждый компонент сериализуется условно через `entity->HasComponent<T>()`. Это позволяет правильно сохранять сущности без меша (свет, камера) и добавлять новые компоненты без изменения логики итерации.

### SceneBrowser

`SceneBrowser` — **плавающее окно**, не тайлированная панель. Рендерится напрямую из `RunSceneViewImGuiFrame`, минуя `drawIfVisible`.

---

## Известные ограничения / To Fix

- **PhysX на macOS**: собирается через o3de-форк, поддерживает Apple Silicon и Intel. CMake автоматически выбирает `TARGET_BUILD_PLATFORM=mac`.
- **PhysX на Windows**: собирается с `-DCHE_BUILD_PHYSICS=ON`. CMake автоматически выбирает `TARGET_BUILD_PLATFORM=windows`.
- **PhysX на Linux**: собирается с оригинальным NVIDIA SDK, `TARGET_BUILD_PLATFORM=linux`.
- **OpenGL on macOS**: ограничен версией 4.1. Рабочий, но Metal предпочтительнее.
- **Slang на Windows**: загружается через `file(DOWNLOAD)` + `file(ARCHIVE_EXTRACT)` (не FetchContent) — это нужно, чтобы избежать конфликта между VS-bundled cmake 3.31 и system cmake 4.x при регенерации ZERO_CHECK.
- **Slang компиляция медленная**: первый запуск грузит шейдеры ~1-2с каждый. Это нормально.
- **Hot-reload ModuleManager**: упрощён (убран FileWatcher), hot-reload модулей не работает в текущей версии.
