# Физика

## Обзор

Физический движок — NVIDIA PhysX (опциональный модуль `PhysicsPhysX`).  
Физика **полностью handle-based**, симметрично рендеру. Единственный публичный
виртуальный интерфейс — `IPhysicsFactory`; все PhysX-объекты живут в сторах внутри
бэкенда и адресуются хэндлами:

```cpp
using PhysWorldHandle = Handle<PhysWorldTag>;   // физический мир (PxScene)
using PhysBodyHandle  = Handle<PhysBodyTag>;    // тело (PxRigidActor)
using PhysShapeHandle = Handle<PhysShapeTag>;   // дескриптор шейпа
```

Прикладной код обычно не трогает физику напрямую — он добавляет
`RigidBody3DComponent`, а `PhysicsSystem` создаёт/удаляет тела и синхронизирует
трансформы.

## PhysicsSubsystem

Тонкий прокси над `IPhysicsFactory`. `Application` держит его как
`Scope<PhysicsSubsystem>` (`nullptr` = физика отключена). Доступ:

```cpp
PhysicsSubsystem* phys = Application::Get().Physics();
if (!phys) return;  // модуль не загружен / PhysicsEnabled=false

PhysWorldHandle world = phys->CreateWorld(PhysicsWorldDesc{ .Gravity = {0, -9.81f, 0} });
PhysShapeHandle shape = phys->CreateShape(PhysicsColliderShapeDesc{ /* Box по умолчанию */ });

// Операции над телами и queries — напрямую через фабрику
IPhysicsFactory* f = phys->GetFactory();
PhysBodyHandle body = f->CreateRigidBody(world, bodyDesc, shape);
f->AddBodyForce(body, {0, 10, 0}, PhysicsForceMode::Impulse);

phys->Delete(shape);
f->Delete(body);
phys->DestroyWorld(world);
```

`IPhysicsFactory` группы методов: World (`CreateWorld`/`Delete`/`SetGravity`/`StepSimulation`/`SetContactListener`),
Shapes (`CreateShape`/`Delete`), Bodies (`CreateRigidBody`/`Delete`/`GetBodyType`/`Get|SetBodyTransform`/
`SetBodyKinematicTarget`/`Get|SetBodyLinearVelocity`/`AddBodyForce`), Queries (`Raycast`/`OverlapSphere|Box|Capsule`).

## RigidBody3DComponent

Добавляется к сущности для включения физики.

```cpp
struct RigidBody3DComponent {
    PhysicsRigidBodyDesc     BodyDesc{};   // параметры тела
    PhysicsColliderShapeDesc ShapeDesc{};  // параметры шейпа
    RigidBodySyncMode        SyncMode = RigidBodySyncMode::Auto;
    PhysBodyHandle           Body{};       // handle, валиден только в Play-режиме
    PhysShapeHandle          Shape{};      // handle дескриптора шейпа
    bool                     SynchronisedTransform = true;
};
```

```cpp
RigidBody3DComponent rb;
rb.BodyDesc.Type         = PhysicsBodyType::Dynamic;  // Static / Dynamic / Kinematic
rb.BodyDesc.Mass         = 1.0f;
rb.ShapeDesc.Type        = PhysShapeType::Box;        // Box / Sphere / Capsule
rb.ShapeDesc.HalfExtents = {0.5f, 0.5f, 0.5f};
rb.SyncMode              = RigidBodySyncMode::Auto;

scene.TryGetEntity(handle)->AddComponent<RigidBody3DComponent>(rb);
```

> Поля `Body` / `Shape` заполняет `PhysicsSystem` при входе в Play-режим; вне его они невалидны.

### Форма коллайдера

`PhysicsColliderShapeDesc` — POD-дескриптор; внутри `PhysicsSubsystem` он
конвертируется в `variant<BoxDesc, SphereDesc, CapsuleDesc>`.

