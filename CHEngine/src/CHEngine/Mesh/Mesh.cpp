#include "chepch.h"
#include "Mesh.h"

namespace CHEngine {

	const VertexInputLayout& GetStandardMeshLayout()
	{
		// pos3 + normal3 + uv2 + color3 = 11 floats per vertex (44 bytes stride).
		static const VertexInputLayout kLayout = []() {
			Vector<VertexAttributeDesc> attrs = {
				VertexAttributeDesc{ VertexFormat::Float3, /*slot*/0, /*offset*/0,                 /*sem*/0 },
				VertexAttributeDesc{ VertexFormat::Float3, /*slot*/0, /*offset*/3 * sizeof(float), /*sem*/0 },
				VertexAttributeDesc{ VertexFormat::Float2, /*slot*/0, /*offset*/6 * sizeof(float), /*sem*/0 },
				VertexAttributeDesc{ VertexFormat::Float3, /*slot*/0, /*offset*/8 * sizeof(float), /*sem*/0 },
			};
			return VertexInputLayout(attrs, /*stride*/11 * sizeof(float));
		}();
		return kLayout;
	}

}
