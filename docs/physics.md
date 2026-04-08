# Физика

## Обзор

Физический движок — NVIDIA PhysX (опциональный модуль `PhysicsPhysX`).  
Движок подключается через `PhysicsFacade` и `RigidBody3DComponent`.

## PhysicsFacade

Статический фасад для создания физических объектов.

```cpp
// Инициализация (вызывается автоматически в Application)
PhysicsFacade::Init(physicsFactory);

// Создать физический мир
auto physWorld = PhysicsFacade::CreateWorld(PhysicsWorldDesc{ .Gravity = {0, -9.81f, 0} });

// Создать тело для сущности
PhysicsFacade::CreateRigidBodyRuntime(physWorld, rigidBodyComp, initialTransform);

// Уничтожить тело
PhysicsFacade::DestroyRigidBodyRuntime(physWorld, rigidBodyComp);
```

## RigidBody3DComponent

Добавляется к сущности для включения физики.

```cpp
RigidBody3DComponent rb;

// Тип тела
rb.BodyDesc.Type = PhysicsBodyType::Dynamic;    // Динамика (гравитация, столкновения)
// rb.BodyDesc.Type = PhysicsBodyType::Static;  // Статика (пол, стены)
// rb.BodyDesc.Type = PhysicsBodyType::Kinematic; // Кинематика (управляется кодом)

// Масса
rb.BodyDesc.Mass = 1.0f;

// Форма коллайдера
rb.ShapeDesc.Type   = PhysicsColliderShapeType::Box;
rb.ShapeDesc.HalfExtents = {0.5f, 0.5f, 0.5f};  // Полуразмеры AABB

// Режим синхронизации с трансформом ECS
rb.SyncMode = RigidBodySyncMode::Auto;

entity.AddComponent<RigidBody3DComponent>(rb);
```

### Форма коллайдера

| Тип | Поля | Описание |
|-----|------|----------|
| `Box` | `HalfExtents` (vec3) | Прямоугольный параллелепипед |
| `Sphere` | `Radius` (float) | Сфера |
| `Capsule` | `Radius`, `HalfHeight` | Капсула (для персонажей) |

### Режимы синхронизации

`RigidBodySyncMode` управляет тем, как `PhysicsSystem` обменивается данными между ECS-трансформом и физическим телом:

| Режим | Описание |
|-------|----------|
| `Auto` | Движок сам выбирает: Dynamic/Kinematic → читать из физики; Static → не трогать |
| `ReadFromPhysics` | Всегда копировать позицию из PhysX → TransformComponent |
| `WriteToPhysics` | Всегда копировать TransformComponent → PhysX (кинематическое управление) |
| `ReadWrite` | Оба направления |

**Для персонажа, управляемого кодом:**

```cpp
rb.BodyDesc.Type = PhysicsBodyType::Kinematic;
rb.SyncMode      = RigidBodySyncMode::WriteToPhysics;

// В системе:
auto& transform = entity.GetComponent<TransformComponent>();
transform.Position += velocity * dt;  // PhysicsSystem отправит это в PhysX
```

**Для падающего объекта:**

```cpp
rb.BodyDesc.Type = PhysicsBodyType::Dynamic;
rb.SyncMode      = RigidBodySyncMode::Auto;  // PhysX управляет позицией
```

## PhysicsSystem

Встроенная система (фаза `Simulation`, приоритет 100).  
Запускается автоматически, пользователю не нужно её регистрировать.

**Алгоритм:**
1. Перебрать все `RigidBody3DComponent`
2. Если `ShouldWriteToPhysics()` → скопировать TransformComponent → тело PhysX
3. Шаг симуляции PhysX (`world.Simulate(dt)`)
4. Если `ShouldReadFromPhysics()` → скопировать позицию PhysX → TransformComponent

## Инициализация физики в World

`World` автоматически создаёт физический мир при старте. После загрузки новой сцены нужно пересоздать рантайм:

```cpp
// После загрузки сцены
world.RebuildPhysicsRuntime();
// Перебирает все RigidBody3DComponent и создаёт для них IPhysicsBody + IPhysicsShape
```

При удалении сущности с физическим компонентом движок вызывает:

```cpp
world.DestroyRigidBodyRuntime(entityHandle);
// Это освобождает IPhysicsBody и IPhysicsShape из PhysX
```

## Настройки мира

```cpp
PhysicsWorldDesc desc;
desc.Gravity = {0.0f, -9.81f, 0.0f};  // Стандартная гравитация Земли
// desc.Gravity = {0, 0, 0};           // Нулевая гравитация (космос)

world.SetPhysicsWorldDesc(desc);
world.RebuildPhysicsRuntime();          // Применить
```

## Пример: падающий куб

```cpp
void OnAttach() override {
    auto& scene = Application::Get().GetWorld().GetScene();

    // Создать пол
    auto floor = scene.CreateEntity("Floor");
    floor.GetComponent<TransformComponent>().Scale = {10, 0.1f, 10};
    {
        RigidBody3DComponent rb;
        rb.BodyDesc.Type        = PhysicsBodyType::Static;
        rb.ShapeDesc.Type       = PhysicsColliderShapeType::Box;
        rb.ShapeDesc.HalfExtents = {5, 0.05f, 5};
        floor.AddComponent<RigidBody3DComponent>(rb);
    }

    // Создать куб
    auto cube = scene.CreateEntity("Cube");
    cube.GetComponent<TransformComponent>().Position = {0, 5, 0};
    {
        RigidBody3DComponent rb;
        rb.BodyDesc.Type        = PhysicsBodyType::Dynamic;
        rb.BodyDesc.Mass        = 1.0f;
        rb.ShapeDesc.Type       = PhysicsColliderShapeType::Box;
        rb.ShapeDesc.HalfExtents = {0.5f, 0.5f, 0.5f};
        rb.SyncMode             = RigidBodySyncMode::Auto;
        cube.AddComponent<RigidBody3DComponent>(rb);
    }

    Application::Get().GetWorld().RebuildPhysicsRuntime();
}
```

## Отключение физики

Если модуль `PhysicsPhysX` не загружен (или не установлен), движок продолжает работу без физики.  
`PhysicsFacade` возвращает `nullptr`, а `PhysicsSystem` просто пропускает обработку.
