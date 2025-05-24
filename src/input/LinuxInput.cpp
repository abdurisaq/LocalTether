#ifndef _WIN32
#include "input/LinuxInput.h"
#include "network/Message.h" 
#include "utils/Logger.h"    

#include <iostream>
#include <unistd.h>     
#include <sys/wait.h>  
#include <sys/prctl.h>  
#include <csignal>     
#include <cstdlib>
#include <filesystem>
#include <fstream>      
#include <chrono>     
#include <fcntl.h>      // For O_RDONLY, O_RDWR
#include <sys/mman.h>   // For mmap, shm_open
#include <sys/stat.h>   // For mode constants

#ifndef PATH_MAX
#include <limits.h>
#endif

struct HelperSharedData {
    pid_t helper_pid;
    char socket_path[256];
    bool ready;
};


namespace LT = LocalTether;

namespace LocalTether::Input {

std::string LinuxInput::getExecutablePath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        return std::string(result, count);
    }
    LT::Utils::Logger::GetInstance().Error("LinuxInput: Could not determine executable path for input helper via /proc/self/exe. Error: " + std::string(strerror(errno)));
    return "";
}

LinuxInput::LinuxInput() : ipc_socket_(ipc_io_context_) {
    LT::Utils::Logger::GetInstance().Info("LinuxInput constructor.");
}

LinuxInput::~LinuxInput() {
    stop();
    close_and_unmap_shared_memory(); 
}

bool LinuxInput::open_and_map_shared_memory() {
    if (shared_data_ptr_ != nullptr && shared_data_ptr_ != MAP_FAILED) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Shared memory already mapped.");
        return true;
    }

    shm_fd_ = shm_open(shm_name_, O_RDWR, 0666); 
    if (shm_fd_ == -1) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: shm_open failed (may not exist yet): " + std::string(strerror(errno)));
        return false;
    }

    shared_data_ptr_ = (HelperSharedData*)mmap(NULL, sizeof(HelperSharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (shared_data_ptr_ == MAP_FAILED) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: mmap failed: " + std::string(strerror(errno)));
        close(shm_fd_);
        shm_fd_ = -1;
        shared_data_ptr_ = nullptr;
        return false;
    }
    LT::Utils::Logger::GetInstance().Debug("LinuxInput: Shared memory segment " + std::string(shm_name_) + " opened and mapped.");
    return true;
}

void LinuxInput::close_and_unmap_shared_memory() {
    if (shared_data_ptr_ != nullptr && shared_data_ptr_ != MAP_FAILED) {
        munmap(shared_data_ptr_, sizeof(HelperSharedData));
        shared_data_ptr_ = nullptr;
    }
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    LT::Utils::Logger::GetInstance().Debug("LinuxInput: Shared memory unmapped and closed.");
}

bool LinuxInput::read_info_from_shared_memory(pid_t& out_pid, std::string& out_socket_path) {
    if (!shared_data_ptr_ || shared_data_ptr_ == MAP_FAILED) {
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Attempted to read from unmapped shared memory.");
        return false;
    }

    if (shared_data_ptr_->ready) {
        out_pid = shared_data_ptr_->helper_pid;
        out_socket_path = shared_data_ptr_->socket_path;
        if (out_pid > 0 && !out_socket_path.empty()) {
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Read helper PID " + std::to_string(out_pid) + " and socket path '" + out_socket_path + "' from shared memory.");
            return true;
        }
    }
    return false;
}


