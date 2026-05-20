# ECS / Scene / World

## Обзор

CHEngine использует [entt](https://github.com/skypjack/entt) как основу ECS.  
Поверх entt реализован слой из `Scene`, `Entity`, `World` и `SystemScheduler`.

```
World
├── Scene          — контейнер сущностей (entt::registry + UUID-индекс)
├── SystemScheduler — управляет системами и их фазами
├── DeferredOps    — отложенные операции создания/удаления
├── EventBus       — типизированная шина событий по фазам
└── IPhysicsWorld  — физический мир (из PhysicsPhysX-модуля)
```

## Scene

`Scene` — это обёртка над `entt::registry` с добавлением UUID-поиска и EntityHandle-пула.

### Создание сущности

```cpp
EntityHandle h = scene.CreateEntity("Player");
// По умолчанию добавляются: TagComponent, IDComponent, TransformComponent,
// MeshComponent, ColorComponent, VisibilityComponent
```

UUID генерируется автоматически (RFC 4122 v4, через `boost::uuids`).

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

`Entity` — тонкая обёртка с удобным API поверх `entt::entity`.

```cpp
// Работа с компонентами
entity.AddComponent<LightComponent>(lightDesc);
entity.GetComponent<TransformComponent>().Position = {1, 2, 3};
entity.HasComponent<RigidBody3DComponent>();
entity.RemoveComponent<LightComponent>();

// TryGet — не бросает исключение если компонента нет
auto* transform = entity.TryGetComponent<TransformComponent>();
```

## Компоненты

Все компоненты — простые структуры данных (data-only, без методов с логикой).

### TransformComponent

```cpp
struct TransformComponent {
    glm::vec3 Position = {0, 0, 0};
    glm::vec3 Rotation = {0, 0, 0};  // Эйлеровы углы в радианах
    glm::vec3 Scale    = {1, 1, 1};

    glm::mat4 GetMatrix() const;     // Вычисляет TRS-матрицу
};
```

### MeshComponent

```cpp
struct MeshComponent {
    std::vector<Mesh> Meshes;        // GPU-буферы (VertexArray + материал)
    std::string SourcePath;          // Путь к файлу-источнику (для отображения в UI)
};
```

### CameraComponent

```cpp
struct CameraComponent {
    float FOV         = 60.0f;
    float NearClip    = 0.1f;
    float FarClip     = 1000.0f;
    float AspectRatio = 16.0f / 9.0f;
    bool  Active      = true;
    bool  Primary     = false;       // Используется RenderSystem для выбора камеры
};
```

### LightComponent

```cpp
struct LightComponent {
    glm::vec3 Color     = {1, 1, 1};
    float     Intensity = 1.0f;
    LightType Type      = LightType::Point;  // Point, Directional, Spot
};
```

### ColorComponent

```cpp
struct ColorComponent {
    glm::vec4 Color = {1, 1, 1, 1};  // RGBA
};
```

### VisibilityComponent

```cpp
struct VisibilityComponent {
    bool Visible = true;
};
```

### LifetimeComponent

```cpp
struct LifetimeComponent {
    float RemainingSeconds;
    bool  ShouldDestroy = false;  // Устанавливается LifetimeSystem при достижении 0
};
```

Используется для временных сущностей (эффекты, пули, частицы):

```cpp
auto bullet = scene.CreateEntity("Bullet");
bullet.AddComponent<LifetimeComponent>(3.0f);  // Уничтожится через 3 секунды
```

### ScriptComponent

```cpp
struct ScriptEntry {
    std::string Name;   // Имя скрипта (для отображения в UI)
    std::string Source; // Lua-исходник
};

struct ScriptComponent {
    std::vector<ScriptEntry> Scripts;
};
```

Скрипты выполняются `LuaScriptSystem` на каждом тике фазы `Simulation`.

### UICanvasComponent / UIRectTransform

Компоненты для 2D UI поверх 3D-сцены:

```cpp
struct UICanvasComponent { /* настройки канваса */ };
struct UIRectTransform {
    glm::vec2 Position;
    glm::vec2 Size;
    glm::vec2 Anchor;
};
```

### RigidBody3DComponent

Подробно описан в [документации по физике](physics.md).

## World

`World` — контейнер для всей симуляции. Обычно создаётся один, но может быть несколько (например, игровой мир + превью-сцена в редакторе).

```cpp
World& world = Application::Get().GetWorld();
Scene& scene = world.GetScene();
```

### Жизненный цикл

```cpp
world.update(dt);
// Внутри:
// 1. SystemScheduler::runPhase(Simulation, ...)
// 2. SystemScheduler::runPhase(Presentation, ...)  [если WorldState включает рендер]
// 3. DeferredOps::flush(scene)                     [удаление/создание сущностей]
```

### WorldState

`WorldState` управляет тем, какие фазы активны:

```cpp
enum class WorldState {
    Presenting,                  // Только рендер (Edit-режим редактора)
    Simulating,                  // Физика + рендер (Play-режим)
    SimulatingWithoutPresenting, // Только физика (неактивные сессии)
};

world.SetState(WorldState::Simulating);
```

### Физика в World

```cpp
// Пересоздать физический рантайм (например, после загрузки сцены)
world.RebuildPhysicsRuntime();

// Уничтожить физику конкретной сущности
world.DestroyRigidBodyRuntime(entityHandle);

// Очистить весь физический мир
world.ClearPhysicsRuntime();
```

## SystemScheduler

Управляет системами — регистрирует, запускает по фазам, управляет приоритетами.

### Регистрация системы

```cpp
world.GetScheduler().emplaceSystem<MySystem>(/* приоритет */ 50);
```

### Реализация системы

```cpp
class MySystem : public ISystem {
public:
    SystemPhase GetPhase() const override { return SystemPhase::Simulation; }
    uint8_t     GetPriority() const override { return 50; }

    void update(World& world, DeferredOps& ops, Timestep dt) override {
        auto& scene = world.GetScene();

        scene.ForEach<MyComponent>([&](EntityHandle h, const UUID& id, MyComponent& comp) {
            comp.Timer += dt.GetSeconds();

            if (comp.Timer > comp.Lifetime)
                ops.DestroyEntity(h);  // НЕ удаляет сразу, а откладывает
        });
    }

    void onEvent(World& world, Event& e) override {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([](KeyPressedEvent& e) {
            // обработка клавиши
            return false;
        });
    }
};
```

### Фазы и встроенные системы

Активны только две фазы: `Simulation` и `Presentation` (фаза `Initialization` убрана).

| Система | Фаза | Приоритет | Что делает |
|---------|------|-----------|-----------|
| `PhysicsSystem` | Simulation | 100 | Синхронизирует ECS ↔ PhysX |
| `LuaScriptSystem` | Simulation | 50 | Выполняет Lua-скрипты из ScriptComponent |
| `LifetimeSystem` | Simulation | 20 | Уменьшает таймер, помечает к удалению |
| `ComponentValidationSystem` | Simulation | 30 | Проверяет целостность компонентов |
| `RenderSystem` | Presentation | 100 | Отправляет меши на рендер |

### Управление системами

```cpp
auto& scheduler = world.GetScheduler();

// Включить/выключить
scheduler.setEnabled<PhysicsSystem>(false);
scheduler.isEnabled<PhysicsSystem>();

// Получить указатель
auto* sys = scheduler.getSystem<MySystem>();
```

## DeferredOps

Безопасное создание, удаление и модификация сущностей прямо во время итерации по ECS.  
Все операции откладываются и применяются после завершения всех фаз текущего тика.

```cpp
// Внутри системы (сигнатура update принимает DeferredOps&):
void update(World& world, DeferredOps& ops, Timestep dt) override {
    world.GetScene().ForEach<ProjectileComponent>([&](EntityHandle h, ..., ProjectileComponent& p) {
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
