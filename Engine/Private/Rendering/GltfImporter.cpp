#include "Rendering/GltfImporter.h"

#include "Core/FileHelper.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/Material.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/StaticMeshVertex.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <cgltf.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace
{
	/** The extensions that a file may require and still be imported. */
	constexpr std::string_view SupportedExtensions[] = {"KHR_lights_punctual", "KHR_materials_emissive_strength"};

	/** Frees what cgltf allocated for a file. */
	struct FGltfDataDeleter
	{
		void operator()(cgltf_data* Data) const
		{
			cgltf_free(Data);
		}
	};

	/**
	 * Keeps the content of the files that cgltf asks for, such as the .bin file beside a .gltf file, for as long
	 * as an import runs. cgltf is given pointers into it.
	 */
	struct FGltfFileStore
	{
		std::vector<std::unique_ptr<std::string>> Files;

		/** Why the last file could not be read. */
		std::string LastError;
	};

	std::filesystem::path PathFromUtf8(std::string_view Text)
	{
		return std::filesystem::path(std::u8string(Text.begin(), Text.end()));
	}

	/** Reads a file for cgltf through the engine's file functions, which handle paths that are not ASCII. */
	cgltf_result ReadFileForGltf(const cgltf_memory_options* /*MemoryOptions*/,
		const cgltf_file_options* FileOptions,
		const char* Path,
		cgltf_size* Size,
		void** Data)
	{
		FGltfFileStore& Store = *static_cast<FGltfFileStore*>(FileOptions->user_data);

		std::unique_ptr<std::string> Content = std::make_unique<std::string>();
		if (!FFileHelper::LoadFileToString(PathFromUtf8(Path), *Content, Store.LastError))
		{
			return cgltf_result_file_not_found;
		}

		// A buffer says how long it is; a file that is shorter would be read beyond its end.
		const cgltf_size ExpectedSize = *Size;
		if (Content->size() < ExpectedSize)
		{
			Store.LastError = std::format(
				"'{}' is {} bytes long, but the glTF file says it holds {} bytes", Path, Content->size(), ExpectedSize);
			return cgltf_result_data_too_short;
		}

		*Size = Content->size();
		*Data = Content->data();
		Store.Files.push_back(std::move(Content));
		return cgltf_result_success;
	}

	void ReleaseFileForGltf(
		const cgltf_memory_options* /*MemoryOptions*/, const cgltf_file_options* /*FileOptions*/, void* /*Data*/)
	{
		// The file store releases everything together, when the import is over.
	}

	std::string_view DescribeResult(cgltf_result Result)
	{
		switch (Result)
		{
			case cgltf_result_data_too_short:
				return "the file ends before its data does";
			case cgltf_result_unknown_format:
				return "the file is neither a .gltf nor a .glb file";
			case cgltf_result_invalid_json:
				return "the file's JSON text is not valid";
			case cgltf_result_invalid_gltf:
				return "the file's content is not valid glTF";
			case cgltf_result_file_not_found:
				return "a file it refers to cannot be read";
			case cgltf_result_io_error:
				return "a file it refers to cannot be read";
			case cgltf_result_out_of_memory:
				return "there is not enough memory to read it";
			case cgltf_result_legacy_gltf:
				return "the file is glTF 1.0, and only glTF 2.0 is supported";
			default:
				return "the file could not be read";
		}
	}

	/** Decodes base64 text, as found in "data:" addresses. Returns nothing if the text is not valid base64. */
	std::optional<std::vector<std::byte>> DecodeBase64(std::string_view Text)
	{
		std::vector<std::byte> Bytes;
		Bytes.reserve(Text.size() / 4 * 3);

		uint32 Accumulator = 0;
		int32 BitCount = 0;
		for (const char Character : Text)
		{
			uint32 Value = 0;
			if (Character >= 'A' && Character <= 'Z')
			{
				Value = static_cast<uint32>(Character - 'A');
			}
			else if (Character >= 'a' && Character <= 'z')
			{
				Value = static_cast<uint32>(Character - 'a') + 26;
			}
			else if (Character >= '0' && Character <= '9')
			{
				Value = static_cast<uint32>(Character - '0') + 52;
			}
			else if (Character == '+' || Character == '-')
			{
				Value = 62;
			}
			else if (Character == '/' || Character == '_')
			{
				Value = 63;
			}
			else if (Character == '=')
			{
				// Padding: the data is complete.
				break;
			}
			else
			{
				return std::nullopt;
			}

			Accumulator = (Accumulator << 6) | Value;
			BitCount += 6;
			if (BitCount >= 8)
			{
				BitCount -= 8;
				Bytes.push_back(static_cast<std::byte>((Accumulator >> BitCount) & 0xffu));
			}
		}

		return Bytes;
	}

	/** Reads every element of an accessor as Components floats each. False if the accessor is of another shape. */
	bool ReadFloats(
		const cgltf_accessor& Accessor, cgltf_type Type, std::size_t Components, std::vector<float>& OutValues)
	{
		if (Accessor.type != Type)
		{
			return false;
		}

		OutValues.assign(Accessor.count * Components, 0.0f);
		if (OutValues.empty())
		{
			return true;
		}

		return cgltf_accessor_unpack_floats(&Accessor, OutValues.data(), OutValues.size()) == OutValues.size();
	}

	ETextureWrap ToTextureWrap(cgltf_wrap_mode Mode)
	{
		switch (Mode)
		{
			case cgltf_wrap_mode_clamp_to_edge:
				return ETextureWrap::ClampToEdge;
			case cgltf_wrap_mode_mirrored_repeat:
				return ETextureWrap::MirroredRepeat;
			case cgltf_wrap_mode_repeat:
			default:
				return ETextureWrap::Repeat;
		}
	}

	/**
	 * Splits a node's matrix into a location, rotation and scale. A matrix with shear, which a transform cannot
	 * represent, comes out as the nearest transform without it.
	 */
	FTransform DecomposeMatrix(const cgltf_float* Matrix)
	{
		// The 16 numbers are the matrix column by column.
		FVector3f AxisX(Matrix[0], Matrix[1], Matrix[2]);
		FVector3f AxisY(Matrix[4], Matrix[5], Matrix[6]);
		FVector3f AxisZ(Matrix[8], Matrix[9], Matrix[10]);

		FVector3f Scale(glm::length(AxisX), glm::length(AxisY), glm::length(AxisZ));
		// A mirroring matrix turns the axes inside out; one negative scale puts them right again.
		if (glm::determinant(glm::mat3(AxisX, AxisY, AxisZ)) < 0.0f)
		{
			Scale.x = -Scale.x;
		}

		const bool bHasRotation = Scale.x != 0.0f && Scale.y != 0.0f && Scale.z != 0.0f;

		FTransform Transform;
		Transform.Location = FVector(Matrix[12], Matrix[13], Matrix[14]);
		Transform.Scale = Scale;
		if (bHasRotation)
		{
			Transform.Rotation =
				glm::normalize(glm::quat_cast(glm::mat3(AxisX / Scale.x, AxisY / Scale.y, AxisZ / Scale.z)));
		}
		return Transform;
	}

	/** One import in progress: the parsed file, what has been created from it so far, and the warnings. */
	class FGltfImport
	{
	public:
		FGltfImport(const cgltf_data& InData, std::filesystem::path InPath, std::vector<std::string>* InWarnings)
			: Data(InData)
			, Path(std::move(InPath))
			, Warnings(InWarnings)
		{
		}

		/** Creates the entities of the file's scene below a new entity named after the file, and returns that. */
		WEntity* CreateEntities(WScene& Scene)
		{
			WarnAboutWhatIsLeftOut();

			WEntity* const Root = Scene.CreateEntity(FFileHelper::PathToUtf8(Path.stem()));
			WW_CHECK(Root != nullptr);

			/** A node that is still to be turned into an entity, with the entity of its parent. */
			struct FPendingNode
			{
				const cgltf_node* Node = nullptr;
				WEntity* Parent = nullptr;
			};

			// Pushed in reverse, so that entities are created in the order of the file.
			std::vector<FPendingNode> Pending;
			const std::span<cgltf_node* const> RootNodes = GetRootNodes();
			for (std::size_t Index = RootNodes.size(); Index > 0; --Index)
			{
				Pending.push_back({RootNodes[Index - 1], Root});
			}

			while (!Pending.empty())
			{
				const FPendingNode Current = Pending.back();
				Pending.pop_back();

				WEntity* const Entity = CreateEntity(Scene, *Current.Node, Current.Parent);
				for (std::size_t Index = Current.Node->children_count; Index > 0; --Index)
				{
					Pending.push_back({Current.Node->children[Index - 1], Entity});
				}
			}

			return Root;
		}

	private:
		void Warn(const std::string& Message)
		{
			WW_LOG(LogRendering, Warning, "Importing '{}': {}", FFileHelper::PathToUtf8(Path), Message);
			if (Warnings != nullptr)
			{
				Warnings->push_back(Message);
			}
		}

		/** Says once, for the whole file, which kinds of content are not imported. */
		void WarnAboutWhatIsLeftOut()
		{
			if (Data.animations_count > 0)
			{
				Warn(std::format("the file's {} animations are not imported", Data.animations_count));
			}
			if (Data.skins_count > 0)
			{
				Warn(std::format(
					"the file's {} skins are not imported; skinned meshes stay in their rest pose", Data.skins_count));
			}
			if (Data.cameras_count > 0)
			{
				Warn(std::format("the file's {} cameras are not imported", Data.cameras_count));
			}
		}

		/** The nodes at the top of the scene the file shows by default, or of its first scene. */
		std::span<cgltf_node* const> GetRootNodes() const
		{
			const cgltf_scene* const Scene =
				Data.scene != nullptr ? Data.scene : (Data.scenes_count > 0 ? &Data.scenes[0] : nullptr);
			if (Scene == nullptr)
			{
				return {};
			}
			return std::span<cgltf_node* const>(Scene->nodes, Scene->nodes_count);
		}

		std::string DescribeNode(const cgltf_node& Node) const
		{
			return Node.name != nullptr ? std::string(Node.name)
										: std::format("Node{}", cgltf_node_index(&Data, &Node));
		}

		WEntity* CreateEntity(WScene& Scene, const cgltf_node& Node, WEntity* Parent)
		{
			WEntity* const Entity = Scene.CreateEntity(DescribeNode(Node), Parent);
			WW_CHECK(Entity != nullptr);

			FTransform Transform;
			if (Node.has_matrix != 0)
			{
				Transform = DecomposeMatrix(Node.matrix);
			}
			else
			{
				// A glTF rotation is stored x, y, z, w.
				Transform.Location = FVector(Node.translation[0], Node.translation[1], Node.translation[2]);
				Transform.Rotation =
					glm::normalize(FQuat(Node.rotation[3], Node.rotation[0], Node.rotation[1], Node.rotation[2]));
				Transform.Scale = FVector3f(Node.scale[0], Node.scale[1], Node.scale[2]);
			}
			Entity->SetRelativeTransform(Transform);

			if (Node.mesh != nullptr)
			{
				AddMeshComponents(*Entity, *Node.mesh);
			}
			if (Node.light != nullptr)
			{
				AddLightComponent(*Entity, *Node.light);
			}
			return Entity;
		}

		/** Adds one mesh component for each part of a glTF mesh, which has one material per part. */
		void AddMeshComponents(WEntity& Entity, const cgltf_mesh& Mesh)
		{
			for (cgltf_size Index = 0; Index < Mesh.primitives_count; ++Index)
			{
				const cgltf_primitive& Primitive = Mesh.primitives[Index];
				WStaticMesh* const StaticMesh = GetMesh(Mesh, Primitive);
				if (StaticMesh == nullptr)
				{
					continue;
				}

				WStaticMeshComponent* const Component = Entity.AddComponent<WStaticMeshComponent>();
				WW_CHECK(Component != nullptr);
				Component->Mesh = StaticMesh;
				Component->Material = GetMaterial(Primitive.material);
			}
		}

		void AddLightComponent(WEntity& Entity, const cgltf_light& Light)
		{
			const FLinearColor Color(Light.color[0], Light.color[1], Light.color[2]);
			if (Light.type == cgltf_light_type_directional)
			{
				WDirectionalLightComponent* const Component = Entity.AddComponent<WDirectionalLightComponent>();
				WW_CHECK(Component != nullptr);
				Component->Color = Color;
				Component->Intensity = Light.intensity;
			}
			else if (Light.type == cgltf_light_type_point)
			{
				WPointLightComponent* const Component = Entity.AddComponent<WPointLightComponent>();
				WW_CHECK(Component != nullptr);
				Component->Color = Color;
				Component->Intensity = Light.intensity;
				Component->Range = Light.range;
			}
			else
			{
				Warn(std::format("the light of '{}' is a spot light, which is not imported", Entity.GetName()));
			}
		}

		std::string DescribeMesh(const cgltf_mesh& Mesh) const
		{
			return Mesh.name != nullptr ? std::format("mesh '{}'", Mesh.name)
										: std::format("mesh {}", cgltf_mesh_index(&Data, &Mesh));
		}

		/** Returns the static mesh for one part of a glTF mesh, created on first use. Null if it cannot be imported. */
		WStaticMesh* GetMesh(const cgltf_mesh& Mesh, const cgltf_primitive& Primitive)
		{
			const auto Existing = Meshes.find(&Primitive);
			if (Existing != Meshes.end())
			{
				return Existing->second;
			}

			WStaticMesh* StaticMesh = nullptr;
			std::vector<FStaticMeshVertex> Vertices;
			std::vector<uint32> Indices;
			std::string Problem;
			if (ReadGeometry(Primitive, Vertices, Indices, Problem))
			{
				StaticMesh = NewObject<WStaticMesh>();
				if (!StaticMesh->SetGeometry(std::move(Vertices), std::move(Indices), Problem))
				{
					StaticMesh = nullptr;
				}
			}
			if (StaticMesh == nullptr)
			{
				Warn(std::format("a part of {} is not imported: {}", DescribeMesh(Mesh), Problem));
			}

			if (Primitive.targets_count > 0)
			{
				Warn(std::format("the morph targets of {} are not imported", DescribeMesh(Mesh)));
			}

			Meshes.emplace(&Primitive, StaticMesh);
			return StaticMesh;
		}

		/** Reads the triangles of one part of a glTF mesh. On failure OutProblem says why. */
		bool ReadGeometry(const cgltf_primitive& Primitive,
			std::vector<FStaticMeshVertex>& OutVertices,
			std::vector<uint32>& OutIndices,
			std::string& OutProblem) const
		{
			if (Primitive.type != cgltf_primitive_type_triangles)
			{
				OutProblem = "it is made of points, lines, triangle strips or triangle fans, and only triangle lists "
							 "are supported";
				return false;
			}
			if (Primitive.has_draco_mesh_compression != 0)
			{
				OutProblem = "it is compressed with Draco, which is not supported";
				return false;
			}

			const cgltf_accessor* const Positions = cgltf_find_accessor(&Primitive, cgltf_attribute_type_position, 0);
			const cgltf_accessor* const Normals = cgltf_find_accessor(&Primitive, cgltf_attribute_type_normal, 0);
			const cgltf_accessor* const TextureCoordinates =
				cgltf_find_accessor(&Primitive, cgltf_attribute_type_texcoord, 0);

			std::vector<float> PositionValues;
			if (Positions == nullptr || !ReadFloats(*Positions, cgltf_type_vec3, 3, PositionValues))
			{
				OutProblem = "it has no positions, or they are not three numbers each";
				return false;
			}
			const std::size_t VertexCount = Positions->count;

			std::vector<float> NormalValues;
			const bool bHasNormals = Normals != nullptr && Normals->count == VertexCount &&
				ReadFloats(*Normals, cgltf_type_vec3, 3, NormalValues);

			std::vector<float> CoordinateValues;
			const bool bHasCoordinates = TextureCoordinates != nullptr && TextureCoordinates->count == VertexCount &&
				ReadFloats(*TextureCoordinates, cgltf_type_vec2, 2, CoordinateValues);

			OutVertices.assign(VertexCount, FStaticMeshVertex());
			for (std::size_t Index = 0; Index < VertexCount; ++Index)
			{
				FStaticMeshVertex& Vertex = OutVertices[Index];
				Vertex.Position =
					FVector3f(PositionValues[Index * 3], PositionValues[Index * 3 + 1], PositionValues[Index * 3 + 2]);
				if (bHasNormals)
				{
					Vertex.Normal =
						FVector3f(NormalValues[Index * 3], NormalValues[Index * 3 + 1], NormalValues[Index * 3 + 2]);
				}
				if (bHasCoordinates)
				{
					Vertex.TextureCoordinate = FVector2f(CoordinateValues[Index * 2], CoordinateValues[Index * 2 + 1]);
				}
			}

			if (Primitive.indices != nullptr)
			{
				OutIndices.assign(Primitive.indices->count, 0);
				const bool bRead = OutIndices.empty() ||
					cgltf_accessor_unpack_indices(
						Primitive.indices, OutIndices.data(), sizeof(uint32), OutIndices.size()) == OutIndices.size();
				if (!bRead)
				{
					OutProblem = "its indices cannot be read";
					return false;
				}
			}
			else
			{
				// Without indices, every three vertices are a triangle.
				OutIndices.resize(VertexCount);
				for (std::size_t Index = 0; Index < VertexCount; ++Index)
				{
					OutIndices[Index] = static_cast<uint32>(Index);
				}
			}

			if (!bHasNormals)
			{
				return MakeFlatNormals(OutVertices, OutIndices, OutProblem);
			}
			return true;
		}

		/**
		 * Gives a mesh without normals the lighting glTF asks for: every triangle flat. Each triangle gets its own
		 * three vertices, with the triangle's own direction as their normal.
		 */
		static bool MakeFlatNormals(
			std::vector<FStaticMeshVertex>& Vertices, std::vector<uint32>& Indices, std::string& OutProblem)
		{
			std::vector<FStaticMeshVertex> FlatVertices;
			FlatVertices.reserve(Indices.size());
			for (std::size_t First = 0; First + 2 < Indices.size(); First += 3)
			{
				const bool bIsInRange = Indices[First] < Vertices.size() && Indices[First + 1] < Vertices.size() &&
					Indices[First + 2] < Vertices.size();
				if (!bIsInRange)
				{
					OutProblem = "an index names a vertex that does not exist";
					return false;
				}

				FStaticMeshVertex Corners[3] = {
					Vertices[Indices[First]], Vertices[Indices[First + 1]], Vertices[Indices[First + 2]]};
				const FVector3f Perpendicular =
					glm::cross(Corners[1].Position - Corners[0].Position, Corners[2].Position - Corners[0].Position);
				const float Length = glm::length(Perpendicular);
				// A triangle without area faces nowhere; any direction will do for something that is never seen.
				const FVector3f Normal = Length > 0.0f ? Perpendicular / Length : FVector3f(0.0f, 1.0f, 0.0f);
				for (FStaticMeshVertex& Corner : Corners)
				{
					Corner.Normal = Normal;
					FlatVertices.push_back(Corner);
				}
			}

			Indices.resize(FlatVertices.size());
			for (std::size_t Index = 0; Index < Indices.size(); ++Index)
			{
				Indices[Index] = static_cast<uint32>(Index);
			}
			Vertices = std::move(FlatVertices);
			return true;
		}

		std::string DescribeMaterial(const cgltf_material& Material) const
		{
			return Material.name != nullptr ? std::format("material '{}'", Material.name)
											: std::format("material {}", cgltf_material_index(&Data, &Material));
		}

		/** Returns the engine material for a glTF material, created on first use. Null for "no material". */
		WMaterial* GetMaterial(const cgltf_material* Source)
		{
			if (Source == nullptr)
			{
				return nullptr;
			}

			const auto Existing = Materials.find(Source);
			if (Existing != Materials.end())
			{
				return Existing->second;
			}

			const std::string Description = DescribeMaterial(*Source);
			WMaterial* const Material = NewObject<WMaterial>();

			// A glTF material that says nothing is white, fully metallic and fully rough.
			Material->Metallic = 1.0f;
			Material->Roughness = 1.0f;
			if (Source->has_pbr_metallic_roughness != 0)
			{
				const cgltf_pbr_metallic_roughness& Pbr = Source->pbr_metallic_roughness;
				Material->BaseColor = FLinearColor(Pbr.base_color_factor[0],
					Pbr.base_color_factor[1],
					Pbr.base_color_factor[2],
					Pbr.base_color_factor[3]);
				Material->Metallic = Pbr.metallic_factor;
				Material->Roughness = Pbr.roughness_factor;
				Material->BaseColorTexture =
					GetTexture(Pbr.base_color_texture, ETextureColorSpace::Srgb, Description, "base colour");
				Material->MetallicRoughnessTexture = GetTexture(
					Pbr.metallic_roughness_texture, ETextureColorSpace::Linear, Description, "metallic-roughness");
			}

			const float EmissiveStrength =
				Source->has_emissive_strength != 0 ? Source->emissive_strength.emissive_strength : 1.0f;
			Material->EmissiveColor = FLinearColor(Source->emissive_factor[0] * EmissiveStrength,
				Source->emissive_factor[1] * EmissiveStrength,
				Source->emissive_factor[2] * EmissiveStrength);
			Material->EmissiveTexture =
				GetTexture(Source->emissive_texture, ETextureColorSpace::Srgb, Description, "emissive");

			Material->NormalTexture =
				GetTexture(Source->normal_texture, ETextureColorSpace::Linear, Description, "normal");
			if (Source->normal_texture.texture != nullptr)
			{
				Material->NormalScale = Source->normal_texture.scale;
			}

			if (Source->alpha_mode != cgltf_alpha_mode_opaque)
			{
				Warn(std::format(
					"{} is transparent or cut out, which is not supported; it is imported opaque", Description));
			}
			if (Source->double_sided != 0)
			{
				Warn(std::format("{} is double-sided, which is not supported; only its front is drawn", Description));
			}
			if (Source->unlit != 0)
			{
				Warn(std::format("{} is unlit, which is not supported; it is lit like any other", Description));
			}
			if (Source->has_pbr_specular_glossiness != 0 && Source->has_pbr_metallic_roughness == 0)
			{
				Warn(std::format("{} uses the specular-glossiness model, which is not supported", Description));
			}

			Materials.emplace(Source, Material);
			return Material;
		}

		/**
		 * Returns the engine texture for a texture a glTF material uses, created on first use. Null if the material
		 * has no such texture or it cannot be imported.
		 *
		 * @param ColorSpace What the material uses the texture for: colours or data.
		 * @param Owner The material, for warnings.
		 * @param Usage Which of the material's textures this is, for warnings.
		 */
		WTexture* GetTexture(const cgltf_texture_view& View,
			ETextureColorSpace ColorSpace,
			const std::string& Owner,
			std::string_view Usage)
		{
			if (View.texture == nullptr)
			{
				return nullptr;
			}
			if (View.texcoord != 0)
			{
				Warn(
					std::format("the {} texture of {} uses texture coordinate set {}, and only set 0 is supported; the "
								"texture is left out",
						Usage,
						Owner,
						View.texcoord));
				return nullptr;
			}
			if (View.has_transform != 0)
			{
				Warn(std::format("the {} texture of {} is moved or scaled by KHR_texture_transform, which is ignored",
					Usage,
					Owner));
			}

			const cgltf_image* const Source = View.texture->image;
			if (Source == nullptr)
			{
				Warn(std::format(
					"the {} texture of {} has no PNG or JPEG image; the texture is left out", Usage, Owner));
				return nullptr;
			}

			// One picture can be a colour texture in one material and a data texture in another.
			const FTextureKey Key(View.texture, ColorSpace);
			const auto Existing = Textures.find(Key);
			if (Existing != Textures.end())
			{
				return Existing->second;
			}

			WTexture* Texture = nullptr;
			FImage Image;
			std::string Problem;
			if (LoadImage(*Source, Image, Problem))
			{
				Texture = NewObject<WTexture>();
				if (!Texture->SetImage(std::move(Image), ColorSpace, Problem))
				{
					Texture = nullptr;
				}
			}

			if (Texture == nullptr)
			{
				Warn(std::format("the {} texture of {} is left out: {}", Usage, Owner, Problem));
			}
			else if (View.texture->sampler != nullptr)
			{
				Texture->WrapU = ToTextureWrap(View.texture->sampler->wrap_s);
				Texture->WrapV = ToTextureWrap(View.texture->sampler->wrap_t);
			}

			Textures.emplace(Key, Texture);
			return Texture;
		}

		/** Reads the picture of a glTF image from wherever the file keeps it. */
		bool LoadImage(const cgltf_image& Source, FImage& OutImage, std::string& OutProblem) const
		{
			if (Source.buffer_view != nullptr)
			{
				// The picture is a PNG or JPEG file inside one of the glTF file's buffers.
				const cgltf_buffer_view& View = *Source.buffer_view;
				const bool bIsInBuffer = View.buffer != nullptr && View.buffer->data != nullptr &&
					View.offset <= View.buffer->size && View.size <= View.buffer->size - View.offset;
				if (!bIsInBuffer)
				{
					OutProblem = "its data lies outside the file's buffers";
					return false;
				}

				const std::byte* const Bytes = static_cast<const std::byte*>(View.buffer->data) + View.offset;
				return FImageFile::LoadFromMemory(std::span<const std::byte>(Bytes, View.size), OutImage, OutProblem);
			}

			if (Source.uri == nullptr)
			{
				OutProblem = "the file does not say where its picture is";
				return false;
			}

			const std::string_view Uri(Source.uri);
			constexpr std::string_view DataPrefix = "data:";
			if (Uri.starts_with(DataPrefix))
			{
				// The picture is written into the glTF file itself, as base64 text after a comma.
				constexpr std::string_view Base64Marker = ";base64,";
				const std::size_t MarkerPosition = Uri.find(Base64Marker);
				const std::optional<std::vector<std::byte>> Bytes = MarkerPosition != std::string_view::npos
					? DecodeBase64(Uri.substr(MarkerPosition + Base64Marker.size()))
					: std::nullopt;
				if (!Bytes.has_value())
				{
					OutProblem = "its embedded picture is not valid base64 text";
					return false;
				}
				return FImageFile::LoadFromMemory(*Bytes, OutImage, OutProblem);
			}

			// The picture is a file beside the glTF file. Its address may have characters written as %20 and the like.
			std::string FileName(Uri);
			FileName.resize(cgltf_decode_uri(FileName.data()));
			return FImageFile::Load(Path.parent_path() / PathFromUtf8(FileName), OutImage, OutProblem);
		}

		using FTextureKey = std::pair<const cgltf_texture*, ETextureColorSpace>;

		const cgltf_data& Data;
		std::filesystem::path Path;
		std::vector<std::string>* Warnings = nullptr;

		// What has been created so far. Null marks something that could not be imported, so that it is neither
		// tried nor warned about twice.
		std::unordered_map<const cgltf_primitive*, WStaticMesh*> Meshes;
		std::unordered_map<const cgltf_material*, WMaterial*> Materials;
		std::map<FTextureKey, WTexture*> Textures;
	};
}

