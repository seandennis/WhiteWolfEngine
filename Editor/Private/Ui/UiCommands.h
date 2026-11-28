#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandResult.h"

#include <optional>
#include <string>
#include <vector>

class FEditor;
class FJsonValue;
struct FTransform;
struct FUiAsset;
struct FUiNewEntity;

/**
 * The commands that the widgets of the user interface give the editor.
 *
 * A widget does nothing to the scene itself: it gives the editor a command, the same one that an agent or a test
 * would give (D-040). What the user does as one thing is one step of the history, with a name that says what it
 * was (D-054); if the user was in the middle of something else, a change begun with BeginChange, that ends first.
 * A command that fails is reported by the editor in its log, which the user interface shows, so most of what is
 * here does not return whether it worked. What reads or writes files does: the user has to be told at once.
 */
class FUiCommands
{
public:
	/**
	 * Project.New: makes a project and has the editor work on it.
	 *
	 * @param Directory The project's own directory, which is made if it is not there.
	 * @param Name The project's name, which also names its file.
	 */
	static FCommandResult NewProject(FEditor& Editor, const std::string& Directory, const std::string& Name);

	/** Project.Open: has the editor work on the project in a project file, or in a directory that holds one. */
	static FCommandResult OpenProject(FEditor& Editor, const std::string& Path);

	/** Scene.New: replaces the scene being edited with an empty one. */
	static void NewScene(FEditor& Editor);

	/**
	 * Scene.Save: writes the scene to its own file. The result says why if the file was not written, which the
	 * user has to be told and not only the log.
	 */
	static FCommandResult SaveScene(FEditor& Editor);

	/** Scene.Save to a file, which becomes the scene's. A relative path is relative to the project's content. */
	static FCommandResult SaveSceneAs(FEditor& Editor, const std::string& Path);

	/**
	 * Asset.CreateMaterial: makes a material file in the project's content, with the values of a new material. The
	 * result says why if it was not made, which the user has to be told and not only the log.
	 */
	static FCommandResult CreateMaterial(FEditor& Editor, const std::string& Path);

	/**
	 * Asset.Save: writes a material whose changes are not saved to its file. The result says why if it was not
	 * written, which the user has to be told and not only the log.
	 */
	static FCommandResult SaveAsset(FEditor& Editor, const std::string& AssetPath);

	/** Scene.Open: replaces the scene being edited with the one in a file. */
	static FCommandResult OpenScene(FEditor& Editor, const std::string& Path);

	/** Scene.List: returns the scene files of the project, or none if no project is open. */
	static std::vector<std::string> ListScenes(FEditor& Editor);

	/**
	 * Asset.List: returns the asset files of the project, and the files of assets that the engine has built in,
	 * in the order of their names.
	 */
	static std::vector<std::string> ListAssetFiles(FEditor& Editor);

	/**
	 * Asset.Describe: returns the assets of a file, which is loaded for it if it is not loaded.
	 *
	 * @param OutError Receives the reason if the file cannot be loaded.
	 * @return The assets, or nothing if the file cannot be loaded.
	 */
	static std::optional<std::vector<FUiAsset>> DescribeAssetFile(
		FEditor& Editor, const std::string& File, std::string& OutError);

	/** Edit.Undo: undoes the last step of the history, if there is one. */
	static void Undo(FEditor& Editor);

	/** Edit.Redo: redoes the step that was undone last, if there is one. */
	static void Redo(FEditor& Editor);

	/**
	 * Returns those of the entities that exist and are not below another of them, in the order they were given:
	 * the ones to act on when what is done to an entity is done to everything below it, as moving it is.
	 */
	static std::vector<int64> GetTopmostEntities(const FEditor& Editor, const std::vector<int64>& EntityIds);

	/** Selection.Set: selects the entities with the given identifiers and no others. */
	static void SetSelection(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Selection.Set: adds an entity to what is selected, as the last of it, or takes it out if it is selected
	 * already. What a click on an entity does while Ctrl is held.
	 */
	static void ToggleSelection(FEditor& Editor, int64 EntityId);

	/**
	 * Entity.Create: creates an entity and selects it.
	 *
	 * @param ParentId The entity it is attached to; at the root of the scene if not given.
	 * @return The identifier of the new entity, or nothing if the editor refused.
	 */
	static std::optional<int64> CreateEntity(FEditor& Editor, const std::string& Name, std::optional<int64> ParentId);

	/** Entity.Rename. */
	static void RenameEntity(FEditor& Editor, int64 EntityId, const std::string& Name);

	/**
	 * Entity.Delete for each of the entities, as one step. An entity that goes with one of the others, because it
	 * is below it, needs no command of its own.
	 */
	static void DeleteEntities(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Entity.Duplicate for each of the entities that is not below one of the others, as one step, and selects the
	 * copies.
	 */
	static void DuplicateEntities(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Entity.Duplicate for each of the entities, and selects the copies. It is no step by itself: during a change
	 * (BeginChange) it is part of that change, as the copies are that a drag of the gizmo makes before it moves
	 * them.
	 *
	 * @return The identifier of the copy of each of the entities, in their order, or nothing where the editor
	 * refused to copy one.
	 */
	static std::vector<std::optional<int64>> CopyEntities(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Entity.SetParent for each of the entities that is not below one of the others, as one step: they keep their
	 * places in the world.
	 *
	 * @param ParentId The new parent; the root of the scene if not given.
	 */
	static void AttachEntities(FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId);

	/**
	 * Returns true if AttachEntities can give the entities this parent: it is none of them and below none of them.
	 */
	static bool CanAttachEntities(
		const FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId);

	/**
	 * Entity.Create and what follows it: makes an entity where the viewport's camera looks, with the component and
	 * the mesh that it is to begin with, and selects it. One step, named "Add" and what the entity is called.
	 *
	 * @param NewEntity What to make. It is written in the code, not by the user, so a class of component that
	 * cannot be added or a mesh that does not exist is a mistake there, and a fatal error.
	 */
	static void AddEntity(FEditor& Editor, const FUiNewEntity& NewEntity);

	/** Viewport.Focus: brings the viewport's camera to where it sees the selected entities whole, if any are. */
	static void FocusSelection(FEditor& Editor);

	/** Component.Add: gives an entity a component of the named class. The step is named after the class. */
	static void AddComponent(FEditor& Editor, int64 EntityId, const std::string& ClassName);

	/** Component.Remove. The step is named after the component's class. */
	static void RemoveComponent(FEditor& Editor, int64 ComponentId);

	/**
	 * Begins a change that the user makes over several frames, such as dragging a number through many values: the
	 * commands given from now until EndChange are one step of the history, with this name.
	 */
	static void BeginChange(FEditor& Editor, std::string StepName);

	/** Ends the change that BeginChange began. Does nothing if none is under way. */
	static void EndChange(FEditor& Editor);

	/**
	 * Object.SetProperty: gives a property of an entity or component a value. During a change (BeginChange) it is
	 * part of that change, and a step by itself otherwise.
	 *
	 * @param Value The value, in the form the command takes it.
	 */
	static void SetProperty(FEditor& Editor, int64 ObjectId, const std::string& PropertyName, const FJsonValue& Value);

	/**
	 * Object.SetProperty: puts an entity where a transform says, relative to its parent. During a change
	 * (BeginChange) it is part of that change, and a step by itself otherwise.
	 */
	static void SetEntityTransform(FEditor& Editor, int64 EntityId, const FTransform& Transform);
};
