#pragma once

#include <Core.h>
#include <CheStl/Vector.h>
#include "Render/Core/RenderTypes.h"

namespace CHEngine {

	// ─── Описание одного вершинного атрибута ────────────────────────
	struct VertexAttributeDesc {
		VertexFormat Format;        // Float3, UByte4_Norm и т.д.
		uint32_t     Slot;          // Индекс вершинного буфера (0, 1, 2...)
		uint32_t     Offset;        // Смещение в буфере
		uint32_t     SemanticIndex; // TEXCOORD0, TEXCOORD1 и т.д.

		VertexAttributeDesc() = default;

		VertexAttributeDesc(VertexFormat format, uint32_t slot, uint32_t offset, uint32_t semantic = 0)
			: Format(format), Slot(slot), Offset(offset), SemanticIndex(semantic) {
		}
	};

	// ─── Полное описание вершинного входа ───────────────────────────
	struct VertexInputLayout {
		Vector<VertexAttributeDesc> Attributes;
		Vector<uint32_t> Strides; // Stride для каждого слота

		VertexInputLayout() = default;

		// Удобный конструктор: один буфер, атрибуты подряд
		VertexInputLayout(const Vector<VertexAttributeDesc>& attrs, uint32_t stride)
			: Attributes(attrs), Strides({ stride }) {
		}

		// Несколько буферов
		VertexInputLayout(const Vector<VertexAttributeDesc>& attrs, const Vector<uint32_t>& strides)
			: Attributes(attrs), Strides(strides) {
		}
	};

}