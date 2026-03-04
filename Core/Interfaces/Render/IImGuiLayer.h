#pragma once

namespace CHEngine {
    class IImGuiLayer {
    public:
        virtual ~IImGuiLayer() = default;
        virtual void Begin() = 0;
        virtual void End() = 0;
    };
}