WEntity* FGltfImporter::Import(
	const std::filesystem::path& Path, WScene& Scene, std::string& OutError, std::vector<std::string>* OutWarnings)
{
	const std::string PathText = FFileHelper::PathToUtf8(Path);

	std::string FileContent;
	if (!FFileHelper::LoadFileToString(Path, FileContent, OutError))
	{
		return nullptr;
	}

	FGltfFileStore FileStore;
	cgltf_options Options{};
	Options.file.read = &ReadFileForGltf;
	Options.file.release = &ReleaseFileForGltf;
	Options.file.user_data = &FileStore;

	cgltf_data* ParsedData = nullptr;
	cgltf_result Result = cgltf_parse(&Options, FileContent.data(), FileContent.size(), &ParsedData);
	const std::unique_ptr<cgltf_data, FGltfDataDeleter> Data(ParsedData);
	if (Result != cgltf_result_success)
	{
		OutError = std::format("'{}' cannot be imported: {}", PathText, DescribeResult(Result));
		return nullptr;
	}

	for (cgltf_size Index = 0; Index < Data->extensions_required_count; ++Index)
	{
		const std::string_view Extension(Data->extensions_required[Index]);
		const bool bIsSupported =
			std::ranges::find(SupportedExtensions, Extension) != std::ranges::end(SupportedExtensions);
		if (!bIsSupported)
		{
			OutError = std::format(
				"'{}' cannot be imported: it requires the extension {}, which is not supported", PathText, Extension);
			return nullptr;
		}
	}

	Result = cgltf_load_buffers(&Options, Data.get(), PathText.c_str());
	if (Result != cgltf_result_success)
	{
		const std::string_view Reason = FileStore.LastError.empty() ? DescribeResult(Result) : FileStore.LastError;
		OutError = std::format("'{}' cannot be imported: {}", PathText, Reason);
		return nullptr;
	}

	Result = cgltf_validate(Data.get());
	if (Result != cgltf_result_success)
	{
		OutError = std::format("'{}' cannot be imported: {}", PathText, DescribeResult(Result));
		return nullptr;
	}

	FGltfImport ImportInProgress(*Data, Path, OutWarnings);
	return ImportInProgress.CreateEntities(Scene);
}
