# ECS / Scene / World

## Обзор

CHEngine использует [entt](https://github.com/skypjack/entt) как основу ECS.  
Поверх entt реализован слой из `Scene`, `Entity`, `World` и `SystemScheduler`.

```
World
├── Scene           — контейнер сущностей (entt::registry + UUID-индекс + строковый/скриптовый пул)
├── SystemScheduler — управляет системами и их фазами
├── DeferredOps     — отложенные структурные операции + хуки компонентов
├── EventBus        — типизированная шина событий по фазам
└── PhysWorldHandle — handle физического мира (создаётся через PhysicsSubsystem)
```

## Scene

`Scene` — это обёртка над `entt::registry` с добавлением UUID-поиска и EntityHandle-пула.

### Создание сущности

```cpp
EntityHandle h = scene.CreateEntity("Player");
// По умолчанию добавляются ТОЛЬКО: TagComponent, IDComponent, VisibilityComponent.
// Transform / Mesh / Light и т.д. добавляются явно через Entity::AddComponent<T>.
```

UUID генерируется автоматически (через `boost::uuids`). `CreateEntity` возвращает
`EntityHandle` (index + generation), **не** объект `Entity`. Чтобы работать с
компонентами, получи обёртку:

```cpp
Entity* e = scene.TryGetEntity(h);   // nullptr если хэндл невалиден
```

Имя тега хранится как `StringID` в строковом пуле сцены (`Scene::InternString` /
`Scene::GetString`), а не как `std::string`.

### Поиск сущности

```cpp
// По UUID
EntityHandle h = scene.FindByID(uuid);

// Итерация по компоненту
scene.ForEach<MeshComponent>([](EntityHandle h, const UUID& id, MeshComponent& mesh) {
    // ...
});
```

### Удаление сущности

```cpp
scene.DestroyEntity(handle);
// Или через DeferredOps (безопасно во время симуляции):
world.GetDeferredOps().DestroyEntity(handle);
```

## Entity

`Entity` — тонкая обёртка (`entt::entity` + `Scene*`), получаемая через
`Scene::TryGetEntity(handle)`.

```cpp
Entity* e = scene.TryGetEntity(handle);

// Работа с компонентами
e->AddComponent<LightComponent>();
e->GetComponent<TransformComponent>().ObjectTransform.Position = {1, 2, 3};
e->HasComponent<RigidBody3DComponent>();
e->RemoveComponent<LightComponent>();

// PatchComponent — вызывает entt-signal'ы (нужно для реактивных систем)
e->PatchComponent<TransformComponent>([](TransformComponent& t) {
    t.ObjectTransform.Position.y += 1.0f;
    t.MarkDirty();
});
```

## Компоненты

Все компоненты — **POD-структуры** (data-only). При добавлении нового компонента его
нужно зарегистрировать в meta-сериализаторе и добавить в `AllComponents`
(см. `Scene/Components.h`).

### IDComponent / TagComponent / ParentNodeComponent

```cpp
struct IDComponent  { UUID Value{}; };
struct TagComponent { StringID Name = INVALID_ID<StringID>; };   // имя в строковом пуле
struct ParentNodeComponent { UUID Value{}; };                    // родитель (иерархия сцены)
```

### TransformComponent

```cpp
struct Transform {
    glm::vec3 Position = {0, 0, 0};
    glm::vec3 Rotation = {0, 0, 0};  // Эйлеровы углы в ГРАДУСАХ
    glm::vec3 Scale    = {1, 1, 1};
    glm::mat4 GetMatrix() const;
    glm::mat4 GetNormalMatrix() const;
};

struct TransformComponent {
    Transform ObjectTransform;
    bool      Dirty = true;          // true = изменено пользователем, физика ещё не знает
    void MarkDirty() { Dirty = true; }
};
```

> Любое изменение `ObjectTransform` из кода (Gizmo, Properties, Undo, скрипт)
> обязано вызвать `MarkDirty()` — иначе `PhysicsSystem` не подхватит правку.

### MeshComponent

```cpp
struct MeshComponent {
    MeshRef  Mesh;        // единый меш с субмешами (VB/IB разделяется через MeshLoader)
    StringID SourcePath;  // путь к файлу-источнику в строковом пуле сцены
};
```

`MeshRef` — умный указатель, управляющий временем жизни через `MeshLoader` (refcount).
Один `Mesh` содержит вектор материалов (по одному на субмеш); доступ через `Mesh->GetMaterial(submeshIndex)`.
Доступ к буферам и диапазонам субмешей — через `MeshLoader::GetGpuRecord(Mesh.Handle())`.

### CameraComponent

```cpp
using CameraVariant = std::variant<PerspectiveCamera, OrthographicCamera>;

struct CameraComponent {
    CameraVariant Camera = PerspectiveCamera{};
    bool FixedAspectRatio = false;
    bool Primary          = true;    // RenderSystem выбирает Primary-камеру
    bool IsActive         = false;
};
```

### LightComponent

```cpp
enum class LightType : int { None = -1, Directional = 0, Point = 1, Spot = 2 };

struct Light {
    LightType Type      = LightType::None;
    glm::vec3 Color     = {1, 1, 1};
    float     Intensity = 1.0f;
    float     Range     = 10.0f;     // Point / Spot
    float     InnerCone = 12.5f;     // Spot, градусы
    float     OuterCone = 17.5f;     // Spot, градусы
};

struct LightComponent { Light LightData; };
```

Направление (Directional/Spot) берётся из `Transform.Rotation`, позиция (Point/Spot) — из `Transform.Position`.

### ColorComponent / VisibilityComponent

```cpp
struct ColorComponent      { glm::vec4 Color = {0.8f, 0.8f, 0.8f, 1.0f}; };  // RGBA
struct VisibilityComponent { bool Visible = true; };
```

### LifetimeComponent

```cpp
struct LifetimeComponent {
    float RemainingSeconds = 0.0f;
    bool  DestroyOnExpire  = true;   // LifetimeSystem уничтожает сущность при достижении 0
};
```

```cpp
EntityHandle h = scene.CreateEntity("Bullet");
scene.TryGetEntity(h)->AddComponent<LifetimeComponent>(LifetimeComponent{ 3.0f });
```

### ScriptComponent

```cpp
struct ScriptEntry { std::string Path; bool Enabled = true; };  // путь к .lua файлу

struct ScriptComponent { ScriptsID Scripts = INVALID_ID<ScriptsID>; };
```

`ScriptComponent` ссылается на запись в **скриптовом пуле** сцены
(`Scene::AllocateScripts` / `GetScripts`), а не хранит исходник напрямую. Скрипты —
ссылки на `.lua` файлы; выполняются `LuaScriptSystem` на каждом тике фазы `Simulation`.
Кроме того, у сцены есть `Scene::WorldScripts` — скрипты уровня всей сцены.

### UI-компоненты

Система UI: корневой канвас (`UIOverlayCanvasComponent` для Screen Space или
`UIWorldCanvasComponent` для World Space) + дочерние элементы, у каждого —
`UIRectTransformComponent` и один из визуальных компонентов:

```cpp
struct UIOverlayCanvasComponent { glm::vec2 AnchorMin, AnchorMax, Position, Size, Pivot; int SortOrder; float Alpha; };
struct UIWorldCanvasComponent   { glm::vec2 Size; float Alpha; bool DoubleSided; };
struct UIRectTransformComponent { glm::vec2 AnchorMin, AnchorMax, Pivot; float Size; float Alpha; int ZOrder; };

struct UIImageComponent  { float Width; StringID TexturePath; bool PreserveAspect, SlicedBorder; };
struct UITextComponent   { StringID Text, FontPath; float FontSize; glm::vec4 Color; /* + выравнивание, Bold/Italic/WordWrap */ };
struct UIPanelComponent  { float Width; glm::vec4 BorderColor; float BorderWidth, CornerRadius; };
struct UIButtonComponent { float Width; glm::vec4 NormalColor, HoverColor, PressedColor, DisabledColor; bool Interactable; float CornerRadius; };
struct UISliderComponent { float Width, Value, Min, Max; glm::vec4 BackgroundColor, FillColor, HandleColor; float HandleSize; bool Interactable; };
```

UI-ввод обрабатывает `UIInputSystem` (фаза `Simulation`), рендер — `UIRenderSystem` (фаза `Presentation`).
Кнопки вызывают Lua-функцию `OnClick(entity)` на своей сущности.

### RigidBody3DComponent

Подробно описан в [документации по физике](physics.md).

## World

`World` — контейнер для всей симуляции. `Application` им **не владеет** — мир создаёт
прикладной слой; в редакторе их несколько (по одному на сессию-вкладку), а список
ведёт `WorldsList`.

```cpp
WorldsList worlds;
World      world{ &worlds };
world.SetScene(MakeRef<Scene>());
Scene& scene = *world.GetSceneRef();
```

### Жизненный цикл

```cpp
world.Update(dt);
// Внутри:
// 1. SystemScheduler::RunPhase(Simulation, ...)
// 2. SystemScheduler::RunPhase(Presentation, ...)  [если WorldState включает рендер]
// 3. DeferredOps::Flush(scene)                      [удаление/создание/трансфер сущностей]
// 4. ProcessPendingSceneLoad()                      [отложенная загрузка сцены, если есть]
```

Дополнительно:
- `world.RefreshRenderTransforms()` — пере-загрузить UBO трансформов после поздних
  правок в кадре (drag гизмо) перед `EndFrame`.
- `world.RequestSceneLoad(relPath)` — отложенная смена сцены в конце кадра
  (используется Lua: `World:LoadScene`).

### WorldState

`WorldState` управляет тем, какие фазы активны:

```cpp
enum class WorldState : uint8_t {
    NONE,
    Presenting,                  // Только рендер (Edit-режим редактора)
    Simulating,                  // Физика + рендер (Play-режим)
    SimulatingWithoutPresenting, // Только физика (неактивные сессии)
};

world.SetState(WorldState::Simulating);
WorldState s = world.GetState();
```

### Физика в World

`World` хранит описание физического мира и handle рантайма; само создание тел
делает `PhysicsSystem` (через хуки `DeferredOps` на добавление/удаление
`RigidBody3DComponent`).

```cpp
world.SetPhysicsWorldDesc(PhysicsWorldDesc{ .Gravity = {0, -9.81f, 0} });
PhysWorldHandle pw = world.GetPhysicsRuntimeWorld();  // невалиден вне Play-режима
```

## SystemScheduler

Управляет системами — регистрирует, запускает по фазам, управляет приоритетами.

### Регистрация системы

```cpp
world.GetScheduler().EmplaceSystem<MySystem>(/* приоритет */ 50);
```

### Реализация системы

Фаза и приоритет задаются в конструкторе базового `ISystem`. Главный метод — `Run`.

```cpp
class MySystem : public ISystem {
public:
    explicit MySystem(uint8_t priority = 50)
        : ISystem(SystemPhase::Simulation, priority) {}

    const char* GetName() const override { return "MySystem"; }

    void Run(World& world, DeferredOps& ops, Timestep dt) override {
        Scene& scene = *world.GetSceneRef();

        scene.ForEach<MyComponent>([&](EntityHandle h, const UUID& id, MyComponent& comp) {
            comp.Timer += dt.GetSeconds();
            if (comp.Timer > comp.Lifetime)
                ops.DestroyEntity(h);  // НЕ удаляет сразу, а откладывает
        });
    }

    // Опционально: вызываются при входе/выходе из активного состояния World
    void OnBegin(World& world, DeferredOps& ops) override {}
    void OnEnd  (World& world, DeferredOps& ops) override {}
};
```

> События обрабатываются не через систему, а через `EventBus`: системы публикуют
> `world.GetEvents().Publish<E>(...)` и потребляют `ConsumePhase<E>(...)` внутри `Run`.

### Фазы и встроенные системы

Две фазы: `Simulation` и `Presentation`. Регистрируются в `World::RegisterDefaultSystems`:

| Система | Фаза | Приоритет | Что делает |
|---------|------|-----------|-----------|
| `LuaScriptSystem` | Simulation | 5 | Выполняет Lua-скрипты (ScriptComponent + WorldScripts) |
| `UIInputSystem` | Simulation | 6 | Обрабатывает наведение/клики по UI-элементам |
| `LifetimeSystem` | Simulation | 20 | Уменьшает таймер, помечает к удалению |
| `ComponentValidationSystem` | Simulation | 30 | Проверяет целостность компонентов |
| `PhysicsSystem` | Simulation | 100 | Синхронизирует ECS ↔ PhysX, шаг симуляции |
| `RenderSystem` | Presentation | 10 | Обходит меши и строит рендер-граф |
| `UIRenderSystem` | Presentation | 200 | Рендерит UI поверх сцены |

### Управление системами

```cpp
SystemScheduler& scheduler = world.GetScheduler();

// Включить/выключить по имени или ссылке
scheduler.SetEnabled("PhysicsSystem", false);
bool on = scheduler.IsEnabled("PhysicsSystem");
```

## DeferredOps

Безопасное создание, удаление и модификация сущностей прямо во время итерации по ECS.  
Все операции откладываются и применяются после завершения всех фаз текущего тика.

```cpp
// Внутри системы (сигнатура Run принимает DeferredOps&):
void Run(World& world, DeferredOps& ops, Timestep dt) override {
    world.GetSceneRef()->ForEach<ProjectileComponent>([&](EntityHandle h, const UUID&, ProjectileComponent& p) {
        if (p.HitSomething)
            ops.DestroyEntity(h);  // НЕ удаляет сразу, а откладывает
    });
}
```

```cpp
// Создать новую сущность отложенно
DeferredEntityHandle deferred = ops.CreateEntity("Explosion");
ops.AddComponent<TransformComponent>(deferred, hitPoint);
ops.AddComponent<LifetimeComponent>(deferred, 1.5f);
```

```cpp
// Удалить компонент у существующей сущности
ops.RemoveComponent<LightComponent>(handle);

// Произвольная отложенная операция
ops.Enqueue([](Ref<Scene> scene) {
    // любой код, работающий со сценой
});

// Перенести сущность (с поддеревом ParentNodeComponent) в другой World — для мультимировой симуляции
ops.TransferEntity(handle, "OtherWorldName");
```

### Хуки компонентов

`DeferredOps` умеет вызывать колбэки при добавлении/удалении компонента и при
кросс-мировом трансфере. Этим пользуется, например, `PhysicsSystem`, чтобы создавать
PhysX-тело при появлении `RigidBody3DComponent`:

```cpp
ops.SubscribeOnComponentAdded<RigidBody3DComponent>(+[](World& w, EntityHandle h) { /* создать тело */ });
ops.SubscribeOnComponentRemoved<RigidBody3DComponent>(+[](World& w, EntityHandle h) { /* удалить тело */ });
```

## EventBus

Типизированная шина событий внутри World. Работает по фазам — события публикуются
в рамках одной фазы и потребляются в той же фазе или позже в том же тике.

```cpp
// Опубликовать событие
world.GetEvents().Publish<ExplosionEvent>(SystemPhase::Simulation, position, radius);

// Потребить все события фазы
world.GetEvents().ConsumePhase<ExplosionEvent>(SystemPhase::Simulation,
    [](const ExplosionEvent& e) {
        // обработка
    });
```
