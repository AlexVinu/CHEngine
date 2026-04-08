# План: Editor / Play режимы

## Текущее состояние

Что **уже есть** в движке:
- `World::m_Active` — гейт Presentation-фазы (но нигде не выставляется в `true`)
- `SystemScheduler::setEnabled()` — включение/выключение систем поштучно
- `SceneSerializer` — полная JSON-сериализация сцены (v3)
- `UndoStack` — undo/redo в редакторе
- `PhysicsSystem`, `LifetimeSystem`, `RenderSystem` — зарегистрированы в World
- Toolbar в Sandbox — есть, но без Play/Pause/Stop кнопок

Что **отсутствует**:
- Enum состояния редактора (Edit / Play / Pause)
- UI кнопки Play ▶ / Pause ⏸ / Stop ⏹
- Снапшот сцены перед запуском (чтобы восстановить при Stop)
- Пауза физики
- Блокировка редактирования в Play-режиме
- Step-mode (покадровая симуляция)

---

## Целевая архитектура

```
          ┌──────────────────────────────────────────┐
          │              SceneViewLayer               │
          │                                          │
          │  EditorState: Edit / Play / Pause        │
          │  m_SceneSnapshot: json (снапшот сцены)   │
          │                                          │
          │  [▶ Play]  [⏸ Pause]  [⏹ Stop]          │
          └──────────┬───────────────────────────────┘
                     │ управляет
          ┌──────────▼───────────────────────────────┐
          │                 World                     │
          │                                          │
          │  m_Active  (Presentation вкл/выкл)       │
          │  m_Simulating (Simulation вкл/выкл)      │
          │  PhysicsWorld (создан / уничтожен)       │
          └──────────────────────────────────────────┘
```

### Режимы

| Режим | Simulation | Presentation | Физика | Редактирование | Камера |
|-------|-----------|-------------|--------|---------------|--------|
| **Edit** | ❌ | ✅ (рендер) | ❌ выключена | ✅ полное | Орбитальная (редактор) |
| **Play** | ✅ | ✅ | ✅ шаг каждый кадр | ❌ заблокировано | Орбитальная (или игровая) |
| **Pause** | ❌ (стоит) | ✅ (рендер) | ❌ (пауза) | ⚠️ только инспекция | Орбитальная (редактор) |

---

## Этапы реализации

### Этап 1 — EditorState и управление World

**Цель:** переключение режимов без UI, через код.

#### 1.1 Enum в SceneViewLayer

```
Файл: Sandbox/src/SceneViewLayer.h
```

```cpp
enum class EditorState : uint8_t {
    Edit,       // Редактор — симуляция не идёт
    Play,       // Игровой режим — всё работает
    Pause       // Пауза — симуляция остановлена, рендер идёт
};
```

Добавить поле `m_EditorState = EditorState::Edit` в SceneViewLayer.

#### 1.2 Управление World из SceneViewLayer

```
Файл: CHEngine/src/CHEngine/World/World.h / World.cpp
```

Добавить в `World`:

```cpp
void setSimulating(bool sim);   // Включает/выключает Simulation-фазу
bool isSimulating() const;
void setActive(bool active);    // Уже есть m_Active, добавить сеттер
bool isActive() const;
```

Изменить `World::update()`:

```cpp
void World::update(Timestep dt)
{
    if (m_Simulating)
        m_Scheduler.runPhase(SystemPhase::Simulation, *this, m_CommandBuffer, dt);

    if (m_Active)
        m_Scheduler.runPhase(SystemPhase::Presentation, *this, m_CommandBuffer, dt);

    // Flush deferred commands (всегда, даже в Edit)
    m_CommandBuffer.collectPendingDestroyHandles(...);
    m_CommandBuffer.flush(*m_Scene);
}
```

#### 1.3 Пауза физики в PhysicsSystem

```
Файл: CHEngine/src/CHEngine/World/Systems/PhysicsSystem.cpp
```

PhysicsSystem уже зависит от `World` — если `m_Simulating == false`, `runPhase(Simulation)` не вызовется. Физика автоматически встанет на паузу. Дополнительных изменений не нужно.

---

### Этап 2 — Снапшот сцены (Save / Restore)

**Цель:** при нажатии Play сохранить состояние, при Stop — восстановить.

#### 2.1 Снапшот через существующий SceneSerializer

```
Файл: Sandbox/src/SceneViewLayer.h
```

```cpp
nlohmann::json m_SceneSnapshot;     // Снапшот перед Play
bool           m_HasSnapshot = false;
```

#### 2.2 Методы Enter/Exit режимов

```
Файл: Sandbox/src/SceneViewLayer.cpp (новая секция или SceneViewLayer_PlayMode.cpp)
```