bool LinuxInput::launchHelperProcess() {
    
    if (open_and_map_shared_memory()) {
        if (read_info_from_shared_memory(helper_actual_pid_, actual_helper_socket_path_)) {
            if (helper_actual_pid_ > 0 && kill(helper_actual_pid_, 0) == 0) {
                 LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper process (PID " + std::to_string(helper_actual_pid_) + ") detected via shared memory. Attempting to connect.");
                
                 return true; 
            } else {
                LT::Utils::Logger::GetInstance().Warning("LinuxInput: Stale PID " + std::to_string(helper_actual_pid_) + " in shared memory. Will attempt to launch new helper.");
                close_and_unmap_shared_memory(); 
                shm_unlink(shm_name_); 
            }
        } else {
            
            close_and_unmap_shared_memory();
            shm_unlink(shm_name_);
        }
    } else {
         
         shm_unlink(shm_name_);
    }


    std::string exePath = getExecutablePath();
    if (exePath.empty()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Cannot launch input helper: executable path unknown.");
        return false;
    }
    
    // Clean up any old socket file that might exist at a predictable path if we are about to launch.
    // This is less critical now as the path is dynamic and communicated via SHM.
    // std::filesystem::remove(actual_helper_socket_path_); // actual_helper_socket_path_ is not known yet

    std::string command_str_log = "pkexec " + exePath + " --input-helper-mode";
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to launch helper: " + command_str_log);

    pkexec_pid_ = fork();
    if (pkexec_pid_ == 0) { 
        prctl(PR_SET_PDEATHSIG, SIGHUP);
        int dev_null_fd = open("/dev/null", O_WRONLY);
        if (dev_null_fd != -1) {
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);
            close(dev_null_fd);
        }
        execlp("pkexec", "pkexec", exePath.c_str(), "--input-helper-mode", (char*)nullptr);
        perror("LinuxInput: execlp pkexec failed"); 
        _exit(127); 
    } else if (pkexec_pid_ < 0) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to fork for pkexec. Error: " + std::string(strerror(errno)));
        return false;
    }

    LT::Utils::Logger::GetInstance().Info("LinuxInput: pkexec process forked (PID: " + std::to_string(pkexec_pid_) + "). Waiting for helper to signal readiness via shared memory.");
    
    // Detached thread to reap the pkexec child process
    std::thread([pid = pkexec_pid_]() {
        int status;
        waitpid(pid, &status, 0);
        // Optionally log status
    }).detach();

    return true; 
}

