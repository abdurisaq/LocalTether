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

#ifndef PATH_MAX
#include <limits.h>
#endif


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
}

bool LinuxInput::readHelperPidFile() {
    std::ifstream pid_file(helper_pid_file_path_);
    if (pid_file.is_open()) {
        pid_t pid_from_file = -1;
        pid_file >> pid_from_file;
        pid_file.close();
        if (pid_from_file > 0) {
            
            if (kill(pid_from_file, 0) == 0) { 
                helper_pid_ = pid_from_file;
                LT::Utils::Logger::GetInstance().Info("LinuxInput: Read helper PID " + std::to_string(helper_pid_) + " from file.");
                return true;
            } else {
                LT::Utils::Logger::GetInstance().Warning("LinuxInput: PID " + std::to_string(pid_from_file) + " from PID file does not exist or no permission. Removing stale PID file.");
                std::filesystem::remove(helper_pid_file_path_);
            }
        } else {
             LT::Utils::Logger::GetInstance().Warning("LinuxInput: Invalid PID read from PID file: " + std::to_string(pid_from_file));
        }
    } else {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: PID file not found: " + helper_pid_file_path_);
    }
    helper_pid_ = -1;
    return false;
}


bool LinuxInput::launchHelperProcess() {
    
    if (readHelperPidFile() && helper_pid_ != -1) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper process (PID " + std::to_string(helper_pid_) + ") might already be running. Attempting to connect.");
        return true; 
    }


    std::string exePath = getExecutablePath();
    if (exePath.empty()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Cannot launch input helper: executable path unknown.");
        return false;
    }

    std::filesystem::remove(helper_socket_path_);
    

    //attempting
    std::string command_str_log = "pkexec " + exePath + " --input-helper-mode";
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to launch helper: " + command_str_log);

    pid_t pid = fork();
    if (pid == 0) { 
        // Set PDEATHSIG to SIGHUP so this child (which runs pkexec) exits if the main app (parent) dies.
        prctl(PR_SET_PDEATHSIG, SIGHUP);

        //dont want it to output to terminal so send to null
        int dev_null_fd = open("/dev/null", O_WRONLY);
        if (dev_null_fd != -1) {
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);
            close(dev_null_fd);
        }
        execlp("pkexec", "pkexec", /*"--disable-internal-agent",*/ exePath.c_str(), "--input-helper-mode", (char*)nullptr);
        perror("LinuxInput: execlp pkexec failed"); // This might go to /dev/null
        _exit(127); 
    } else if (pid < 0) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to fork for pkexec. Error: " + std::string(strerror(errno)));
        return false;
    }

    LT::Utils::Logger::GetInstance().Info("LinuxInput: pkexec process forked (PID: " + std::to_string(pid) + "). Waiting for helper to start and write PID file.");
  
    return true; // successfully launched
}

bool LinuxInput::connectToHelper() {
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to connect to input helper via " + helper_socket_path_);
    int connect_retries = 10; 
    bool pid_file_found_this_attempt = false;

    while (connect_retries-- > 0) {
        if (!pid_file_found_this_attempt) { // Only try to read PID file once per connection attempt sequence
            if (readHelperPidFile() && helper_pid_ != -1) {
                pid_file_found_this_attempt = true; 
            } else if (connect_retries < 5) { 
                 LT::Utils::Logger::GetInstance().Warning("LinuxInput: Helper PID file not found yet. Retrying connection...");
            }
        }

        try {
            ipc_socket_.connect(asio::local::stream_protocol::endpoint(helper_socket_path_));
            helper_connected_ = true;
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Connected to input helper (PID: " + (helper_pid_ != -1 ? std::to_string(helper_pid_) : "unknown") + ").");

            ipc_io_context_.reset(); 
            ipc_thread_ = std::thread([this]() {
                LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC thread started.");
                try {
                    ipc_io_context_.run();
                } catch (const std::exception& e) {
                    LT::Utils::Logger::GetInstance().Error("LinuxInput: IPC io_context error: " + std::string(e.what()));
                    helper_connected_ = false; // Mark as disconnected
                }
                LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC thread finished.");
            });
            readFromHelperLoop();
            return true;
        } catch (const std::system_error& e) {
            if (connect_retries == 0) {
                 LT::Utils::Logger::GetInstance().Error(std::string("LinuxInput: Failed to connect to helper on final attempt: ") + e.what());
            } else {
                 LT::Utils::Logger::GetInstance().Debug("LinuxInput: Failed to connect to helper (attempt " + std::to_string(10 - connect_retries) + "): " + e.what() + ". Retrying in 1s...");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to input helper after multiple retries.");
    return false;
}

bool LinuxInput::start() {
    if (running_) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Already running.");
        return true;
    }
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Starting...");

    if (!launchHelperProcess()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to initiate helper process launch.");
        return false;
    }

    if (!connectToHelper()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to helper process after launch attempt.");
        cleanupHelperProcess(); // cleanup if you cant connect
        return false;
    }

    running_ = true;
    local_pause_active_ = false; 
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Started successfully.");
    return true;
}

void LinuxInput::cleanupHelperProcess() {
    if (helper_pid_ != -1) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to terminate helper process PID: " + std::to_string(helper_pid_));
        if (kill(helper_pid_, SIGTERM) == 0) {
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Sent SIGTERM to helper PID: " + std::to_string(helper_pid_));
        } else {
            LT::Utils::Logger::GetInstance().Warning("LinuxInput: Failed to send SIGTERM to helper PID " + std::to_string(helper_pid_) + ". Error: " + strerror(errno));
        }
        helper_pid_ = -1; // Reset PID
    }
    
    std::filesystem::remove(helper_pid_file_path_);
    std::filesystem::remove(helper_socket_path_);
}


void LinuxInput::stop() {
    if (!running_) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Already stopped or not started.");
        return;
    }
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopping...");
    running_ = false; 

    if (helper_connected_ && ipc_socket_.is_open()) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Sending shutdown command to helper.");

        asio::error_code ec;
        ipc_socket_.shutdown(asio::local::stream_protocol::socket::shutdown_both, ec);
        if (ec) LT::Utils::Logger::GetInstance().Warning("LinuxInput: IPC socket shutdown error: " + ec.message());
        ipc_socket_.close(ec);
        if (ec) LT::Utils::Logger::GetInstance().Warning("LinuxInput: IPC socket close error: " + ec.message());
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
    } else {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: IPC thread not joinable on stop.");
    }

    cleanupHelperProcess(); 

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopped.");
}

