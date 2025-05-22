#pragma once
#include <filesystem>
#include <iostream>
#include <vector>
#include <atomic>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

void runScript(const std::string &scriptPath);

std::vector<std::string> scanForServer(std::atomic<bool> &running);


std::filesystem::path findProjectRoot(const std::string& targetDirName, int maxDepth = 4);


std::string getScriptPath();