bool LinuxInput::connectToHelper() {
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to connect to input helper.");
    int connect_retries = 20; // Increased retries for SHM readiness
    bool shm_info_read = false;

    while (connect_retries-- > 0) {
        if (!shm_info_read) {
            if (!open_and_map_shared_memory()) {
                 LT::Utils::Logger::GetInstance().Debug("LinuxInput: SHM not available yet. Retrying... (attempts left: " + std::to_string(connect_retries) + ")");
                 std::this_thread::sleep_for(std::chrono::milliseconds(250));
                 continue;
            }
            if (read_info_from_shared_memory(helper_actual_pid_, actual_helper_socket_path_)) {
                shm_info_read = true;
                // Keep SHM mapped for now
            } else {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: Helper info not ready in SHM. Retrying... (attempts left: " + std::to_string(connect_retries) + ")");
                // Don't unmap yet, helper might still be writing.
                 std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }
        }

        if (!shm_info_read || actual_helper_socket_path_.empty()) {
            LT::Utils::Logger::GetInstance().Warning("LinuxInput: Helper socket path not available from SHM. Retrying... (attempts left: " + std::to_string(connect_retries) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            shm_info_read = false; // Force re-read of SHM
            continue;
        }
        
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to connect to socket: " + actual_helper_socket_path_);

        try {
            ipc_socket_.connect(asio::local::stream_protocol::endpoint(actual_helper_socket_path_));
            helper_connected_ = true;
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Connected to input helper (PID: " + std::to_string(helper_actual_pid_) + " via socket " + actual_helper_socket_path_ + ").");

            ipc_io_context_.reset(); 
            ipc_thread_ = std::thread([this]() {
                LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC thread started.");
                try {
                    ipc_io_context_.run();
                } catch (const std::exception& e) {
                    LT::Utils::Logger::GetInstance().Error("LinuxInput: IPC io_context error: " + std::string(e.what()));
                    helper_connected_ = false; 
                }
                LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC thread finished.");
            });
            readFromHelperLoop();
            return true;
        } catch (const std::system_error& e) {
            if (connect_retries == 0) {
                 LT::Utils::Logger::GetInstance().Error(std::string("LinuxInput: Failed to connect to helper on final attempt: ") + e.what());
            } else {
                 LT::Utils::Logger::GetInstance().Debug("LinuxInput: Failed to connect to helper socket (attempt " + std::to_string(20 - connect_retries) + "): " + e.what() + ". Retrying in 1s...");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to input helper after multiple retries (SHM or socket).");
    close_and_unmap_shared_memory(); // Clean up SHM if connection failed
    return false;
}

bool LinuxInput::start() {
    if (running_) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Already running.");
        return true;
    }
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Starting...");
    running_ = true;

    if (!launchHelperProcess()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to launch helper process.");
        running_ = false;
        return false;
    }

    if (!connectToHelper()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to helper process.");
        cleanupHelperProcess(); // Attempt to kill pkexec if connection failed
        running_ = false;
        return false;
    }
    
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Started successfully.");
    return true;
}

void LinuxInput::cleanupHelperProcess() {
    // This function now primarily targets the pkexec_pid_ if it's known,
    // or the helper_actual_pid_ if pkexec_pid_ is not set (e.g. helper was already running).
    pid_t pid_to_kill = -1;
    std::string desc_to_kill;

    if (pkexec_pid_ > 0) {
        pid_to_kill = pkexec_pid_;
        desc_to_kill = "pkexec process";
    } else if (helper_actual_pid_ > 0) {
        pid_to_kill = helper_actual_pid_;
        desc_to_kill = "helper process";
    }

    if (pid_to_kill != -1) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to terminate " + desc_to_kill + " (PID: " + std::to_string(pid_to_kill) + ")");
        if (kill(pid_to_kill, SIGTERM) == 0) {
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Sent SIGTERM to PID: " + std::to_string(pid_to_kill));
            // Optionally wait for a bit and send SIGKILL if still alive
        } else {
            // It might have already exited, or we don't have permission (less likely for pkexec_pid)
            // LT::Utils::Logger::GetInstance().Warning("LinuxInput: Failed to send SIGTERM to PID " + std::to_string(pid_to_kill) + ". Error: " + strerror(errno));
        }
    }
    pkexec_pid_ = -1; 
    helper_actual_pid_ = -1;

    // The helper is responsible for unlinking its socket file and SHM on clean exit.
    // Parent might try to unlink SHM if it thinks it's stale.
    // shm_unlink(shm_name_); // Consider if parent should do this aggressively
}


void LinuxInput::stop() {
    if (!running_) {
        // LT::Utils::Logger::GetInstance().Info("LinuxInput: Already stopped or not started.");
        return;
    }
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopping...");
    running_ = false; 

    if (helper_connected_ && ipc_socket_.is_open()) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Closing IPC socket.");
        asio::error_code ec;
        ipc_socket_.shutdown(asio::local::stream_protocol::socket::shutdown_both, ec);
        ipc_socket_.close(ec);
    }
    helper_connected_ = false;

    if (ipc_io_context_.stopped()) {
         LT::Utils::Logger::GetInstance().Debug("LinuxInput: IPC io_context was already stopped.");
    } else {
        ipc_io_context_.stop();
    }

    if (ipc_thread_.joinable()) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Joining IPC thread...");
        ipc_thread_.join();
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: IPC thread joined.");
    }
    
    cleanupHelperProcess(); 
    close_and_unmap_shared_memory(); // Unmap SHM from parent side

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopped.");
}

