

#define ASIO_ENABLE_SSL
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "utils/ScanNetwork.h"

std::filesystem::path findProjectRoot(const std::string& targetDirName, int maxDepth) {
    namespace fs = std::filesystem;

    fs::path current = fs::current_path();
    int depth = 0;

    while (!current.empty() && depth < maxDepth) {
        if (current.filename() == targetDirName) {
            return current;
        }
        current = current.parent_path();
        ++depth;
    }

    throw std::runtime_error("Project root '" + targetDirName + "' not found within max depth.");
}

std::string getScriptPath() {
    try {
        auto root = findProjectRoot("LocalTether", 4);

#ifdef _WIN32
        return (root / "scripts" / "scanLan.ps1").string();
#else
        return (root / "scripts" / "scanLan.sh").string();
#endif
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return "";
    }
}

#ifdef _WIN32
void runScript(const std::string &scriptPath) {
    
    std::string cmdLine = "pwsh -NoLogo -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + scriptPath + "\"";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; 

    BOOL success = CreateProcessA(
        NULL,
        cmdLine.data(),  
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        std::cerr << "Failed to start PowerShell script. Error code: " << GetLastError() << "\n";
        return;
    }

   
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode == 0) {
        std::cout << "Command succeeded.\n";
    } else {
        std::cout << "Command exited with code: " << exitCode << "\n";
    }
}
#else
void runScript(const std::string &scriptPath) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        execl("/bin/bash", "bash", scriptPath.c_str(), (char*) nullptr);

        perror("execl failed");
        _exit(1);
    } else if (pid > 0) {
       
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exitCode = WEXITSTATUS(status);
            if (exitCode == 0) {
                std::cout << "Command succeeded.\n";
            } else {
                std::cout << "Command exited with code: " << exitCode << "\n";
            }
        } else {
            std::cout << "Command did not terminate normally.\n";
        }
    } else {
         
        perror("fork failed");
    }
}
#endif

std::vector<std::string> scanForServer(std::atomic<bool> & running) {
     
#ifdef _WIN32
    std::string scriptPath = getScriptPath();
    if (!scriptPath.empty()) {
         
        runScript(scriptPath);
    } else {
        std::cerr << "ScanNetwork: Could not get Windows script path." << std::endl;
    }
#else
    std::string scriptPath = getScriptPath();
    if (!scriptPath.empty()) {
         
        runScript(scriptPath);
    } else {
        std::cerr << "ScanNetwork: Could not get Linux script path." << std::endl;
    }
#endif

    std::vector<std::string> foundIpsFromFile;
    std::filesystem::path ipAddrFilePath;
    try {
         
        std::filesystem::path rootDir = findProjectRoot("LocalTether", 4);
        if (rootDir.empty()) {
             std::cerr << "ScanNetwork: Project root 'LocalTether' not found. Cannot locate ipAddress.txt." << std::endl;
             running = false;
             return {};
        }
        ipAddrFilePath = rootDir / "scripts" / "ipAddress.txt";
    } catch (const std::exception& e) {
        std::cerr << "ScanNetwork: Error finding project root or ipAddress.txt path: " << e.what() << std::endl;
        running = false;
        return {};
    }

     
    FILE* file = fopen(ipAddrFilePath.string().c_str(), "r");
    if (file == NULL) {
        std::cerr << "ScanNetwork: Failed to open ipAddress.txt at " << ipAddrFilePath.string() << std::endl;
        running = false;
        return {};
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = '\0';  
        if (strlen(buffer) > 0) {  
            foundIpsFromFile.push_back(buffer);
        }
    }
    fclose(file);

    if (foundIpsFromFile.empty()) {
        std::cout << "ScanNetwork: No IPs found by the script in " << ipAddrFilePath.string() << std::endl;
    } else {
         
    }

     
    std::vector<std::string> verifiedLocalTetherServers;
    asio::io_context io_context;
    uint16_t localTetherPort = 8080;  

     

    for (const std::string& ip_str : foundIpsFromFile) {
        if (!running.load()) {
            std::cout << "ScanNetwork: Scan aborted by flag." << std::endl;
            break;
        }

         

        try {
            asio::ip::tcp::resolver resolver(io_context);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip_str), localTetherPort);

            asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);
             
             
            ssl_ctx.set_verify_mode(asio::ssl::verify_none);

            asio::ssl::stream<asio::ip::tcp::socket> ssl_socket(io_context, ssl_ctx);

            std::error_code ec_connect;
             
             
             
            ssl_socket.lowest_layer().connect(endpoint, ec_connect);

            if (ec_connect) {
                 
                io_context.restart();  
                continue;
            }

             

            std::error_code ec_handshake;
            ssl_socket.handshake(asio::ssl::stream_base::client, ec_handshake);

            if (!ec_handshake) {
                std::cout << "ScanNetwork: LocalTether server confirmed at " << ip_str << " (SSL handshake OK)" << std::endl;
                verifiedLocalTetherServers.push_back(ip_str);
                
                std::error_code ec_ssl_shutdown;
                ssl_socket.shutdown(ec_ssl_shutdown);  
            } else {
                 
            }
            
             
            if (ssl_socket.lowest_layer().is_open()) {
                std::error_code ec_close;
                ssl_socket.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec_close);  
                ssl_socket.lowest_layer().close(ec_close);
            }

        } catch (const std::system_error& se) {
             
        } catch (const std::exception& e) {
             
        }
        io_context.restart();  
    }

    std::cout << "ScanNetwork: Verification complete. Found " << verifiedLocalTetherServers.size() << " active LocalTether server(s)." << std::endl;

    running = false;  
    return verifiedLocalTetherServers;
}