```cpp
void SceneViewLayer::EnterPlayMode()
{
    if (m_EditorState != EditorState::Edit) return;

    // 1. Сохранить снапшот текущей сцены в память (JSON)
    m_SceneSnapshot = SceneSerializer::SerializeToJson(m_Scene);
    m_HasSnapshot = true;

    // 2. Пересобрать физику (создать PhysX-тела для всех RigidBody)
    m_World.RebuildPhysicsRuntime();

    // 3. Включить симуляцию
    m_World.setSimulating(true);
    m_World.setActive(true);

    // 4. Очистить undo-стек (в Play undo не работает)
    m_UndoStack.Clear();

    m_EditorState = EditorState::Play;
}

void SceneViewLayer::EnterPauseMode()
{
    if (m_EditorState != EditorState::Play) return;

    m_World.setSimulating(false);
    // m_Active остаётся true — рендер продолжается

    m_EditorState = EditorState::Pause;
}

void SceneViewLayer::ResumeFromPause()
{
    if (m_EditorState != EditorState::Pause) return;

    m_World.setSimulating(true);

    m_EditorState = EditorState::Play;
}

void SceneViewLayer::StopPlayMode()
{
    if (m_EditorState == EditorState::Edit) return;

    // 1. Остановить симуляцию
    m_World.setSimulating(false);

    // 2. Очистить физику
    m_World.ClearPhysicsRuntime();

    // 3. Восстановить сцену из снапшота
    if (m_HasSnapshot)
    {
        SceneSerializer::DeserializeFromJson(m_Scene, m_SceneSnapshot, ...);
        m_HasSnapshot = false;
        m_SceneSnapshot = {};
    }

    // 4. Вернуть рендер в режим редактора
    m_World.setActive(true);

    m_EditorState = EditorState::Edit;
}
```

#### 2.3 SceneSerializer — методы in-memory

```
Файл: CHEngine/src/CHEngine/Scene/SceneSerializer.h / .cpp
```

Сейчас `SceneSerializer` умеет `SaveToFile` / `LoadFromFile`. Нужно добавить:

```cpp
static nlohmann::json SerializeToJson(const Scene& scene);
static void DeserializeFromJson(Scene& scene, const nlohmann::json& data,
                                 RenderResourceManager& resources);
```

Это рефакторинг: вынести логику из `SaveToFile`/`LoadFromFile` в эти методы. Файловые методы станут обёртками.

---

### Этап 3 — UI кнопки в Toolbar

**Цель:** Play ▶ / Pause ⏸ / Stop ⏹ в центре панели инструментов.

```
Файл: Sandbox/src/SceneViewLayer_Panels.cpp → DrawToolbar()
```

#### 3.1 Расположение

```
[ T  R  S | Local/World | Grid | ... ]   [ ▶  ⏸  ⏹ ]   [ Renderer | Theme | FPS ]
          левая часть                      центр              правая часть
```

#### 3.2 Логика кнопок

```cpp
// === Центр тулбара ===
float centerX = (toolbarMin.x + toolbarMax.x) * 0.5f;
ImGui::SetCursorScreenPos({centerX - 50, toolbarMin.y + pad});

// Play / Resume
bool isEdit = (m_EditorState == EditorState::Edit);
bool isPlay = (m_EditorState == EditorState::Play);
bool isPause = (m_EditorState == EditorState::Pause);

if (isEdit || isPause)
{
    if (ImGui::Button("▶"))
    {
        if (isEdit)  EnterPlayMode();
        else         ResumeFromPause();
    }
}
else // isPlay
{
    if (ImGui::Button("⏸"))
        EnterPauseMode();
}

ImGui::SameLine();

// Stop (активен только если Play или Pause)
ImGui::BeginDisabled(isEdit);
if (ImGui::Button("⏹"))
    StopPlayMode();
ImGui::EndDisabled();
```

#### 3.3 Горячие клавиши

| Клавиша | Действие |
|---------|---------|
| `Ctrl+P` / `Cmd+P` | Play / Resume |
| `Ctrl+Shift+P` | Pause |
| `Escape` | Stop (выход из Play) |

#### 3.4 Визуальная индикация режима

- **Edit**: обычный вид, стандартные цвета
- **Play**: зелёная рамка вокруг вьюпорта (или зелёный индикатор в тулбаре)
- **Pause**: жёлтая рамка / жёлтый индикатор

```cpp
if (m_EditorState == EditorState::Play)
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
else if (m_EditorState == EditorState::Pause)
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
```

---

### Этап 4 — Блокировка редактирования в Play

**Цель:** в Play/Pause режиме нельзя двигать объекты гизмо, менять компоненты, импортировать модели.

```
Файлы: SceneViewLayer_Panels.cpp, SceneViewLayer_Render.cpp
```

#### 4.1 Гизмо

```cpp
// В DrawGizmo():
if (m_EditorState != EditorState::Edit)
    return;  // Гизмо не рисуется в Play/Pause
```

#### 4.2 Панель свойств

```cpp
// В DrawPropsPanel():
bool readOnly = (m_EditorState != EditorState::Edit);

if (readOnly)
    ImGui::BeginDisabled(true);

// ... все редакторы ...

if (readOnly)
    ImGui::EndDisabled();
```

Значения показываются (для отладки), но не редактируются.

#### 4.3 Импорт моделей

