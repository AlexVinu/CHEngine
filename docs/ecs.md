# ECS / Scene / World

## Обзор

CHEngine использует [entt](https://github.com/skypjack/entt) как основу ECS.  
Поверх entt реализован слой из `Scene`, `Entity`, `World` и `SystemScheduler`.

```
World
├── Scene          — контейнер сущностей (entt::registry + UUID-индекс)
├── SystemScheduler — управляет системами и их фазами
├── DeferredOps    — отложенные операции создания/удаления (был CommandBuffer)
├── EventBus       — pub/sub шина событий между системами
└── IPhysicsWorld  — физический мир (из PhysicsPhysX-модуля)
```

## Scene

`Scene` — обёртка над `entt::registry` с UUID-поиском и EntityHandle-пулом.

### Создание сущности

```cpp
EntityHandle h = scene.CreateEntity("Player");
// По умолчанию добавляются: TagComponent, IDComponent, TransformComponent
```

UUID генерируется автоматически (RFC 4122 v4, через `boost::uuids`).

### Поиск и итерация

```cpp
// По UUID
EntityHandle h = scene.TryGetEntityHandleByUUID(uuid);

// Итерация по набору компонентов
scene.ForEach<MeshComponent, TransformComponent>(
    [](EntityHandle h, const UUID& id, MeshComponent& mesh, TransformComponent& tr) {
        // ...
    });
```

### Удаление сущности

```cpp
// Безопасно во время ForEach — через DeferredOps:
ops.DestroyEntity(handle);
// Применится после завершения всех систем кадра
```

## Entity

```cpp
entity.AddComponent<LightComponent>(lightDesc);
entity.GetComponent<TransformComponent>().ObjectTransform.Position = {1, 2, 3};
entity.HasComponent<RigidBody3DComponent>();
entity.RemoveComponent<LightComponent>();

// TryGet — возвращает nullptr если компонента нет
auto* transform = entity.TryGetComponent<TransformComponent>();
```

## Компоненты

Все компоненты — простые структуры данных.

### Обязательные (добавляются при CreateEntity)

| Компонент | Содержимое |
|-----------|-----------|
| `IDComponent` | `UUID` |
| `TagComponent` | имя (string) |
| `TransformComponent` | `Transform ObjectTransform` (Position/Rotation/Scale + GetMatrix()) |

### Опциональные (добавляются явно)

| Компонент | Назначение |
|-----------|-----------|
| `MeshComponent` | `vector<Mesh> Meshes`, `SourcePath` — move-only |
| `ColorComponent` | `glm::vec4 Color` |
| `VisibilityComponent` | `bool Visible` |
| `LightComponent` | `Light LightData` (Type/Color/Intensity/Range/...) |
| `CameraComponent` | `SceneCamera`, `Primary`, `FixedAspectRatio` |
| `RigidBody3DComponent` | BodyDesc, ShapeDesc, `IPhysicsBody*` |
| `LifetimeComponent` | `RemainingSeconds`, `DestroyOnExpire` |
| `ScriptComponent` | `vector<ScriptEntry> Scripts` (Lua-скрипты) |

### UI-компоненты (игровой UI)

Рендерятся через `UIRenderSystem` в фазе `Presentation` (шедулер, приоритет 200).

| Компонент | Назначение |
|-----------|-----------|
| `UICanvasComponent` | Корневой контейнер; `RenderMode` (ScreenSpaceOverlay / WorldSpace), `SortOrder` |
| `UIRectTransformComponent` | 2D-лейаут: `AnchorMin/Max`, `Position`, `Size`, `Pivot`, `Rotation`, `Alpha`, `ZOrder` |
| `UIPanelComponent` | Скруглённый фон с рамкой: `Color`, `BorderColor`, `BorderWidth`, `CornerRadius` |
| `UIImageComponent` | Цвет или текстура: `Color`, `TexturePath`, `PreserveAspect` |
| `UITextComponent` | Текст с шрифтом: `Text`, `FontPath`, `FontSize`, `Color`, `HAlign`, `VAlign`, `WordWrap` |
| `UIButtonComponent` | Кнопка с состояниями: `NormalColor`, `HoverColor`, `PressedColor`, `OnClick` (Lua) |
| `UISliderComponent` | Горизонтальный слайдер: `Value`, `Min`, `Max`, цвета, `OnChange` (Lua) |

## World

`World` — контейнер для всей симуляции. В редакторе каждая вкладка-сессия имеет свой `World`.

### WorldState

`World` управляется через `WorldState`:

| Состояние | Что работает |
|-----------|-------------|
| `Presenting` | Только Presentation-фаза (рендер). Edit-режим редактора. |
| `Simulating` | Simulation + Presentation (физика + рендер). Play-режим. |
| `SimulatingWithoutPresenting` | Только Simulation. Неактивные фоновые сессии. |
| `NONE` | Ничего не работает. Переходное состояние при выключении. |

```cpp
world.SetState(WorldState::Simulating);   // применится в начале следующего Update()
```

### Жизненный цикл Update

```cpp
World::Update(dt):
  1. Если m_PendingState != m_State → ApplyStateTransition() (NotifyEnd/NotifyBegin систем)
  2. Если Simulating/SimulatingWithoutPresenting → Scheduler::RunPhase(Simulation)
  3. Если Presenting/Simulating → Scheduler::RunPhase(Presentation)
  4. DeferredOps::Flush(world, scene)   — применить отложенные изменения ECS
```

## SystemScheduler

Управляет системами — регистрирует, запускает по фазам, уведомляет о начале/конце.

### Реализация системы

```cpp
class MySystem : public ISystem {
public:
    MySystem() : ISystem(SystemPhase::Simulation, /*priority*/ 50) {}

    const char* GetName() const override { return "MySystem"; }

    void OnBegin(World& world, DeferredOps& ops) override { /* вызван при переходе в состояние */ }
    void Run(World& world, DeferredOps& ops, Timestep dt) override { /* каждый кадр */ }
    void OnEnd(World& world, DeferredOps& ops) override { /* вызван при выходе из состояния */ }
};
```

### Регистрация

```cpp
// В конструкторе World или вручную:
m_Scheduler.EmplaceSystem<MySystem>();
```

### Встроенные системы (RegisterDefaultSystems)

| Система | Фаза | Приоритет | Что делает |
|---------|------|-----------|-----------|
| `LifetimeSystem` | Simulation | 20 | Уменьшает таймер, помечает к удалению |
| `ComponentValidationSystem` | Simulation | 30 | Проверяет целостность компонентов |
| `LuaScriptSystem` | Simulation | 5 | Запускает Lua-скрипты (OnStart/OnUpdate/OnStop) |
| `PhysicsSystem` | Simulation | 100 | Синхронизирует ECS ↔ PhysX |
| `RenderSystem` | Presentation | 10 | Строит PassDesc и добавляет в фрейм-граф |
| `UIRenderSystem` | Presentation | 200 | Рендерит игровой UI через ImGui DrawList |

## DeferredOps

Безопасное создание и удаление сущностей прямо во время `ForEach`.

```cpp
// Внутри системы:
void Run(World& world, DeferredOps& ops, Timestep dt) override {
    auto& scene = *world.GetScene();
    scene.ForEach<ProjectileComponent>([&](EntityHandle h, ..., ProjectileComponent& p) {
        if (p.HitSomething)
            ops.DestroyEntity(h);         // НЕ удаляет сразу — откладывает
    });
}
// После всех систем: DeferredOps::Flush(world, scene) применит удаления
```

Доступные операции:

```cpp
ops.DestroyEntity(handle);
ops.AddComponent<TransformComponent>(handle, args...);
ops.RemoveComponent<RigidBody3DComponent>(handle);

// Подписка на добавление компонента (например, для PhysicsSystem)
HookToken token = ops.SubscribeOnComponentAdded<RigidBody3DComponent>(
    [](World& w, EntityHandle h) { /* создать PhysX тело */ }
);
ops.Unsubscribe(token);
```

## Lua Script System

`LuaScriptSystem` запускает скрипты из `ScriptComponent::Scripts[]`.

Уровни скриптов:
- **Entity scripts** — `ScriptComponent` на конкретной сущности
- **World scripts** — `Scene::WorldScripts` (глобальные для сцены)

Lua-колбэки (все опциональны):

```lua
-- Entity script
function OnStart(entity, world) end
function OnUpdate(entity, world, dt) end
function OnStop(entity, world) end

-- World script
function OnStart(world) end
function OnUpdate(world, dt) end
function OnStop(world) end
```

`UIButtonComponent.OnClick` и `UISliderComponent.OnChange` — имена Lua-функций на скрипте сущности.

## EventBus

Типизированная шина событий внутри `World`. Двойная буферизация по фазам.

```cpp
// Публикация
world.GetEventBus().Publish<CollisionEvent>(SystemPhase::Simulation, entityA, entityB);

// Подписка (внутри Run/OnBegin)
ops.GetEventBus().ConsumePhase<CollisionEvent>(SystemPhase::Simulation, [](CollisionEvent& e) {
    // обработка
});
```