| `Type` (`PhysShapeType`) | Поля | Описание |
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
auto& tc = entity->GetComponent<TransformComponent>();
tc.ObjectTransform.Position += velocity * dt;
tc.MarkDirty();   // обязательно — иначе write-блок PhysicsSystem пропустит правку
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
1. Для каждого `RigidBody3DComponent`: если `SyncMode` пишет в физику **и** `Transform.Dirty` — скопировать `TransformComponent` → тело (`SetBodyTransform` / `SetBodyKinematicTarget`)
2. Шаг симуляции PhysX: `IPhysicsFactory::StepSimulation(world, dt)`
3. Если `SyncMode` читает из физики — скопировать `GetBodyTransform` → `TransformComponent` (и сбросить `Dirty = false`)

Создание/удаление тел `PhysicsSystem` делает через хуки `DeferredOps`
(`SubscribeOnComponentAdded/Removed<RigidBody3DComponent>`) и при входе/выходе из
Play-режима (`OnBegin` / `OnEnd`).

## Инициализация физики в World

Физический мир создаётся при переходе `World` в состояние, включающее симуляцию
(`Simulating` / `SimulatingWithoutPresenting`). Описание мира задаётся заранее:

```cpp
world.SetPhysicsWorldDesc(PhysicsWorldDesc{
    .Gravity = {0.0f, -9.81f, 0.0f},   // {0,0,0} — невесомость
});

world.SetState(WorldState::Simulating);          // тела создаются здесь
PhysWorldHandle pw = world.GetPhysicsRuntimeWorld();
```

При выходе из Play-режима (`SetState(WorldState::Presenting)`) тела уничтожаются, а
хэндлы `Body`/`Shape` в компонентах становятся невалидными.

## Пример: падающий куб

```cpp
void OnAttach() override {
    Scene& scene = *m_World.GetSceneRef();

    auto makeBox = [&](const char* name, glm::vec3 pos, glm::vec3 half,
                       PhysicsBodyType type) {
        EntityHandle h = scene.CreateEntity(name);
        Entity* e = scene.TryGetEntity(h);

        auto& tc = e->AddComponent<TransformComponent>();
        tc.ObjectTransform.Position = pos;
        tc.ObjectTransform.Scale    = half * 2.0f;
        tc.MarkDirty();

        RigidBody3DComponent rb;
        rb.BodyDesc.Type         = type;
        rb.ShapeDesc.Type        = PhysShapeType::Box;
        rb.ShapeDesc.HalfExtents = half;
        rb.SyncMode              = RigidBodySyncMode::Auto;
        e->AddComponent<RigidBody3DComponent>(rb);
    };

    makeBox("Floor", {0, 0, 0}, {5.0f, 0.05f, 5.0f}, PhysicsBodyType::Static);
    makeBox("Cube",  {0, 5, 0}, {0.5f, 0.5f, 0.5f}, PhysicsBodyType::Dynamic);

    // Тела создаются при входе в Play-режим:
    m_World.SetState(WorldState::Simulating);
}
```

## Queries и контакты

Лучи и overlap-запросы идут через `IPhysicsFactory` по миру; результаты содержат
`PhysBodyHandle` (обратный индекс по `PxRigidActor*` внутри бэкенда):

```cpp
IPhysicsFactory* f = Application::Get().Physics()->GetFactory();

PhysicsRaycastHit hit;
if (f->Raycast(world, origin, dir, /*maxDistance=*/100.0f, hit))
    CHE_INFO("Hit body {} at {}", hit.Body.Index(), glm::to_string(hit.Position));

PhysicsOverlapResult res;
f->OverlapSphere(world, center, /*radius=*/2.0f, res);
```

Контакты — через `IPhysicsContactListener`:

```cpp
struct MyListener : IPhysicsContactListener {
    void OnContact(const PhysicsContactEvent& e) override {
        // e.BodyA, e.BodyB, e.Point, e.Normal
    }
};
f->SetContactListener(world, &listener);
```

## Отключение физики

Если модуль `PhysicsPhysX` не загружен (`PhysicsEnabled = false` или модуль
отсутствует), движок работает без физики: `Application::Get().Physics()` возвращает
`nullptr`, `World` не создаёт физический мир, а `PhysicsSystem` пропускает обработку.
