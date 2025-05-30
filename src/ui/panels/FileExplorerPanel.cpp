#include "ui/panels/FileExplorerPanel.h"
#include "utils/Logger.h"  
#include "ui/Icons.h"      
#include <fstream>        
#include <algorithm>      

 
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>  
#include <limits.h>  
#endif

namespace LocalTether::UI::Panels {

    namespace fs = std::filesystem;

     
    std::filesystem::path get_executable_directory() {
        std::filesystem::path exe_path;
#ifdef _WIN32
        wchar_t path_buf[MAX_PATH] = {0};  
        GetModuleFileNameW(NULL, path_buf, MAX_PATH);
        exe_path = path_buf;  
#elif defined(__linux__)
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        if (count != -1) {
            exe_path = std::string(result, count);
        }
#endif
        if (!exe_path.empty()) {
            return exe_path.parent_path();
        }
         
        Utils::Logger::GetInstance().Error("Could not determine executable directory. Falling back to current_path().");
        return fs::current_path(); 
    }

     
    fs::path find_ancestor_directory(const fs::path& start_path, const std::string& target_dir_name, int max_depth) {
        fs::path current_path = start_path;
        for (int i = 0; i < max_depth && !current_path.empty() && current_path.has_parent_path(); ++i) {
            if (current_path.filename().string() == target_dir_name) {  
                return current_path;
            }
            current_path = current_path.parent_path();
        }
         
        if (!current_path.empty() && current_path.filename().string() == target_dir_name) {
             return current_path;
        }
        return "";  
    }


    FileExplorerPanel::FileExplorerPanel() {
        fs::path base_path;
        fs::path exe_dir = get_executable_directory();
        Utils::Logger::GetInstance().Debug("Executable directory: " + exe_dir.string());

        fs::path project_root_path = find_ancestor_directory(exe_dir, "LocalTether", 4);

        if (!project_root_path.empty()) {
            Utils::Logger::GetInstance().Info("Found project root 'LocalTether' at: " + project_root_path.string());
            base_path = project_root_path;
        } else {
            Utils::Logger::GetInstance().Warning("'LocalTether' project root not found within 4 parent levels of executable. Using executable directory as base.");
            base_path = exe_dir;  
        }
        
        rootStoragePath_ = (base_path / "server_storage").string();
        Utils::Logger::GetInstance().Info("Server storage path set to: " + rootStoragePath_);
        
        newFolderNameBuffer_[0] = '\0';
        newFileNameBuffer_[0] = '\0';
        itemToDeletePath_[0] = '\0';
        selectedPath_.clear();

         
        isMoveMode_ = false;
        itemToMovePath_.clear();
        moveDestinationPath_.clear();

        isRenameMode_ = false;
        itemToRenamePath_.clear();
        renameBuffer_[0] = '\0';
         

        InitializeStorage();
    }

    void FileExplorerPanel::InitializeStorage() {
        try {
            if (!fs::exists(rootStoragePath_)) {
                if (fs::create_directories(rootStoragePath_)) {
                    Utils::Logger::GetInstance().Info("Created server storage directory: " + rootStoragePath_);
                } else {
                    Utils::Logger::GetInstance().Error("Failed to create server storage directory: " + rootStoragePath_);
                     
                    return;
                }
            }
            RefreshView();
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Filesystem error during storage initialization: " + std::string(e.what()));
        }
    }

    void FileExplorerPanel::RefreshView() {
        rootNode_ = FileMetadata();  
        rootNode_.name = "Storage Root";  
        rootNode_.fullPath = rootStoragePath_;
        rootNode_.relativePath = "";
        rootNode_.isDirectory = true;
        
        selectedPath_.clear();  
        itemToDeletePath_[0] = '\0';

        try {
            ScanDirectoryRecursive(rootStoragePath_, rootNode_);
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Filesystem error during refresh: " + std::string(e.what()));
        }
    }

