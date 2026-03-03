# CHEngine

Кроссплатформенный игровой движок на C++ с поддержкой Windows, macOS и Linux.

## Возможности

- **Кроссплатформенная поддержка**: Сборка и запуск на Windows (MSVC), macOS (Clang) и Linux (GCC/Clang)
- **Рендеринг на OpenGL**: Аппаратное ускорение графики с GLAD и GLFW, совместимость с OpenGL 3.3–4.6
- **Модульная архитектура**: Подключаемые модули рендера с динамической загрузкой (.dll/.so/.dylib)
- **Система событий**: Event-driven приложение с слойной архитектурой
- **Пользовательская система памяти**: Кастомный аллокатор с поддержкой выравнивания на всех платформах
- **Логирование**: Структурированное логирование с spdlog

## Поддерживаемые платформы

| Платформа | Компилятор | Статус |
|----------|----------|--------|
| Windows  | MSVC 2022 | ✅ Поддерживается |
| macOS    | Clang    | ✅ Поддерживается |
| Linux    | GCC/Clang| ✅ Поддерживается |

## Структура проекта

```
CHEngine/
├── Core/                          # Ядро движка
│   ├── src/
│   │   ├── Core.h                # Платформенные макросы
│   │   ├── Log/                  # Система логирования
│   │   └── Memory/               # Утилиты для памяти
│   └── vendor/spdlog/            # Библиотека логирования
├── CHEngine/                       # Основной движок
│   ├── src/
│   │   ├── CHEngine/             # Приложение, события, слои
│   │   ├── Platform/Desktop/     # Кроссплатформенная реализация окна
│   │   └── Interfaces/           # Интерфейсы рендера
│   ├── vendor/
│   │   ├── GLFW/                 # Управление окном и вводом
│   │   └── GLM/                  # Математическая библиотека
│   └── CMakeLists.txt
├── Modules/
│   └── Rendering/RendererOGL/    # Модуль рендера OpenGL
├── Sandbox/                        # Пример приложения
└── CMakeLists.txt                 # Корневая конфигурация CMake
```

## Зависимости

### Обязательные
- **CMake** 3.20+
- **C++20** компилятор (MSVC 2022, Clang или GCC 10+)

### Встроенные (Vendored)
- **spdlog** - Быстрая библиотека логирования для C++
- **GLFW** - Управление окном и вводом
- **GLAD** - OpenGL загрузчик
- **GLM** - Математическая библиотека

## Сборка

### Windows (MSVC)

```bash
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Debug
```

### macOS (Clang)

```bash
mkdir build
cd build
cmake -G "Unix Makefiles" ..
cmake --build .
```

> **Примечание**: macOS поддерживает OpenGL максимум до версии 4.1. Движок автоматически запрашивает нужную версию.

### Linux (GCC/Clang)

```bash
mkdir build
cd build
cmake -G "Unix Makefiles" ..
cmake --build .
```

## Конфигурация

Проект использует CMake со следующими опциями:

```cmake
-DCHE_BUILD_SANDBOX=ON      # Сборка приложения-примера Sandbox
-DCHE_BUILD_OPENGL=ON       # Сборка модуля рендера OpenGL
```

## Платформенные дефайны

Движок автоматически определяет платформенные макросы:

- `CHE_PLATFORM_WINDOWS` - Платформа Windows
- `CHE_PLATFORM_APPLE` - Платформа macOS
- `CHE_PLATFORM_LINUX` - Платформа Linux
- `CHE_PLATFORM_UNIX` - Любая Unix-подобная система (macOS или Linux)

## Архитектурные решения

### Кроссплатформенные C++ макросы

```cpp
// Core.h предоставляет платформо-независимые макросы экспорта
#define CHENGINE_API __attribute__((visibility("default")))  // Unix
#define CHENGINE_API __declspec(dllexport)                   // Windows

// Макрос отладочного прерывания
#define CHE_DEBUGBREAK() __builtin_trap()      // GCC/Clang
#define CHE_DEBUGBREAK() __debugbreak()        // MSVC
```

### Флаги компилятора

**MSVC:**
```cmake
/utf-8 /W4              # Предупреждения
/Od /Zi                 # Отладка
/O2                     # Релиз
```

**GCC/Clang:**
```cmake
-Wall -Wextra -Wpedantic    # Предупреждения
-fvisibility=hidden         # Видимость символов
-g -O0                      # Отладка
-O2                         # Релиз
```

### Динамическая загрузка модулей

Платформо-специфичные имена модулей обрабатываются прозрачно:

```cpp
#if defined(CHE_PLATFORM_WINDOWS)
    LoadModule("RendererOGL.dll");
#elif defined(CHE_PLATFORM_APPLE)
    LoadModule("libRendererOGL.dylib");
#else
    LoadModule("libRendererOGL.so");
#endif
```

## Результаты компиляции

Артефакты сборки организованы по платформам и конфигурациям:

```
bin/
├── Debug-windows-x64/
├── Debug-macos-x64/
├── Debug-linux-x64/
├── Release-windows-x64/
└── ...
```

## Запуск примера

После сборки запустите приложение Sandbox:

```bash
# Windows
./bin/Debug-windows-x64/Sandbox/Sandbox.exe

# macOS
./bin/Debug-macos-x64/Sandbox/Sandbox

# Linux
./bin/Debug-linux-x64/Sandbox/Sandbox
```

## Разработка

### Добавление платформо-специфичного кода

Используйте платформенные дефайны для условной компиляции:

```cpp
#ifdef CHE_PLATFORM_WINDOWS
    // Код специфичный для Windows
#elif defined(CHE_PLATFORM_UNIX)
    // Код для Unix-подобных систем (Linux/macOS)
#endif
```

### Код специфичный для компилятора

Для различий между компиляторами используйте:

```cpp
#ifdef _MSC_VER
    // Специфично для MSVC
#else
    // GCC/Clang
#endif
```

## Ключевые изменения

### v1.0 — Кроссплатформенность
- Удалены Windows-only guards из CMakeLists.txt
- Добавлено определение платформ APPLE, LINUX, UNIX
- Заменены `__declspec(dllexport/import)` на `__attribute__((visibility()))`
- Унифицированы флаги компилятора (MSVC vs GCC/Clang)
- Переименован `WindowsWindow` → `DesktopWindow` (GLFW-based, платформо-независимый)
- Платформо-специфичная загрузка модулей (.dll/.so/.dylib)
- Исправлены ошибки dlopen/dlsym на не-Windows платформах

### v1.1 — Исправления macOS
- **OpenGL версия**: macOS поддерживает максимум OpenGL 4.1; добавлен `GLFW_OPENGL_FORWARD_COMPAT` для Core Profile
- **OpenGL DSA**: заменены функции OpenGL 4.5 (`glCreateBuffers`, `glCreateVertexArrays`) на совместимые с 4.1 (`glGenBuffers`, `glGenVertexArrays`)
- **MallocAllocator**: исправлен `posix_memalign` — выравнивание теперь минимально `sizeof(void*)`, что требует стандарт POSIX

## Лицензия

Смотрите файл LICENSE для деталей.

## Внесение вклада

Приветствуются вклады! Пожалуйста, убедитесь что:
- Код компилируется на Windows, macOS и Linux
- Платформо-специфичный код правильно защищен дефайнами
- Изменения CMakeLists.txt сохраняют кроссплатформенность
