#include "EngineContext.h"

namespace CHEngine
{
    EngineContext& GetEngineContext()
    {
        static EngineContext ctx;
        return ctx;
    }
}
