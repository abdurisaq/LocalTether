#pragma once
#include "imgui_include.h"
#include <string>
#include <vector>
#include <filesystem>  
#include <chrono>      
#include <unordered_map> 
#include <filesystem>

#include "ui/UIState.h"

#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/chrono.hpp>


namespace LocalTether::Network {
    class Message;
    class Client;  
    class Server;  
     
}


namespace LocalTether::UI::Panels {

    enum class FileSyncState {
        SyncedWithServer, 
        LocalCacheOnly,   
        ServerOnly        
    };

    std::filesystem::path get_executable_directory();
    std::filesystem::path find_ancestor_directory(const std::filesystem::path& start_path, const std::string& target_dir_name, int max_depth);


    struct FileMetadata {
        std::string name;
        std::string fullPath;      
        std::string relativePath;  
        bool isDirectory = false;
        uintmax_t size = 0;
        std::chrono::system_clock::time_point modifiedTime;
        std::vector<FileMetadata> children;
        
        
        FileSyncState syncState = FileSyncState::ServerOnly; 
        bool isCachedLocally = false;                      

        template<class Archive>
        void serialize(Archive & archive) {
            
            
            archive(CEREAL_NVP(name), CEREAL_NVP(fullPath), CEREAL_NVP(relativePath),
                    CEREAL_NVP(isDirectory), CEREAL_NVP(size),
                    CEREAL_NVP(modifiedTime), CEREAL_NVP(children));
        }
    };
    


    class FileExplorerPanel {
    public:
        FileExplorerPanel();
        
        void Show(bool* p_open = nullptr);
        const FileMetadata& getRootNode() const;
        void SetRootNode(const FileMetadata& newRootNode);

        void HandleExternalFileDragOver(const ImVec2& mouse_pos_in_window);
        void HandleExternalFileDrop(const std::string& dropped_file_path);
        void ClearExternalDragState();

        void RefreshView();

        
        void BroadcastFileSystemUpdate();
        
    private:
        std::string rootStoragePath_;
        FileMetadata rootNode_; 
        
         
        std::string selectedPath_; 
        char newFolderNameBuffer_[256];
        char newFileNameBuffer_[256];
        char itemToDeletePath_[1024]; 

         
        bool isMoveMode_ = false;
        std::string itemToMovePath_;       
        std::string moveDestinationPath_;  

        bool isRenameMode_ = false;
        std::string itemToRenamePath_;     
        char renameBuffer_[256];        
        
        bool is_external_drag_over_panel_ = false;
        std::string external_drag_target_folder_display_name_;
        ImVec2 last_panel_pos_ = ImVec2(0,0);       
        ImVec2 last_panel_size_ = ImVec2(0,0);      
        std::filesystem::path current_drop_target_dir_;

         
        void InitializeStorage(); 
        void ScanDirectoryRecursive(const std::filesystem::path& dirPath, FileMetadata& parentNode);
        void DrawFileSystemNode(FileMetadata& node, const std::string& current_node_path_prefix);
        
         
        void HandleCreateFolder();
        void HandleCreateFile();
        void HandleDeleteSelected();

         
        void HandleInitiateMove();
        void HandleConfirmMove();
        void HandleCancelMove();

        void HandleInitiateRename();
        void HandleConfirmRename();
        void HandleCancelRename();

        
    };
}