void LinuxInput::readFromHelperLoop() {
    if (!helper_connected_ || !ipc_socket_.is_open()) {
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Cannot start read loop, not connected.");
        return;
    }

    ipc_socket_.async_read_some(asio::buffer(ipc_read_buffer_),
        [this](const std::error_code& ec, std::size_t bytes_transferred) {
            if (!ec) {
                if (bytes_transferred >= sizeof(LT::Network::InputPayload)) { // Basic check
                    LT::Network::InputPayload payload;
                    memcpy(&payload, ipc_read_buffer_.data(), sizeof(LT::Network::InputPayload)); // Simplistic

                    {
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        received_payloads_queue_.push_back(payload);
                    }
                    LT::Utils::Logger::GetInstance().Debug("LinuxInput: Received payload from helper.");
                } else if (bytes_transferred > 0) {
                    LT::Utils::Logger::GetInstance().Warning("LinuxInput: Received partial/invalid data from helper: " + std::to_string(bytes_transferred) + " bytes.");
                }
                
                if (helper_connected_.load(std::memory_order_relaxed) && running_.load(std::memory_order_relaxed)) {
                    readFromHelperLoop();
                } else {
                    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopping read loop as helper disconnected or not running.");
                }
            } else {
                if (ec == asio::error::eof || ec == asio::error::connection_reset) {
                    LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper IPC disconnected (EOF/reset).");
                } else if (ec == asio::error::operation_aborted) {
                    LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper IPC read operation aborted (likely during stop).");
                }
                else {
                    LT::Utils::Logger::GetInstance().Error("LinuxInput: Error reading from helper IPC: " + ec.message());
                }
                helper_connected_ = false; 
                
            }
        });
}

std::vector<LT::Network::InputPayload> LinuxInput::pollEvents() {
    if (!running_.load(std::memory_order_relaxed)) {
        return {};
    }
    if (!helper_connected_.load(std::memory_order_relaxed)) {
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: pollEvents called but helper not connected.");
        return {};
    }

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
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Cannot send command to helper, not connected. Cmd: " + std::to_string(static_cast<int>(cmdType)));
        return;
    }

    std::vector<uint8_t> message;
    message.push_back(static_cast<uint8_t>(cmdType));
    message.insert(message.end(), data.begin(), data.end());

    asio::async_write(ipc_socket_, asio::buffer(message),
        [this, cmdType](const std::error_code& ec, std::size_t ) {
            if (ec) {
                LT::Utils::Logger::GetInstance().Error("LinuxInput: Error writing command " + std::to_string(static_cast<int>(cmdType)) + " to helper IPC: " + ec.message());
                helper_connected_ = false;
            } else {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: Successfully sent command " + std::to_string(static_cast<int>(cmdType)) + " to helper.");
            }
        });
}
void LinuxInput::sendPayloadToHelper(IPCCommandType cmdType, const LT::Network::InputPayload& payload) {
     if (!helper_connected_ || !ipc_socket_.is_open()) {
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Cannot send payload to helper, not connected. Cmd: " + std::to_string(static_cast<int>(cmdType)));
        return;
    }
    std::vector<uint8_t> message;
    message.push_back(static_cast<uint8_t>(cmdType));
    const uint8_t* payload_bytes = reinterpret_cast<const uint8_t*>(&payload);
    message.insert(message.end(), payload_bytes, payload_bytes + sizeof(payload));

    asio::async_write(ipc_socket_, asio::buffer(message),
        [this, cmdType](const std::error_code& ec, std::size_t /*bytes_transferred*/) {
            if (ec) {
                LT::Utils::Logger::GetInstance().Error("LinuxInput: Error writing payload cmd " + std::to_string(static_cast<int>(cmdType)) + " to helper IPC: " + ec.message());
                helper_connected_ = false;
            } else {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: Successfully sent payload cmd " + std::to_string(static_cast<int>(cmdType)) + " to helper.");
            }
        });
}


void LinuxInput::simulateInput(const LT::Network::InputPayload& payload) {
    if (!running_ || !helper_connected_) {
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Cannot simulate input, not running or helper not connected.");
        return;
    }
    LT::Utils::Logger::GetInstance().Debug("LinuxInput: Queuing input simulation request to helper.");
    sendPayloadToHelper(IPCCommandType::SimulateInput, payload);
}

void LinuxInput::setInputPaused(bool paused) {
    if (local_pause_active_ == paused) return; 

    local_pause_active_ = paused;
    if (paused) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Instructing helper to PAUSE input stream.");
        sendCommandToHelper(IPCCommandType::PauseStream);
    } else {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Instructing helper to RESUME input stream.");
        sendCommandToHelper(IPCCommandType::ResumeStream);
    }
}

bool LinuxInput::isInputPaused() const {
    return local_pause_active_;
}


} 
#endif 