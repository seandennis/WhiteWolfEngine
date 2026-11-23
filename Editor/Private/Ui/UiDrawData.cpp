#include "Ui/UiDrawData.h"

#include "Math/Color.h"
#include "Rendering/UiDrawCommand.h"
#include "Rendering/UiDrawList.h"
#include "Rendering/UiVertex.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiTextures.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace
{
	// Indices are copied as they are; CMake/WhiteWolfThirdParty.cmake makes Dear ImGui's as wide as the engine's.
	static_assert(std::is_same_v<ImDrawIdx, uint32>);

	/** Returns one edge of a clip rectangle as a whole pixel, kept within the surface so that it fits an integer. */
	int32 ToClipEdge(float Edge, float SurfaceSize)
	{
		return static_cast<int32>(std::clamp(std::round(Edge), 0.0f, SurfaceSize));
	}

	uint8 GetChannel(ImU32 PackedColor, int32 Shift)
	{
		return static_cast<uint8>((PackedColor >> Shift) & 0xFF);
	}
}

void FUiDrawData::Convert(const ImDrawData& DrawData, const FUiTextures& Textures, FUiDrawList& OutList)
{
	OutList.Vertices.clear();
	OutList.Indices.clear();
	OutList.Commands.clear();
	OutList.Vertices.reserve(static_cast<std::size_t>(DrawData.TotalVtxCount));
	OutList.Indices.reserve(static_cast<std::size_t>(DrawData.TotalIdxCount));

	// Dear ImGui's positions are measured from the corner of its display, which need not be zero.
	const ImVec2 Origin = DrawData.DisplayPos;
	const ImVec2 Size = DrawData.DisplaySize;

	for (const ImDrawList* const List : DrawData.CmdLists)
	{
		const int32 FirstVertex = static_cast<int32>(OutList.Vertices.size());
		const int32 FirstIndex = static_cast<int32>(OutList.Indices.size());

		for (const ImDrawVert& Source : List->VtxBuffer)
		{
			FUiVertex Vertex;
			Vertex.Position = FVector2f(Source.pos.x - Origin.x, Source.pos.y - Origin.y);
			Vertex.TextureCoordinate = FVector2f(Source.uv.x, Source.uv.y);
			Vertex.Color = FColor(GetChannel(Source.col, IM_COL32_R_SHIFT),
				GetChannel(Source.col, IM_COL32_G_SHIFT),
				GetChannel(Source.col, IM_COL32_B_SHIFT),
				GetChannel(Source.col, IM_COL32_A_SHIFT));
			OutList.Vertices.push_back(Vertex);
		}
		OutList.Indices.insert(OutList.Indices.end(), List->IdxBuffer.begin(), List->IdxBuffer.end());

		for (const ImDrawCmd& Source : List->CmdBuffer)
		{
			// A callback is how a widget asks for drawing of its own. Nothing in the editor does.
			WW_CHECKF(
				Source.UserCallback == nullptr, "A widget asked for a way of drawing that the editor does not have");

			FUiDrawCommand Command;
			Command.FirstIndex = FirstIndex + static_cast<int32>(Source.IdxOffset);
			Command.IndexCount = static_cast<int32>(Source.ElemCount);
			Command.VertexOffset = FirstVertex + static_cast<int32>(Source.VtxOffset);
			Command.ClipLeft = ToClipEdge(Source.ClipRect.x - Origin.x, Size.x);
			Command.ClipTop = ToClipEdge(Source.ClipRect.y - Origin.y, Size.y);
			Command.ClipRight = ToClipEdge(Source.ClipRect.z - Origin.x, Size.x);
			Command.ClipBottom = ToClipEdge(Source.ClipRect.w - Origin.y, Size.y);

			const FUiPicture Picture = Textures.Find(Source.GetTexID());
			Command.Texture = Picture.Texture;
			Command.RenderTarget = Picture.RenderTarget;
			OutList.Commands.push_back(Command);
		}
	}
}