```cpp
// В DrawScenePanel():
if (m_EditorState == EditorState::Edit)
{
    if (ImGui::Button("+ Import Model"))
        ImportModel();
}
```

Кнопки импорта/добавления скрыты в Play/Pause.

#### 4.4 Undo

Undo блокируется в Play/Pause — `m_UndoStack.Clear()` при входе в Play,
и `Cmd+Z` не реагирует.

---

### Этап 5 — Step Mode (покадровый)

**Цель:** при Pause нажать Step — симуляция продвинется на 1 кадр.

#### 5.1 Кнопка Step

```cpp
// Рядом с Pause/Stop:
ImGui::BeginDisabled(!isPause);
if (ImGui::Button("⏭"))  // Step
    StepOneFrame();
ImGui::EndDisabled();
```

Горячая клавиша: `Ctrl+Shift+Right` или `F10`.

#### 5.2 Реализация

```cpp
void SceneViewLayer::StepOneFrame()
{
    if (m_EditorState != EditorState::Pause) return;

    // Выполнить один кадр симуляции с фиксированным dt
    float fixedDt = 1.0f / 60.0f;
    m_World.setSimulating(true);
    m_World.update(Timestep(fixedDt));
    m_World.setSimulating(false);
}
```

Это полезно для отладки физики — можно смотреть покадрово как тела двигаются.

---

### Этап 6 — Камера в Play-режиме (опционально)

**Цель:** в Play камера может переключаться на игровую (CameraComponent с Primary=true).

#### 6.1 Переключение камеры

В Edit: всегда орбитальная редакторская камера.  
В Play: если есть сущность с `CameraComponent { Primary = true }` — использовать её.  
Если нет — оставить редакторскую.

```cpp
void SceneViewLayer::UpdateCamera(Timestep dt)
{
    if (m_EditorState == EditorState::Play)
    {
        // Найти Primary Camera
        auto* gameCam = FindPrimaryCamera(m_Scene);
        if (gameCam)
        {
            RenderFacade::SetSceneCamera(
                gameCam->GetViewMatrix(),
                gameCam->GetProjectionMatrix(),
                gameCam->GetPosition()
            );
            return;
        }
    }

    // Fallback: орбитальная камера редактора
    ApplyOrbit();
    RenderFacade::SetSceneCamera(m_Camera.GetViewMatrix(), ...);
}
```

---

## Порядок реализации

```
Этап 1 — EditorState + World API        (~ 1-2 часа)
  │
  ▼
Этап 2 — Снапшот сцены                  (~ 2-3 часа)
  │       Рефакторинг SceneSerializer
  │       EnterPlay / StopPlay логика
  ▼
Этап 3 — UI кнопки в тулбаре            (~ 1-2 часа)
  │       ▶ ⏸ ⏹ + горячие клавиши
  │       Визуальная индикация
  ▼
Этап 4 — Блокировка редактирования      (~ 1 час)
  │       Гизмо, свойства, импорт
  ▼
Этап 5 — Step Mode                       (~ 30 мин)
  │       Покадровая симуляция
  ▼
Этап 6 — Камера в Play (опционально)    (~ 1 час)
```

**Итого: ~7-10 часов работы**

---

## Файлы которые будут затронуты

| Файл | Что меняется |
|------|-------------|
| `CHEngine/src/CHEngine/World/World.h` | + `m_Simulating`, + `setSimulating()`, + `setActive()` |
| `CHEngine/src/CHEngine/World/World.cpp` | Изменение `update()` — гейт на `m_Simulating` |
| `CHEngine/src/CHEngine/Scene/SceneSerializer.h` | + `SerializeToJson()`, `DeserializeFromJson()` |
| `CHEngine/src/CHEngine/Scene/SceneSerializer.cpp` | Рефакторинг: вынести JSON-логику из файловых методов |
| `Sandbox/src/SceneViewLayer.h` | + `EditorState`, + `m_SceneSnapshot`, + методы Enter/Exit |
| `Sandbox/src/SceneViewLayer.cpp` | + `EnterPlayMode()`, `StopPlayMode()`, etc. |
| `Sandbox/src/SceneViewLayer_Panels.cpp` | + кнопки ▶⏸⏹, блокировка панелей |
| `Sandbox/src/SceneViewLayer_Render.cpp` | + блокировка гизмо, индикация режима |

---

## Риски и решения

| Риск | Решение |
|------|---------|
| MeshComponent не копируется (deleted copy ctor) | SerializeToJson сохраняет путь к модели; при восстановлении — повторная загрузка с диска |
| Потеря GPU-ресурсов при восстановлении сцены | DeserializeFromJson пересоздаёт VAO/текстуры через RenderFacade |
| Физика не восстанавливается корректно | ClearPhysicsRuntime() → восстановить сцену → RebuildPhysicsRuntime() при следующем Play |
| Play + окно ImGui перекрывает вьюпорт | Панели остаются, но редактирование заблокировано (BeginDisabled) |
| dt прыжок после Pause→Resume | Зажать dt максимумом (например, 0.05с) в первом кадре после Resume |
