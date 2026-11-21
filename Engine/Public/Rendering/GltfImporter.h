#pragma once

#include "CoreMinimal.h"

#include <filesystem>
#include <string>
#include <vector>

class WEntity;
class WScene;

/**
 * Brings glTF 2.0 files, the standard exchange format for 3D scenes, into the engine. Both forms are read: .gltf,
 * a text file with its data beside it or inside it, and .glb, everything in one binary file.
 *
 * What is imported: the nodes of the file's scene as entities with their names and transforms, triangle meshes
 * with normals and the first set of texture coordinates, metallic-roughness materials with their base colour,
 * metallic-roughness, normal and emissive textures (PNG and JPEG), and directional and point lights
 * (KHR_lights_punctual, with KHR_materials_emissive_strength for materials).
 *
 * The engine uses glTF's conventions for axes, units, materials and lights, so everything arrives unconverted.
 *
 * What is not imported, each with a warning that says so: cameras, animations, skins and morph targets, spot
 * lights, points and lines, transparency, double-sided and unlit materials, further sets of texture coordinates,
 * vertex colours, texture transforms and occlusion textures. Tangents in the file are not used; the renderer works
 * them out. Normals that a mesh lacks are calculated, flat for each triangle, as glTF asks.
 */
class FGltfImporter
{
public:
	/**
	 * Imports a file into a scene.
	 *
	 * One entity named after the file is created, and the file's nodes become its descendants. Meshes, materials
	 * and textures are created as objects that the new entities' components refer to; what is used several times
	 * in the file is created once and shared.
	 *
	 * @param Path The .gltf or .glb file. Files it refers to are looked for relative to its directory.
	 * @param Scene The scene the entities are created in.
	 * @param OutError Receives the reason on failure.
	 * @param OutWarnings If given, receives one line for everything in the file that was left out or could not be
	 * read. Warnings are also logged to LogRendering.
	 * @return The entity that stands for the file. Null if the file cannot be read, is not valid glTF 2.0, lacks a
	 * buffer it refers to or requires an extension the engine does not support; the scene is then unchanged.
	 */
	ENGINE_API static WEntity* Import(const std::filesystem::path& Path,
		WScene& Scene,
		std::string& OutError,
		std::vector<std::string>* OutWarnings = nullptr);
};
