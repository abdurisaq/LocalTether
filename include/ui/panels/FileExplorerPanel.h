#pragma once
#include "imgui_include.h"
#include <string>
#include <vector>
#include <filesystem>  
#include <chrono>      
#include <unordered_map>  

namespace LocalTether::UI::Panels {
    

    struct FileMetadata {
        std::string name;                
        std::string fullPath;            
        std::string relativePath;        
        bool isDirectory;
        uint64_t size;                   
        std::chrono::system_clock::time_point modifiedTime;
         
        std::vector<FileMetadata> children;  

         
        FileMetadata() : isDirectory(false), size(0) {}
    };

    class FileExplorerPanel {
    public:
        FileExplorerPanel();
        
        void Show(bool* p_open = nullptr);
        
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

         
        void InitializeStorage(); 
        void ScanDirectoryRecursive(const std::filesystem::path& dirPath, FileMetadata& parentNode);
        void RefreshView();

         
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