    void FileExplorerPanel::ScanDirectoryRecursive(const fs::path& dirPath, FileMetadata& parentNode) {
        parentNode.children.clear();
        try {
            if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
                Utils::Logger::GetInstance().Warning("ScanDirectoryRecursive: Path does not exist or is not a directory: " + dirPath.string());
                return;
            }

            for (const auto& entry : fs::directory_iterator(dirPath)) {
                FileMetadata meta;
                meta.name = entry.path().filename().string();
                meta.fullPath = entry.path().string();
                
                try {
                     
                     
                     
                    fs::path canonical_entry_path = fs::weakly_canonical(entry.path());
                    fs::path canonical_root_path = fs::weakly_canonical(rootStoragePath_);
                    if (canonical_entry_path.string().rfind(canonical_root_path.string(), 0) == 0) {
                         meta.relativePath = fs::relative(canonical_entry_path, canonical_root_path).string();
                    } else {
                         
                        Utils::Logger::GetInstance().Warning("Path " + canonical_entry_path.string() + " is not relative to root " + canonical_root_path.string());
                        meta.relativePath = meta.name;  
                    }

                } catch (const fs::filesystem_error& e) {
                    Utils::Logger::GetInstance().Error("Error getting relative path for " + entry.path().string() + " against " + rootStoragePath_ + ": " + e.what());
                    meta.relativePath = meta.name;  
                }

                meta.isDirectory = entry.is_directory();
                
                try {
                    meta.size = meta.isDirectory ? 0 : fs::file_size(entry.path());
                    auto ftime = fs::last_write_time(entry.path());
                     
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                    );
                    meta.modifiedTime = sctp;

                } catch (const fs::filesystem_error& e) {
                    Utils::Logger::GetInstance().Warning("Could not get metadata for " + meta.fullPath + ": " + e.what());
                    meta.size = 0;  
                    meta.modifiedTime = std::chrono::system_clock::now();  
                }

                if (meta.isDirectory) {
                    ScanDirectoryRecursive(entry.path(), meta);  
                }
                parentNode.children.push_back(meta);
            }
        } catch (const fs::filesystem_error& e) {
             Utils::Logger::GetInstance().Error("Filesystem error iterating directory " + dirPath.string() + ": " + std::string(e.what()));
             return;
        }
         
        std::sort(parentNode.children.begin(), parentNode.children.end(), [](const FileMetadata& a, const FileMetadata& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;  
            return a.name < b.name;
        });
    }

    void FileExplorerPanel::DrawFileSystemNode(FileMetadata& node, const std::string& current_node_path_prefix) {
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        
        if (isMoveMode_) {
            if (node.isDirectory && node.fullPath == moveDestinationPath_) {
                node_flags |= ImGuiTreeNodeFlags_Selected;  
            }
        } else if (isRenameMode_) {
            if (node.fullPath == itemToRenamePath_) {
                 node_flags |= ImGuiTreeNodeFlags_Selected;  
            }
        }
        else {  
            if (selectedPath_ == node.fullPath) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }
        }


        bool node_open;
        std::string icon = node.isDirectory ? ICON_FA_FOLDER : ICON_FA_FILE;
        std::string display_name = (node.fullPath == rootStoragePath_ && node.name == "Storage Root") ? node.name : node.name;
        
         
         

        std::string label = icon + " " + display_name;
        if (node.isDirectory) {
            if (node.children.empty() && node.fullPath != rootStoragePath_) {
                 node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                 node_open = ImGui::TreeNodeEx(node.fullPath.c_str(), node_flags, "%s", label.c_str());
            } else {
                node_open = ImGui::TreeNodeEx(node.fullPath.c_str(), node_flags, "%s", label.c_str());
            }
        } else {
            node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(node.fullPath.c_str(), node_flags, "%s", label.c_str());
            node_open = false;
        }

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {  
            if (isMoveMode_) {
                if (node.isDirectory) {
                    moveDestinationPath_ = node.fullPath;
                    Utils::Logger::GetInstance().Debug("Move destination selected: " + moveDestinationPath_);
                } else {
                     
                    Utils::Logger::GetInstance().Info("Cannot select a file as move destination. Select a folder.");
                }
            } else if (isRenameMode_) {
                 
            }
            else {  
                selectedPath_ = node.fullPath;
                Utils::Logger::GetInstance().Debug("Selected: " + selectedPath_);
                if (selectedPath_ != rootStoragePath_ || node.name != "Storage Root") {
                     strncpy(itemToDeletePath_, selectedPath_.c_str(), sizeof(itemToDeletePath_) - 1);
                     itemToDeletePath_[sizeof(itemToDeletePath_)-1] = '\0';
                } else {
                    itemToDeletePath_[0] = '\0'; 
                }
            }
        }
        
        if (node.isDirectory && node_open && !(node_flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
            for (auto& child : node.children) {
                DrawFileSystemNode(child, child.fullPath + (child.isDirectory ? "/" : ""));
            }
            ImGui::TreePop();
        }
    }

    void FileExplorerPanel::HandleExternalFileDragOver(const ImVec2& mouse_pos_in_window) {
        if (last_panel_size_.x == 0 && last_panel_size_.y == 0) {  
            is_external_drag_over_panel_ = false;
            return;
        }

        ImRect panel_rect(last_panel_pos_, ImVec2(last_panel_pos_.x + last_panel_size_.x, last_panel_pos_.y + last_panel_size_.y));
        if (panel_rect.Contains(mouse_pos_in_window)) {
            is_external_drag_over_panel_ = true;
            
            current_drop_target_dir_ = rootStoragePath_;  
            if (!selectedPath_.empty() && fs::exists(selectedPath_) && fs::is_directory(selectedPath_)) {
                current_drop_target_dir_ = selectedPath_;
            }
            external_drag_target_folder_display_name_ = current_drop_target_dir_.filename().string();
            if (current_drop_target_dir_ == rootStoragePath_) {
                external_drag_target_folder_display_name_ = "Storage Root";
            }

        } else {
            is_external_drag_over_panel_ = false;
            external_drag_target_folder_display_name_.clear();
        }
    }

    void FileExplorerPanel::HandleExternalFileDrop(const std::string& dropped_file_path_str) {
        if (!is_external_drag_over_panel_ || current_drop_target_dir_.empty()) {
            Utils::Logger::GetInstance().Warning("File dropped, but not over a valid target in File Explorer.");
            ClearExternalDragState();
            return;
        }

        fs::path source_file_path(dropped_file_path_str);
        fs::path target_dir = current_drop_target_dir_;
        fs::path destination_path = target_dir / source_file_path.filename();

        try {
            if (!fs::exists(source_file_path)) {
                Utils::Logger::GetInstance().Error("Dropped file does not exist: " + source_file_path.string());
                ClearExternalDragState();
                return;
            }
            if (!fs::exists(target_dir) || !fs::is_directory(target_dir)) {
                Utils::Logger::GetInstance().Error("Drop target directory is not valid: " + target_dir.string());
                ClearExternalDragState();
                return;
            }

            if (fs::exists(destination_path)) {
                Utils::Logger::GetInstance().Warning("File '" + source_file_path.filename().string() + "' already exists in '" + target_dir.filename().string() + "'. Overwriting.");
                 
            }
            
             
             
            if (fs::is_regular_file(source_file_path)) {
                fs::copy_file(source_file_path, destination_path, fs::copy_options::overwrite_existing);
                Utils::Logger::GetInstance().Info("Copied '" + source_file_path.string() + "' to '" + destination_path.string() + "'");
            } else if (fs::is_directory(source_file_path)) {
                 
                fs::copy(source_file_path, destination_path, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
                Utils::Logger::GetInstance().Info("Copied directory '" + source_file_path.string() + "' to '" + destination_path.string() + "'");
            } else {
                Utils::Logger::GetInstance().Warning("Dropped item is not a regular file or directory: " + source_file_path.string());
            }

            RefreshView();  
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error copying file: " + std::string(e.what()));
        }
        ClearExternalDragState();
    }

    void FileExplorerPanel::ClearExternalDragState() {
        is_external_drag_over_panel_ = false;
        external_drag_target_folder_display_name_.clear();
        current_drop_target_dir_.clear();
    }

    void FileExplorerPanel::Show(bool* p_open) {
        if (p_open && !*p_open) {
            ClearExternalDragState();  
            return;
        }
        ImGui::Begin("File Explorer (Server Storage)", p_open);

        last_panel_pos_ = ImGui::GetWindowPos();
    last_panel_size_ = ImGui::GetWindowSize();
         
         
        bool creationDisabled = isMoveMode_ || isRenameMode_;
        if (creationDisabled) ImGui::BeginDisabled();

        if (ImGui::Button(ICON_FA_SYNC_ALT " Refresh")) {
            if (!isMoveMode_ && !isRenameMode_) RefreshView();  
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(150);
        ImGui::InputTextWithHint("##NewFolderName", "New Folder Name", newFolderNameBuffer_, sizeof(newFolderNameBuffer_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER_PLUS " Create Folder")) { 
            HandleCreateFolder();
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(150);
        ImGui::InputTextWithHint("##NewFileName", "New File Name", newFileNameBuffer_, sizeof(newFileNameBuffer_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FILE_MEDICAL " Create File")) {
            HandleCreateFile();
        }
        if (creationDisabled) ImGui::EndDisabled();
        
        ImGui::Separator();

         
        if (ImGui::BeginChild("FileSystemTree", ImVec2(0, ImGui::GetContentRegionAvail().y - 85), true)) {
            if (!rootNode_.fullPath.empty()) {
                 DrawFileSystemNode(rootNode_, "");
            } else {
                ImGui::Text("Storage not initialized or empty.");
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        bool item_selected_for_action = !selectedPath_.empty() && (selectedPath_ != rootStoragePath_ || rootNode_.name != "Storage Root");

        if (isMoveMode_) {
            ImGui::Text("Moving: %s", fs::path(itemToMovePath_).filename().string().c_str());
            if (!moveDestinationPath_.empty()) {
                 ImGui::Text("To: %s", fs::path(moveDestinationPath_).filename().string().c_str());
            } else {
                ImGui::Text("Select a destination folder from the tree.");
            }
            if (ImGui::Button(ICON_FA_CHECK " Confirm Move")) {  
                HandleConfirmMove();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TIMES " Cancel Move")) {  
                HandleCancelMove();
            }
        } else if (isRenameMode_) {
            ImGui::Text("Renaming: %s", fs::path(itemToRenamePath_).filename().string().c_str());
            ImGui::PushItemWidth(200);
            if (ImGui::InputText("New Name", renameBuffer_, sizeof(renameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
                HandleConfirmRename();  
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_SAVE " Save")) {  
                HandleConfirmRename();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TIMES " Cancel")) {  
                HandleCancelRename();
            }
        } else {  
            if (item_selected_for_action) {
                ImGui::Text("Selected: %s", fs::path(selectedPath_).filename().string().c_str());
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_TRASH " Delete")) {
                    if (itemToDeletePath_[0] != '\0') {
                        ImGui::OpenPopup("Confirm Deletion");
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROWS_ALT " Move")) {  
                    HandleInitiateMove();
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_EDIT " Rename")) {  
                    HandleInitiateRename();
                }
            } else if (!selectedPath_.empty() && selectedPath_ == rootStoragePath_ && rootNode_.name == "Storage Root") {
                 ImGui::Text("Selected: Storage Root (Actions disabled)");
            }
            else {
                ImGui::Text("No item selected.");
            }
        }

         
        if (ImGui::BeginPopupModal("Confirm Deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete '%s'?", fs::path(itemToDeletePath_).filename().string().c_str());
            ImGui::TextWrapped("This action cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                HandleDeleteSelected();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (is_external_drag_over_panel_ && !external_drag_target_folder_display_name_.empty()) {
            ImGui::SetNextWindowPos(ImVec2(last_panel_pos_.x + 10, last_panel_pos_.y + last_panel_size_.y - 40));  
            ImGui::SetNextWindowBgAlpha(0.75f);
            ImGui::Begin("DragDropNotification", nullptr, 
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
            ImGui::Text("Drop into: %s", external_drag_target_folder_display_name_.c_str());
            ImGui::End();
        }
        ImGui::End();
    }

    void FileExplorerPanel::HandleCreateFolder() {
        if (strlen(newFolderNameBuffer_) == 0) {
            Utils::Logger::GetInstance().Warning("Folder name cannot be empty.");
            return;
        }
         
         
         
        fs::path parentDirToCreateIn = rootStoragePath_;
        if (!selectedPath_.empty() && fs::exists(selectedPath_) && fs::is_directory(selectedPath_)) {
            parentDirToCreateIn = selectedPath_;
        }


        fs::path newFolderPath = parentDirToCreateIn / newFolderNameBuffer_;
        try {
            if (fs::create_directory(newFolderPath)) {
                Utils::Logger::GetInstance().Info("Created folder: " + newFolderPath.string());
                newFolderNameBuffer_[0] = '\0';  
                RefreshView();
            } else {
                Utils::Logger::GetInstance().Error("Failed to create folder: " + newFolderPath.string() + " (maybe it already exists or bad path).");
            }
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error creating folder " + newFolderPath.string() + ": " + std::string(e.what()));
        }
    }

    void FileExplorerPanel::HandleCreateFile() {
        if (strlen(newFileNameBuffer_) == 0) {
            Utils::Logger::GetInstance().Warning("File name cannot be empty.");
            return;
        }
        fs::path parentDirToCreateIn = rootStoragePath_;
        if (!selectedPath_.empty() && fs::exists(selectedPath_) && fs::is_directory(selectedPath_)) {
            parentDirToCreateIn = selectedPath_;
        }

        fs::path newFilePath = parentDirToCreateIn / newFileNameBuffer_;
        try {
            if (fs::exists(newFilePath)) {
                 Utils::Logger::GetInstance().Warning("File already exists: " + newFilePath.string());
                 return;
            }
            std::ofstream outfile(newFilePath);  
            if (outfile.good()) {
                Utils::Logger::GetInstance().Info("Created file: " + newFilePath.string());
                newFileNameBuffer_[0] = '\0';  
                RefreshView();
            } else {
                 Utils::Logger::GetInstance().Error("Failed to create file (ofstream error): " + newFilePath.string());
            }
            outfile.close();  
        } catch (const std::exception& e) {  
            Utils::Logger::GetInstance().Error("Error creating file " + newFilePath.string() + ": " + std::string(e.what()));
        }
    }

    void FileExplorerPanel::HandleDeleteSelected() {
        if (itemToDeletePath_[0] == '\0' || !fs::exists(itemToDeletePath_)) {
            Utils::Logger::GetInstance().Warning("No valid item selected for deletion or item no longer exists.");
            itemToDeletePath_[0] = '\0';  
            selectedPath_.clear();  
            RefreshView();  
            return;
        }
        
         
         
        try {
            if (fs::equivalent(fs::path(itemToDeletePath_), fs::path(rootStoragePath_))) {
                Utils::Logger::GetInstance().Warning("Cannot delete the root storage directory via UI.");
                itemToDeletePath_[0] = '\0';
                selectedPath_.clear();
                return;
            }
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error comparing paths for deletion safety: " + std::string(e.what()));
             
            return; 
        }


        try {
            uintmax_t removed_count = fs::remove_all(itemToDeletePath_);  
            Utils::Logger::GetInstance().Info("Deleted: " + std::string(itemToDeletePath_) + ". Items removed: " + std::to_string(removed_count));
            itemToDeletePath_[0] = '\0';  
            selectedPath_.clear();  
            RefreshView();
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error deleting " + std::string(itemToDeletePath_) + ": " + std::string(e.what()));
        }
    }
    void FileExplorerPanel::HandleInitiateMove() {
        if (selectedPath_.empty() || (selectedPath_ == rootStoragePath_ && rootNode_.name == "Storage Root")) {
            Utils::Logger::GetInstance().Warning("No valid item selected to move.");
            return;
        }
        isMoveMode_ = true;
        isRenameMode_ = false;  
        itemToMovePath_ = selectedPath_;
        moveDestinationPath_.clear();  
        Utils::Logger::GetInstance().Info("Initiating move for: " + itemToMovePath_);
    }

    void FileExplorerPanel::HandleConfirmMove() {
        if (itemToMovePath_.empty() || moveDestinationPath_.empty()) {
            Utils::Logger::GetInstance().Warning("Move operation aborted: Source or destination not set.");
            HandleCancelMove();
            return;
        }

        fs::path sourcePath(itemToMovePath_);
        fs::path destinationDir(moveDestinationPath_);

        if (!fs::exists(sourcePath)) {
            Utils::Logger::GetInstance().Error("Move failed: Source item no longer exists: " + itemToMovePath_);
            HandleCancelMove();
            RefreshView();  
            return;
        }
        if (!fs::exists(destinationDir) || !fs::is_directory(destinationDir)) {
            Utils::Logger::GetInstance().Error("Move failed: Destination is not a valid directory: " + moveDestinationPath_);
            HandleCancelMove();
            return;
        }

         
        if (fs::is_directory(sourcePath)) {
            fs::path current_check = destinationDir;
            while (current_check.has_parent_path() && current_check != current_check.parent_path() /* check for root */) {
                if (fs::equivalent(current_check, sourcePath)) {
                    Utils::Logger::GetInstance().Error("Move failed: Cannot move a folder into itself or one of its subfolders.");
                    HandleCancelMove();
                    return;
                }
                current_check = current_check.parent_path();
            }
            if (fs::equivalent(current_check, sourcePath)) {  
                Utils::Logger::GetInstance().Error("Move failed: Cannot move a folder into itself (root check).");
                HandleCancelMove();
                return;
            }
        }
        
         
         
        if (fs::equivalent(sourcePath.parent_path(), destinationDir)) {
            Utils::Logger::GetInstance().Info("Item is already in the target directory. No move performed.");
            HandleCancelMove();
            return;
        }


        fs::path newPath = destinationDir / sourcePath.filename();

        if (fs::exists(newPath)) {
            Utils::Logger::GetInstance().Error("Move failed: An item with the same name already exists at the destination: " + newPath.string());
             
            HandleCancelMove();
            return;
        }

        try {
            fs::rename(sourcePath, newPath);
            Utils::Logger::GetInstance().Info("Moved '" + sourcePath.string() + "' to '" + newPath.string() + "'");
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error moving item: " + std::string(e.what()));
        }
        
        HandleCancelMove();  
        RefreshView();
    }

    void FileExplorerPanel::HandleCancelMove() {
        isMoveMode_ = false;
        itemToMovePath_.clear();
        moveDestinationPath_.clear();
        Utils::Logger::GetInstance().Info("Move cancelled.");
    }

    void FileExplorerPanel::HandleInitiateRename() {
        if (selectedPath_.empty() || (selectedPath_ == rootStoragePath_ && rootNode_.name == "Storage Root")) {
            Utils::Logger::GetInstance().Warning("No valid item selected to rename.");
            return;
        }
        isRenameMode_ = true;
        isMoveMode_ = false;  
        itemToRenamePath_ = selectedPath_;
        strncpy(renameBuffer_, fs::path(itemToRenamePath_).filename().string().c_str(), sizeof(renameBuffer_) - 1);
        renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';  
        Utils::Logger::GetInstance().Info("Initiating rename for: " + itemToRenamePath_);
        ImGui::SetKeyboardFocusHere(-1);  
    }

    void FileExplorerPanel::HandleConfirmRename() {
        Utils::Logger::GetInstance().Debug("HandleConfirmRename called with itemToRenamePath_: " + itemToRenamePath_ + " and renameBuffer_: " + renameBuffer_);
        if (!isRenameMode_) {
            Utils::Logger::GetInstance().Warning("Rename operation not initiated. Call HandleInitiateRename first.");
            return;
        }
        if (itemToRenamePath_.empty() || strlen(renameBuffer_) == 0) {
            Utils::Logger::GetInstance().Warning("Rename operation aborted: Item or new name is invalid.");
            HandleCancelRename();
            return;
        }

        fs::path sourcePath(itemToRenamePath_);
        if (!fs::exists(sourcePath)) {
            Utils::Logger::GetInstance().Error("Rename failed: Source item no longer exists: " + itemToRenamePath_);
            HandleCancelRename();
            RefreshView();
            return;
        }
        
         
        std::string newNameStr(renameBuffer_);
        if (newNameStr.find('/') != std::string::npos || newNameStr.find('\\') != std::string::npos) {
            Utils::Logger::GetInstance().Error("Rename failed: New name contains invalid characters ('/' or '\\').");
             
            return;
        }


        fs::path newPath = sourcePath.parent_path() / renameBuffer_;
        Utils::Logger::GetInstance().Debug("Renaming from " + sourcePath.string() + " to " + newPath.string());
        if (fs::exists(newPath) && !fs::equivalent(sourcePath, newPath)) {
            Utils::Logger::GetInstance().Error("Rename failed: An item with the name '" + std::string(renameBuffer_) + "' already exists.");
             
            return;
        }
        

        try {
            fs::rename(sourcePath, newPath);
            Utils::Logger::GetInstance().Info("Renamed '" + sourcePath.string() + "' to '" + newPath.string() + "'");
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error renaming item: " + std::string(e.what()));
        }

        HandleCancelRename();  
        RefreshView();
    }

    void FileExplorerPanel::HandleCancelRename() {
        isRenameMode_ = false;
        itemToRenamePath_.clear();
        renameBuffer_[0] = '\0';
        Utils::Logger::GetInstance().Info("Rename cancelled.");
    }

}  