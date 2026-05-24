#pragma once

#include "Memory/Handle.h"

namespace CHEngine {

	struct PhysWorldTag {};
	struct PhysBodyTag {};
	struct PhysShapeTag {};

	using PhysWorldHandle = Handle<PhysWorldTag>;
	using PhysBodyHandle = Handle<PhysBodyTag>;
	using PhysShapeHandle = Handle<PhysShapeTag>;

}
