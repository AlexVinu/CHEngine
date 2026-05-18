# Ввод и события

## Input — опрос состояния

`Input` работает на основе опроса состояния, а не очереди событий ОС.  
Это стандартная практика для игровых движков: нет задержки, нет пропущенных нажатий.

В начале каждого кадра вызывается `Input::BeginFrame(window)`, который:
- Сохраняет состояние прошлого кадра
- Запрашивает текущее состояние через `glfwGetKey()` / `glfwGetMouseButton()`

### Клавиатура

```cpp
using namespace CHEngine;

// Клавиша удерживается (true каждый кадр пока зажата)
if (Input::IsKeyDown(Key::W))
    transform.Position += forward * speed * dt;

// Клавиша нажата именно в этом кадре
if (Input::IsKeyPressed(Key::Space))
    Jump();

// Клавиша отпущена именно в этом кадре
if (Input::IsKeyReleased(Key::LeftShift))
    StopSprinting();
```

### Мышь

```cpp
// Кнопки мыши
if (Input::IsMouseButtonDown(Mouse::ButtonLeft))
    Shoot();

if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
    OpenContextMenu();

// Позиция
float x = Input::GetMouseX();
float y = Input::GetMouseY();

// Дельта (смещение за кадр) — для вращения камеры
float dx = Input::GetMouseDeltaX();
float dy = Input::GetMouseDeltaY();

camera.Yaw   += dx * sensitivity;
camera.Pitch -= dy * sensitivity;
```

### Коды клавиш

```cpp
namespace CHEngine::Key {
    // Буквы
    A, B, C, ..., Z

    // Цифры
    D0, D1, ..., D9

    // Функциональные
    Space, Enter, Escape, Tab, Backspace
    LeftShift, RightShift, LeftCtrl, RightCtrl
    LeftAlt, RightAlt
    Up, Down, Left, Right
    F1, ..., F12

    // Numpad
    KP0, ..., KP9, KPAdd, KPSubtract, KPMultiply, KPDivide, KPEnter
}

namespace CHEngine::Mouse {
    ButtonLeft, ButtonRight, ButtonMiddle
    Button4, Button5, Button6, Button7
}
```

---

## Event System — событийная система

События используются для **нечастых** действий: изменение размера окна, закрытие, нажатие клавиши в UI.  
Для постоянного ввода в игровой логике используйте `Input` (см. выше).

### Поток событий

```
Window/GLFW → Application::OnEvent()
                  │
                  ├─ World::OnEvent() → SystemScheduler → системы
                  │
                  └─ LayerStack (в обратном порядке) → слои
                       Если e.Handled = true → дальше не передаётся
```

### EventDispatcher

```cpp
void OnEvent(Event& e) override {
    EventDispatcher dispatcher(e);

    // Диспетчер вызывает лямбду только если тип события совпадает
    dispatcher.Dispatch<WindowResizedEvent>([this](WindowResizedEvent& e) {
        m_Width  = e.GetWidth();
        m_Height = e.GetHeight();
        return false;  // Не поглощать событие
    });

    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
        if (e.GetKeyCode() == Key::Escape) {
            Application::Get().Close();
            return true;  // Поглотить событие
        }
        return false;
    });
}
```

### Типы событий

**Клавиатура:**
```cpp
KeyPressedEvent   e;  // e.GetKeyCode(), e.IsRepeat()
KeyReleasedEvent  e;  // e.GetKeyCode()
KeyRepeatedEvent  e;  // e.GetKeyCode()
```

**Мышь:**
```cpp
MouseMovedEvent          e;  // e.GetX(), e.GetY()
MouseScrolledEvent       e;  // e.GetXOffset(), e.GetYOffset()
MouseButtonPressedEvent  e;  // e.GetMouseButton()
MouseButtonReleasedEvent e;  // e.GetMouseButton()
```

**Приложение/Окно:**
```cpp
WindowResizedEvent  e;  // e.GetWidth(), e.GetHeight()
WindowClosedEvent   e;  // (без данных)
WindowFocusEvent    e;
WindowLostFocusEvent e;
AppTickEvent        e;
AppUpdateEvent      e;
AppRenderEvent      e;
```

---

## Layer — слой приложения

`Layer` — основная единица для организации кода приложения.

```cpp
class GameLayer : public CHEngine::Layer {
public:
    GameLayer() : Layer("GameLayer") {}

    void OnAttach() override {
        // Вызывается при добавлении в LayerStack
        // Здесь: загрузка ресурсов, создание сцены
    }

    void OnDetach() override {
        // Вызывается при удалении из LayerStack
        // Здесь: освобождение ресурсов
    }

    void OnUpdate(CHEngine::Timestep dt) override {
        // Вызывается каждый кадр (до рендеринга)
        float seconds = dt.GetSeconds();
    }

    void OnEvent(CHEngine::Event& e) override {
        // Вызывается при событии
        // Если установить e.Handled = true — вышестоящие слои не получат событие
    }

    void OnImGuiRender() override {
        // Вызывается каждый кадр между UISubsystem::Begin/End
        ImGui::Begin("My Panel");
        ImGui::End();
    }
};
```

### LayerStack

```cpp
// В Application::Application():
PushLayer(new GameLayer());       // Обычный слой
PushOverlay(new DebugOverlay());  // Оверлей — поверх всех слоёв
```

**Порядок обновления:** слои снизу вверх (в порядке добавления).  
**Порядок событий:** сверху вниз (оверлеи получают события первыми).

---

## Camera — камера

```cpp
CHEngine::Camera camera;
camera.FOV         = 60.0f;
camera.NearClip    = 0.1f;
camera.FarClip     = 500.0f;
camera.AspectRatio = 16.0f / 9.0f;

// Управление
camera.Yaw   += Input::GetMouseDeltaX() * 0.1f;
camera.Pitch += Input::GetMouseDeltaY() * 0.1f;
// Pitch автоматически зажимается в диапазоне [-89°, +89°]

// Передать в рендерер
Application::Get().Render().SetSceneCamera(cameraUBO);
```

Либо через `CameraComponent` в ECS — `RenderSystem` сам найдёт Primary-камеру:

```cpp
auto camEntity = scene.CreateEntity("MainCamera");
auto& cc = camEntity.AddComponent<CameraComponent>();
cc.FOV     = 60.0f;
cc.Primary = true;  // Используется RenderSystem
```
