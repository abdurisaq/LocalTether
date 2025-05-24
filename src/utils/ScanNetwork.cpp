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
        // fork failed
        perror("fork failed");
    }
}
#endif

std::vector<std::string> scanForServer(std::atomic<bool> & running) {
#ifdef _WIN32
    std::string scriptPath = getScriptPath();
    runScript(scriptPath);
#else
    std::string scriptPath = getScriptPath();
    runScript(scriptPath);
#endif

    char buffer[1024];
    std::vector<std::string> foundIps;
    std::string ipAddr = (findProjectRoot("LocalTether", 4) / "scripts" / "ipAddress.txt").string();

    FILE* file = fopen(ipAddr.c_str(), "r");
    if (file == NULL) {
        printf("Failed to open file\n");
        exit(1);
    }

    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        foundIps.push_back(buffer);
    }
    fclose(file);

    running = false;

    return foundIps;
}

