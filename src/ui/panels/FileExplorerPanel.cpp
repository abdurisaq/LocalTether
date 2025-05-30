#include "ui/panels/FileExplorerPanel.h"
#include "utils/Logger.h"  
#include "ui/Icons.h"       
#include <fstream>        
#include <algorithm>      
#include "network/Client.h"  
#include "network/Server.h"
#include "ui/UIState.h"      
#include "network/Message.h" 

#include <iomanip>
#include <sstream>
#include <string>  

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>  
#elif defined(__linux__)
#include <unistd.h>  
#include <limits.h>  
#include <cstdlib>     
#endif


namespace fs = std::filesystem;


namespace LocalTether::UI::Panels {


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
            exe_path = std::string(result, (size_t)count);
        }
#elif defined(__APPLE__)
        char path_buf[PATH_MAX];
        uint32_t bufsize = PATH_MAX;
        if (_NSGetExecutablePath(path_buf, &bufsize) == 0) {
            exe_path = path_buf;
        }
#endif
        if (!exe_path.empty()) {
            return exe_path.parent_path();
        }
         
        Utils::Logger::GetInstance().Error("Could not determine executable directory. Falling back to current_path().");
        return fs::current_path(); 
    }

    fs::path find_ancestor_directory(const fs::path& start_path, const std::string& target_dir_name, int max_depth) {
        fs::path current_path = fs::absolute(start_path); 
        for (int i = 0; i < max_depth && !current_path.empty() && current_path.has_parent_path(); ++i) {
            if (current_path.filename().string() == target_dir_name) {  
                return current_path;
            }
            if (current_path.parent_path() == current_path) break; 
            current_path = current_path.parent_path();
        }
        if (!current_path.empty() && current_path.filename().string() == target_dir_name) {
             return current_path;
        }
        return "";  
    }

    FileExplorerPanel::FileExplorerPanel() {
        fs::path base_path;
        fs::path exe_dir = LocalTether::UI::Panels::get_executable_directory();
        Utils::Logger::GetInstance().Debug("Executable directory: " + exe_dir.string());

        fs::path project_root_path = LocalTether::UI::Panels::find_ancestor_directory(exe_dir, "LocalTether", 4);

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

    void FileExplorerPanel::BroadcastFileSystemUpdate() {
         
        if (!LocalTether::UI::isNetworkInitialized() || LocalTether::UI::getClient().getRole() != LocalTether::Network::ClientRole::Host) {
            if(LocalTether::UI::isNetworkInitialized()) Utils::Logger::GetInstance().Warning("BroadcastFileSystemUpdate called on non-host client. Skipping broadcast.");
             
            return;
        }
        auto* server = LocalTether::UI::getServerPtr();  
        if (server && server->getState() == Network::ServerState::Running) {
            uint32_t senderId = server->getHostClientId(); 
            
            Utils::Logger::GetInstance().Info("Broadcasting FileSystemUpdate.");
            Network::Message updateMsg = Network::Message::createFileSystemUpdate(this->rootNode_, senderId);
            server->broadcast(updateMsg);
        } else {
            Utils::Logger::GetInstance().Warning("Cannot broadcast file system update: Server not available or not running.");
        }
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
            if (LocalTether::UI::isNetworkInitialized() && LocalTether::UI::getClient().getRole() == LocalTether::Network::ClientRole::Host) {
                 BroadcastFileSystemUpdate();
            }
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
                         meta.relativePath = fs::relative(canonical_entry_path, canonical_root_path).generic_string();
                    } else {
                        Utils::Logger::GetInstance().Warning("Path " + canonical_entry_path.string() + " is not relative to root " + canonical_root_path.string() + ". Using filename as relative path.");
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
    
    void FileExplorerPanel::SetRootNode(const FileMetadata& newRootNode) {
        this->rootNode_ = newRootNode;
        this->selectedPath_.clear();
        this->itemToDeletePath_[0] = '\0';
        this->isMoveMode_ = false;
        this->isRenameMode_ = false;
        Utils::Logger::GetInstance().Info("FileExplorerPanel updated with new file system metadata from server.");
    }

    const FileMetadata& FileExplorerPanel::getRootNode() const {
        return rootNode_;
    }

    void FileExplorerPanel::HandleExternalFileDragOver(const ImVec2& mouse_pos_in_window) {
        if (last_panel_size_.x == 0 && last_panel_size_.y == 0) {  
            is_external_drag_over_panel_ = false;
            return;
        }

        ImRect panel_rect(last_panel_pos_, ImVec2(last_panel_pos_.x + last_panel_size_.x, last_panel_pos_.y + last_panel_size_.y));
        if (panel_rect.Contains(mouse_pos_in_window)) {
            is_external_drag_over_panel_ = true;
            
            current_drop_target_dir_ = fs::path(rootStoragePath_);  
            if (!selectedPath_.empty() && fs::exists(selectedPath_) && fs::is_directory(selectedPath_)) {
                current_drop_target_dir_ = selectedPath_;
            }
            external_drag_target_folder_display_name_ = current_drop_target_dir_.filename().string();
            if (current_drop_target_dir_.string() == rootStoragePath_) {  
                external_drag_target_folder_display_name_ = "Storage Root";
            }
        } else {
            is_external_drag_over_panel_ = false;
            external_drag_target_folder_display_name_.clear();
        }
    }

    void FileExplorerPanel::ClearExternalDragState() {
        is_external_drag_over_panel_ = false;
        external_drag_target_folder_display_name_.clear();
        current_drop_target_dir_.clear();
    }

    const FileMetadata* findNodeByPathRecursiveUtil(const FileMetadata& currentNode, const std::string& targetPath) {
        if (currentNode.fullPath == targetPath) {
            return &currentNode;
        }
        if (currentNode.isDirectory) {
            for (const auto& child : currentNode.children) {
                if (targetPath.rfind(child.fullPath, 0) == 0) { 
                    const FileMetadata* found = findNodeByPathRecursiveUtil(child, targetPath);
                    if (found) {
                        return found;
                    }
                }
            }
        }
        return nullptr;
    }

    void FileExplorerPanel::DrawFileSystemNode(FileMetadata& node, const std::string& current_node_path_prefix) {
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        bool network_ok = LocalTether::UI::isNetworkInitialized();  
        bool isHost = network_ok && (LocalTether::UI::getClient().getRole() == LocalTether::Network::ClientRole::Host);

        fs::path clientCacheRoot;
        if (network_ok && !isHost) {
            fs::path exe_dir = get_executable_directory();
            fs::path project_root_path = find_ancestor_directory(exe_dir, "LocalTether", 4);
            clientCacheRoot = (project_root_path.empty() ? exe_dir : project_root_path) / "client_file_cache";
        }
        
        if (isHost) { 
            if (isMoveMode_) {
                if (node.isDirectory && node.fullPath == moveDestinationPath_) {  
                    node_flags |= ImGuiTreeNodeFlags_Selected;  
                }
            } else if (isRenameMode_) {
                if (node.fullPath == itemToRenamePath_) {  
                     node_flags |= ImGuiTreeNodeFlags_Selected;  
                }
            } else {  
                if (selectedPath_ == node.fullPath) {
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                }
            }
        } else { 
            if (selectedPath_ == node.fullPath) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }
        }

        bool node_open;
         
        std::string icon = node.isDirectory ? ICON_FA_FOLDER : ICON_FA_FILE_ALT; 
        std::string display_name = (node.fullPath == rootStoragePath_ && node.name == "Storage Root") ? node.name : node.name;
        
        std::string label = icon + " " + display_name;

        if (network_ok && !isHost && !node.isDirectory) {
            if (!clientCacheRoot.empty()) { 
                fs::path local_file_path = clientCacheRoot / node.relativePath;
                if (!fs::exists(local_file_path) || !fs::is_regular_file(local_file_path)) { 
                    label += " (Not Local)";
                } else {
                    label += " (Local)";
                }
            } else {
                 label += " (Cache Error)";
            }
        }

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
            if (isHost) { 
                if (isMoveMode_) {
                    if (node.isDirectory) {
                        moveDestinationPath_ = node.fullPath;  
                        Utils::Logger::GetInstance().Debug("Move destination selected: " + moveDestinationPath_);
                    } else {
                        Utils::Logger::GetInstance().Info("Cannot select a file as move destination. Select a folder.");
                    }
                } else if (isRenameMode_) {
                     
                } else {  
                    selectedPath_ = node.fullPath;  
                    Utils::Logger::GetInstance().Debug("Host selected: " + selectedPath_);
                    if (selectedPath_ != rootStoragePath_ || node.name != "Storage Root") {
                         strncpy(itemToDeletePath_, selectedPath_.c_str(), sizeof(itemToDeletePath_) - 1);
                         itemToDeletePath_[sizeof(itemToDeletePath_)-1] = '\0';
                    } else {
                        itemToDeletePath_[0] = '\0'; 
                    }
                }
            } else { 
                selectedPath_ = node.fullPath;
                Utils::Logger::GetInstance().Debug("Client selected: " + selectedPath_);
            }
        }
        
        if (node.isDirectory && node_open && !(node_flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
            for (auto& child : node.children) {
                 
                DrawFileSystemNode(child, child.fullPath + (child.isDirectory ? "/" : ""));
            }
            ImGui::TreePop();
        }
    }
    
    void FileExplorerPanel::HandleExternalFileDrop(const std::string& dropped_file_path_str) {
        if (!is_external_drag_over_panel_ || current_drop_target_dir_.empty()) {
            Utils::Logger::GetInstance().Warning("File dropped, but not over a valid target in File Explorer.");
            ClearExternalDragState();
            return;
        }

        bool network_ok = LocalTether::UI::isNetworkInitialized();
        bool isHost = network_ok && (LocalTether::UI::getClient().getRole() == LocalTether::Network::ClientRole::Host);
        fs::path source_file_path(dropped_file_path_str);

        if (isHost) {
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
                    Utils::Logger::GetInstance().Info("Host Copied '" + source_file_path.string() + "' to '" + destination_path.string() + "'");
                } else if (fs::is_directory(source_file_path)) {
                    fs::copy(source_file_path, destination_path, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
                    Utils::Logger::GetInstance().Info("Host Copied directory '" + source_file_path.string() + "' to '" + destination_path.string() + "'");
                } else {
                    Utils::Logger::GetInstance().Warning("Dropped item is not a regular file or directory: " + source_file_path.string());
                }

                RefreshView();  
                BroadcastFileSystemUpdate();
            } catch (const fs::filesystem_error& e) {
                 
                Utils::Logger::GetInstance().Error("Error copying file for host: " + std::string(e.what()));
            }
        } else if (network_ok) {  
            if (!fs::exists(source_file_path) || !fs::is_regular_file(source_file_path)) {
                Utils::Logger::GetInstance().Warning("Client can only upload regular files. Dropped: " + dropped_file_path_str);
                ClearExternalDragState();
                return;
            }

            std::string targetServerRelativePath;
            fs::path canonical_drop_target = fs::weakly_canonical(current_drop_target_dir_);
            fs::path canonical_root_storage = fs::weakly_canonical(fs::path(rootStoragePath_));  

            if (canonical_drop_target.string().rfind(canonical_root_storage.string(), 0) == 0) {
                targetServerRelativePath = fs::relative(canonical_drop_target, canonical_root_storage).string();
            } else {
                Utils::Logger::GetInstance().Error("Client D&D: Invalid drop target directory calculation. Drop target: " + canonical_drop_target.string() + ", Root: " + canonical_root_storage.string());
                ClearExternalDragState();
                return;
            }
            
            Utils::Logger::GetInstance().Info("Client initiating upload of '" + source_file_path.filename().string() + 
                                              "' to server relative path: /" + targetServerRelativePath);
            
            LocalTether::UI::getClient().uploadFile(source_file_path.string(), targetServerRelativePath, source_file_path.filename().string());
            Utils::Logger::GetInstance().Info("Client file upload initiated for: " + source_file_path.filename().string() + " to " + targetServerRelativePath);
        } else {
             Utils::Logger::GetInstance().Warning("Client file upload skipped: Network not initialized.");
        }
        ClearExternalDragState();
    }

     
    void FileExplorerPanel::Show(bool* p_open) {
        if (p_open && !*p_open) {
            ClearExternalDragState();  
            return;
        }
        ImGui::Begin("File Explorer", p_open); 

        last_panel_pos_ = ImGui::GetWindowPos();
        last_panel_size_ = ImGui::GetWindowSize();
         
        bool network_init = LocalTether::UI::isNetworkInitialized();  
        bool isHost = network_init && (LocalTether::UI::getClient().getRole() == LocalTether::Network::ClientRole::Host);
        
        fs::path clientCacheRoot;
        if (network_init && !isHost) { 
            fs::path exe_dir = get_executable_directory();
            fs::path project_root_path = find_ancestor_directory(exe_dir, "LocalTether", 4);
            clientCacheRoot = (project_root_path.empty() ? exe_dir : project_root_path) / "client_file_cache";
            if (!fs::exists(clientCacheRoot)) {
                try {
                    fs::create_directories(clientCacheRoot);
                } catch (const fs::filesystem_error& e) {
                    Utils::Logger::GetInstance().Error("Failed to create client cache directory: " + clientCacheRoot.string() + " - " + e.what());
                }
            }
        }
         
        bool hostCreationDisabled = isMoveMode_ || isRenameMode_;
        
        ImGui::BeginDisabled(!isHost || hostCreationDisabled);  

        if (ImGui::Button(ICON_FA_SYNC_ALT " Refresh")) {  
            if (isHost && !isMoveMode_ && !isRenameMode_) {  
                RefreshView();
                BroadcastFileSystemUpdate(); 
            }
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(150);
        ImGui::InputTextWithHint("##NewFolderName", "New Folder Name", newFolderNameBuffer_, sizeof(newFolderNameBuffer_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER_PLUS " Create Folder")) {  
             if (isHost) HandleCreateFolder();  
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(150);
        ImGui::InputTextWithHint("##NewFileName", "New File Name", newFileNameBuffer_, sizeof(newFileNameBuffer_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FILE_MEDICAL " Create File")) {  
            if (isHost) HandleCreateFile();  
        }
        
        ImGui::EndDisabled();  
        
        ImGui::Separator();

        if (ImGui::BeginChild("FileSystemTree", ImVec2(0, ImGui::GetContentRegionAvail().y - 85), true)) {
            if (!rootNode_.fullPath.empty()) {
                 DrawFileSystemNode(rootNode_, ""); 
            } else {
                ImGui::Text("Storage not initialized or empty.");
                if (network_init && !isHost) {
                    ImGui::Text("Waiting for file system data from server...");
                }
            }
            ImGui::EndChild();  
        }  

        ImGui::Separator();

        const FileMetadata* selectedNodePtr = selectedPath_.empty() ? nullptr : findNodeByPathRecursiveUtil(rootNode_, selectedPath_);
        bool item_selected_for_action = selectedNodePtr != nullptr && (selectedNodePtr->fullPath != rootStoragePath_ || rootNode_.name != "Storage Root");

        if (isHost) {
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
                if (item_selected_for_action && selectedNodePtr) {
                    ImGui::Text("Selected (Host): %s", selectedNodePtr->name.c_str());
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
                    if (!selectedNodePtr->isDirectory) {
                        ImGui::SameLine();
                         
                         
                        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open")) { 
                            std::string path_to_open_str = selectedNodePtr->fullPath;  
                            Utils::Logger::GetInstance().Info("Host opening file: " + path_to_open_str);
                            #if defined(_WIN32)
                                ShellExecuteA(NULL, "open", path_to_open_str.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            #elif defined(__linux__)
                                std::string command = "xdg-open \"" + path_to_open_str + "\"";
                                system(command.c_str());
                            #elif defined(__APPLE__)
                                std::string command = "open \"" + path_to_open_str + "\"";
                                system(command.c_str());
                            #else
                                Utils::Logger::GetInstance().Warning("Open file not supported on this platform.");
                            #endif
                        }  
                    }
                } else if (!selectedPath_.empty() && selectedPath_ == rootStoragePath_ && rootNode_.name == "Storage Root") {
                     ImGui::Text("Selected: Storage Root (Host Actions disabled)");
                } else {
                    ImGui::Text("No item selected (Host).");
                }
            }  
        } else if (network_init) {  
            if (item_selected_for_action && selectedNodePtr) {
                ImGui::Text("Selected (Client): %s", selectedNodePtr->name.c_str());
                fs::path local_file_path;
                if(!clientCacheRoot.empty()) local_file_path = clientCacheRoot / selectedNodePtr->relativePath;
                
                bool is_locally_available = !local_file_path.empty() && fs::exists(local_file_path) && fs::is_regular_file(local_file_path);

                if (!selectedNodePtr->isDirectory) {
                    if (is_locally_available) {
                        ImGui::SameLine();
                        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open Local")) { 
                            std::string local_path_str = local_file_path.string();  
                            Utils::Logger::GetInstance().Info("Client opening local file: " + local_path_str);
                            #if defined(_WIN32)
                                ShellExecuteA(NULL, "open", local_path_str.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            #elif defined(__linux__)
                                std::string command = "xdg-open \"" + local_path_str + "\"";
                                system(command.c_str());
                            
                            #else
                                Utils::Logger::GetInstance().Warning("Open local file not supported on this platform.");
                            #endif
                        }  
                        ImGui::SameLine();
                        if (ImGui::Button(ICON_FA_TRASH_ALT " Delete Local")) {  
                            try {
                                if (fs::remove(local_file_path)) {
                                    Utils::Logger::GetInstance().Info("Client deleted local file: " + local_file_path.string());
                                } else {
                                    Utils::Logger::GetInstance().Error("Client failed to delete local file (fs::remove returned false): " + local_file_path.string());
                                }
                            } catch(const fs::filesystem_error& e) {
                                Utils::Logger::GetInstance().Error("Error deleting local file " + local_file_path.string() + ": " + e.what());
                            }
                        }  
                    } else {  
                        ImGui::SameLine();
                        if (ImGui::Button(ICON_FA_DOWNLOAD " Request from Server")) {  
                            Utils::Logger::GetInstance().Info("Client requesting file: " + selectedNodePtr->relativePath);
                            LocalTether::UI::getClient().requestFile(selectedNodePtr->relativePath);
                        }
                    }
                } else {  
                     ImGui::Text(" (Directory - No client actions)");
                }
            } else {  
                ImGui::Text("No item selected (Client).");
            }
        } else {  
            ImGui::Text("File Explorer (Network not initialized or role unknown)");
        }  

        if (ImGui::BeginPopupModal("Confirm Deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete '%s'?", fs::path(itemToDeletePath_).filename().string().c_str());
            ImGui::TextWrapped("This action cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                if (isHost) HandleDeleteSelected(); 
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
        fs::path parentDirToCreateIn = fs::path(rootStoragePath_);  
        if (!selectedPath_.empty() && fs::exists(selectedPath_) && fs::is_directory(selectedPath_)) {
            parentDirToCreateIn = selectedPath_;  
        }

        fs::path newFolderPath = parentDirToCreateIn / newFolderNameBuffer_;
        try {
            if (fs::create_directory(newFolderPath)) {
                Utils::Logger::GetInstance().Info("Created folder: " + newFolderPath.string());
                newFolderNameBuffer_[0] = '\0';  
                RefreshView();
                BroadcastFileSystemUpdate();
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
        fs::path parentDirToCreateIn = fs::path(rootStoragePath_);
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
                BroadcastFileSystemUpdate();
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
            BroadcastFileSystemUpdate(); 
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
            BroadcastFileSystemUpdate();
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

        fs::path sourcePath = itemToMovePath_;  
        fs::path destinationDir = moveDestinationPath_;  

        if (!fs::exists(sourcePath)) {
            Utils::Logger::GetInstance().Error("Move failed: Source item no longer exists: " + sourcePath.string());
            HandleCancelMove();
            RefreshView();  
            BroadcastFileSystemUpdate();
            return;
        }
        if (!fs::exists(destinationDir) || !fs::is_directory(destinationDir)) {
            Utils::Logger::GetInstance().Error("Move failed: Destination is not a valid directory: " + destinationDir.string());
            HandleCancelMove();
            return;
        }
        
        try { 
            if (fs::is_directory(sourcePath)) {
                fs::path current_check = destinationDir;
                while (current_check.has_parent_path() && current_check != current_check.parent_path()) {
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
        
            if (fs::equivalent(sourcePath.parent_path(), destinationDir) && sourcePath.filename() == (destinationDir / sourcePath.filename()).filename()) { 
                Utils::Logger::GetInstance().Info("Item is already in the target directory. No move performed.");
                HandleCancelMove();
                return;
            }
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Filesystem error during move safety checks: " + std::string(e.what()));
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
            HandleCancelMove();  
            RefreshView();
            BroadcastFileSystemUpdate();
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error moving item: " + std::string(e.what()));
            HandleCancelMove(); 
        }
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

        fs::path sourcePath = itemToRenamePath_;  
        if (!fs::exists(sourcePath)) {
            Utils::Logger::GetInstance().Error("Rename failed: Source item no longer exists: " + sourcePath.string());
            HandleCancelRename();
            RefreshView();
            BroadcastFileSystemUpdate(); 
            return;
        }
        
        std::string newNameStr(renameBuffer_);
        if (newNameStr.find('/') != std::string::npos || newNameStr.find('\\') != std::string::npos) {
            Utils::Logger::GetInstance().Error("Rename failed: New name contains invalid characters ('/' or '\\').");
            return;
        }

        fs::path newPath = sourcePath.parent_path() / renameBuffer_;
        Utils::Logger::GetInstance().Debug("Renaming from " + sourcePath.string() + " to " + newPath.string());
        
        try { 
            if (fs::exists(newPath) && !fs::equivalent(sourcePath, newPath)) {
                Utils::Logger::GetInstance().Error("Rename failed: An item with the name '" + newNameStr + "' already exists.");
                return;
            }
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Filesystem error during rename safety check: " + std::string(e.what()));
            return;
        }
        
        try {
            fs::rename(sourcePath, newPath);
            Utils::Logger::GetInstance().Info("Renamed '" + sourcePath.string() + "' to '" + newPath.string() + "'");
            HandleCancelRename();  
            RefreshView();
            BroadcastFileSystemUpdate();
        } catch (const fs::filesystem_error& e) {
            Utils::Logger::GetInstance().Error("Error renaming item: " + std::string(e.what()));
            HandleCancelRename(); 
        }
    }

    void FileExplorerPanel::HandleCancelRename() {
        isRenameMode_ = false;
        itemToRenamePath_.clear();
        renameBuffer_[0] = '\0';
        Utils::Logger::GetInstance().Info("Rename cancelled.");
    }

}  