void LinuxInput::readFromHelperLoop() {
    if (!helper_connected_ || !ipc_socket_.is_open() || !running_) {
        return;
    }

    ipc_socket_.async_read_some(asio::buffer(ipc_read_buffer_),
        [this](const std::error_code& ec, std::size_t bytes_transferred) {
            if (!running_) return; // Stopped during async op

            if (!ec) {
                if (bytes_transferred >= sizeof(LT::Network::InputPayload)) { 
                    LT::Network::InputPayload payload;
                    memcpy(&payload, ipc_read_buffer_.data(), sizeof(LT::Network::InputPayload)); 
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        received_payloads_queue_.push_back(payload);
                    }
                } else if (bytes_transferred > 0) {
                    LT::Utils::Logger::GetInstance().Warning("LinuxInput: Received partial/invalid data from helper: " + std::to_string(bytes_transferred) + " bytes.");
                }
                
                if (helper_connected_.load(std::memory_order_relaxed) && running_.load(std::memory_order_relaxed)) {
                    readFromHelperLoop(); // Continue reading
                }
            } else {
                if (ec != asio::error::operation_aborted) { // Aborted is expected on stop
                    LT::Utils::Logger::GetInstance().Error("LinuxInput: Error reading from helper IPC: " + ec.message());
                }
                helper_connected_ = false; 
            }
        });
}

std::vector<LT::Network::InputPayload> LinuxInput::pollEvents() {
    if (!running_.load(std::memory_order_relaxed)) return {};
    if (!helper_connected_.load(std::memory_order_relaxed)) return {};

    std::vector<LT::Network::InputPayload> current_payloads;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!received_payloads_queue_.empty()) {
            current_payloads.swap(received_payloads_queue_);
        }
    }
    return current_payloads;
}

void LinuxInput::sendCommandToHelper(IPCCommandType cmdType, const std::vector<uint8_t>& data) {
    if (!helper_connected_ || !ipc_socket_.is_open()) {
        return;
    }
    std::vector<uint8_t> message;
    message.push_back(static_cast<uint8_t>(cmdType));
    message.insert(message.end(), data.begin(), data.end());

    asio::async_write(ipc_socket_, asio::buffer(message),
        [this, cmdType](const std::error_code& ec, std::size_t ) {
            if (ec && ec != asio::error::operation_aborted) {
                LT::Utils::Logger::GetInstance().Error("LinuxInput: Error writing command " + std::to_string(static_cast<int>(cmdType)) + " to helper IPC: " + ec.message());
                helper_connected_ = false;
            }
        });
}

void LinuxInput::sendPayloadToHelper(IPCCommandType cmdType, const LT::Network::InputPayload& payload) {
     if (!helper_connected_ || !ipc_socket_.is_open()) {
        return;
    }
    std::vector<uint8_t> message;
    message.push_back(static_cast<uint8_t>(cmdType));
    const uint8_t* payload_bytes = reinterpret_cast<const uint8_t*>(&payload);
    message.insert(message.end(), payload_bytes, payload_bytes + sizeof(payload));

    asio::async_write(ipc_socket_, asio::buffer(message),
        [this, cmdType](const std::error_code& ec, std::size_t ) {
            if (ec && ec != asio::error::operation_aborted) {
                LT::Utils::Logger::GetInstance().Error("LinuxInput: Error writing payload cmd " + std::to_string(static_cast<int>(cmdType)) + " to helper IPC: " + ec.message());
                helper_connected_ = false;
            }
        });
}


void LinuxInput::simulateInput(const LT::Network::InputPayload& payload) {
    if (!running_ || !helper_connected_) {
        return;
    }
    sendPayloadToHelper(IPCCommandType::SimulateInput, payload);
}

void LinuxInput::setInputPaused(bool paused) {
    if (local_pause_active_ == paused) return; 
    local_pause_active_ = paused;
    sendCommandToHelper(paused ? IPCCommandType::PauseStream : IPCCommandType::ResumeStream);
}

bool LinuxInput::isInputPaused() const {
    return local_pause_active_.load(std::memory_order_relaxed);
}

} 
#endif 