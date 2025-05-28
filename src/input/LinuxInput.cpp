#ifndef _WIN32
#include "input/LinuxInput.h"
#include "network/Message.h"
#include "utils/Logger.h"
#include "utils/Serialization.h"
#include <SDL.h>  

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pwd.h>
#include <optional>

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

void LinuxInput::helperInitializationRoutine() {
    m_init_in_progress_.store(true, std::memory_order_relaxed);
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper initialization routine started.");

    if (clientScreenWidth_ == 0 || clientScreenHeight_ == 0) {
        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Client screen dimensions are zero. Attempting to fetch with SDL.");
        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
             clientScreenWidth_ = dm.w; clientScreenHeight_ = dm.h;
             LT::Utils::Logger::GetInstance().Info("LinuxInput: Fetched screen dimensions: " + std::to_string(clientScreenWidth_) + "x" + std::to_string(clientScreenHeight_));
        } else {
            LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to get SDL display mode. Dimensions remain zero.");
        }
    }

    if (m_stop_requested_.load(std::memory_order_relaxed)) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Stop requested during helper initialization (before launch).");
        m_init_in_progress_.store(false, std::memory_order_relaxed);
        running_.store(false, std::memory_order_relaxed);
        return;
    }

    if (!launchHelperProcess()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to launch helper process.");
        running_.store(false, std::memory_order_relaxed);
        m_init_in_progress_.store(false, std::memory_order_relaxed);
        return;
    }

    if (m_stop_requested_.load(std::memory_order_relaxed)) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Stop requested during helper initialization (after launch, before connect).");
        cleanupHelperProcess();
        m_init_in_progress_.store(false, std::memory_order_relaxed);
        running_.store(false, std::memory_order_relaxed);
        return;
    }

    if (!connectToHelper()) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to helper process.");
        cleanupHelperProcess();
        running_.store(false, std::memory_order_relaxed);
        m_init_in_progress_.store(false, std::memory_order_relaxed);
        return;
    }

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper initialization routine completed successfully.");
    m_init_in_progress_.store(false, std::memory_order_relaxed);
}

std::string LinuxInput::getExecutablePath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        return std::string(result, count);
    }
    LT::Utils::Logger::GetInstance().Error("LinuxInput: Could not determine executable path: " + std::string(strerror(errno)));
    return "";
}

LinuxInput::LinuxInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight,bool is_host_mode)
    : clientScreenWidth_(clientScreenWidth), clientScreenHeight_(clientScreenHeight),
      ipc_socket_(ipc_io_context_), m_lastSentHostAbsX(-1),m_lastSentHostAbsY(-1), is_host_mode_(is_host_mode) { 
    LT::Utils::Logger::GetInstance().Info("LinuxInput initialized for client screen: " + std::to_string(clientScreenWidth_) + "x" + std::to_string(clientScreenHeight_));
}

LinuxInput::~LinuxInput() {
    stop();
}

bool LinuxInput::open_and_map_shared_memory() {
    if (shared_data_ptr_ != nullptr && shared_data_ptr_ != MAP_FAILED) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Shared memory already mapped.");
        return true;
    }

    shm_fd_ = shm_open(shm_name_, O_RDONLY, 0); 
    if (shm_fd_ == -1) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: shm_open failed (may not exist yet): " + std::string(strerror(errno)));
        return false;
    }

    shared_data_ptr_ = (HelperSharedData*)mmap(NULL, sizeof(HelperSharedData), PROT_READ, MAP_SHARED, shm_fd_, 0);  
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
    if (shared_data_ptr_ && shared_data_ptr_ != MAP_FAILED) {
        munmap(shared_data_ptr_, sizeof(HelperSharedData));
        shared_data_ptr_ = nullptr;
    }
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
}

bool LinuxInput::read_info_from_shared_memory(pid_t& out_pid, std::string& out_socket_path) {
    if (!shared_data_ptr_ || shared_data_ptr_ == MAP_FAILED) {
        return false;  
    }
    if (shared_data_ptr_->ready) {
        out_pid = shared_data_ptr_->helper_pid;
        out_socket_path = shared_data_ptr_->socket_path;
        return (out_pid > 0 && !out_socket_path.empty());
    }
    return false;
}

bool LinuxInput::launchHelperProcess() {
    if (open_and_map_shared_memory()) {
        pid_t existing_pid = 0;
        std::string existing_socket_path;
        if (read_info_from_shared_memory(existing_pid, existing_socket_path)) {
            if (existing_pid > 0 && kill(existing_pid, 0) == 0) {  
                LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper process (PID " + std::to_string(existing_pid) + ") already running.");
                helper_actual_pid_ = existing_pid; 
                actual_helper_socket_path_ = existing_socket_path;
     
                return true;
            }
            LT::Utils::Logger::GetInstance().Warning("LinuxInput: Stale SHM data found. Unlinking and proceeding to launch.");
        }
     
        close_and_unmap_shared_memory();
        shm_unlink(shm_name_); 
    }


    std::string exePath = getExecutablePath();
    if (exePath.empty()) return false;

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    const char *username = pw ? pw->pw_name : "unknown_user";
    std::string uid_str = std::to_string(uid);
    std::string screenWidthStr = std::to_string(clientScreenWidth_);
    std::string screenHeightStr = std::to_string(clientScreenHeight_);

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Launching helper: pkexec " + exePath +
                                         " --input-helper-mode " + uid_str + " " + username +
                                         " " + screenWidthStr + " " + screenHeightStr);
    pkexec_pid_ = fork();
    if (pkexec_pid_ == 0) {  
        prctl(PR_SET_PDEATHSIG, SIGHUP);  
        execlp("pkexec", "pkexec", exePath.c_str(), "--input-helper-mode",
               uid_str.c_str(), username, screenWidthStr.c_str(), screenHeightStr.c_str(), (char*)nullptr);
        LT::Utils::Logger::GetInstance().Error("LinuxInput: execlp pkexec failed: " + std::string(strerror(errno)));
        _exit(127);
    } else if (pkexec_pid_ < 0) {
        LT::Utils::Logger::GetInstance().Error("LinuxInput: fork for pkexec failed: " + std::string(strerror(errno)));
        return false;
    }

     
    std::thread([pid = pkexec_pid_]() {
        int status;
        waitpid(pid, &status, 0);
         
    }).detach();
    return true;
}

bool LinuxInput::connectToHelper() {
    LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to connect to input helper.");
    const int max_retries = 40; 
    bool shm_info_read_this_attempt = false;  

    for (int i = 0; i < max_retries; ++i) {
        if (m_stop_requested_.load(std::memory_order_relaxed)) {
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Stop requested during connectToHelper.");
            close_and_unmap_shared_memory();  
            return false;
        }

        if (!shm_info_read_this_attempt) {
            if (!open_and_map_shared_memory()) {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: SHM not available yet for connection. Retrying... (attempt " +
                                                     std::to_string(i + 1) + "/" + std::to_string(max_retries) + ")");
                if (m_stop_requested_.load(std::memory_order_relaxed)) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            if (read_info_from_shared_memory(helper_actual_pid_, actual_helper_socket_path_)) {
                shm_info_read_this_attempt = true;  
                LT::Utils::Logger::GetInstance().Info("LinuxInput: Helper info read from SHM: PID=" +
                                                    std::to_string(helper_actual_pid_) +
                                                    ", Socket=" + actual_helper_socket_path_);
            } else {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: Helper info not ready in SHM. Retrying... (attempt " +
                                                    std::to_string(i + 1) + "/" + std::to_string(max_retries) + ")");
                 
                if (m_stop_requested_.load(std::memory_order_relaxed)) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }
        }

        if (actual_helper_socket_path_.empty()) {
            LT::Utils::Logger::GetInstance().Warning("LinuxInput: Helper socket path from SHM is empty. Retrying SHM read.");
            shm_info_read_this_attempt = false;  
            close_and_unmap_shared_memory();  
            if (m_stop_requested_.load(std::memory_order_relaxed)) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

        LT::Utils::Logger::GetInstance().Info("LinuxInput: Attempting to connect to socket: " + actual_helper_socket_path_);
        try {
            if (ipc_socket_.is_open()) {
                asio::error_code ec_close;
                ipc_socket_.close(ec_close);
            }

            ipc_socket_.connect(asio::local::stream_protocol::endpoint(actual_helper_socket_path_));
            helper_connected_.store(true, std::memory_order_relaxed);
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Connected to input helper (PID: " +
                                                std::to_string(helper_actual_pid_) +
                                                " via socket " + actual_helper_socket_path_ + ").");

            if (!ipc_io_context_.stopped()) {  
                ipc_io_context_.restart();
            } else {
                 ipc_io_context_.reset();  
            }


            if (ipc_thread_.joinable()) {  
                LT::Utils::Logger::GetInstance().Warning("LinuxInput: IPC thread was joinable before new start. Joining.");
                ipc_thread_.join();
            }

            ipc_thread_ = std::thread([this]() {
                LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC thread started.");
                asio::io_context::work work_guard(ipc_io_context_);
                try {
                    ipc_io_context_.run();
                } catch (const std::exception& e) {
                    LT::Utils::Logger::GetInstance().Error("LinuxInput: IPC io_context run error: " + std::string(e.what()));
                    helper_connected_.store(false, std::memory_order_relaxed);  
                }
                LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC thread finished.");
            });

            if(is_host_mode_) {
                LT::Utils::Logger::GetInstance().Info("LinuxInput: Host mode detected. Grabbing devices from helper.");
                sendCommandToHelper(IPCCommandType::GrabDevices);
            } else {
                LT::Utils::Logger::GetInstance().Info("LinuxInput: Client mode detected. Ungrabbing devices from helper.");
            }
            
            readFromHelperLoop();
            close_and_unmap_shared_memory();  
            return true;
        } catch (const std::system_error& e) {
            if (i == max_retries - 1) {
                LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to helper on final attempt: " + std::string(e.what()));
            } else {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: Failed to connect to helper socket (attempt " +
                                                    std::to_string(i + 1) + "/" + std::to_string(max_retries) +
                                                    "): " + e.what() + ". Retrying in 1s...");
            }
             
        }
        if (m_stop_requested_.load(std::memory_order_relaxed)) {
             LT::Utils::Logger::GetInstance().Info("LinuxInput: Stop requested during connectToHelper retry wait.");
             close_and_unmap_shared_memory();
             return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    LT::Utils::Logger::GetInstance().Error("LinuxInput: Failed to connect to input helper after " +
                                         std::to_string(max_retries) + " retries.");
    close_and_unmap_shared_memory(); 
    return false;
}

bool LinuxInput::start() {
    if (running_.load(std::memory_order_relaxed) && helper_connected_.load(std::memory_order_relaxed)) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Already started and connected.");
        return true;
    }

    if (m_init_in_progress_.load(std::memory_order_relaxed)) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Initialization already in progress.");
        return true; 
    }

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Starting asynchronously...");
    m_stop_requested_.store(false, std::memory_order_relaxed); 
    running_.store(true, std::memory_order_relaxed); 

    if (m_init_thread_.joinable()) {
        m_init_thread_.join();
    }
    m_init_thread_ = std::thread(&LinuxInput::helperInitializationRoutine, this);

    return true; 
}

void LinuxInput::cleanupHelperProcess() {
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
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Terminating " + desc_to_kill + " (PID: " + std::to_string(pid_to_kill) + ")");
        kill(pid_to_kill, SIGTERM);  
         
    }
    pkexec_pid_ = -1;
    helper_actual_pid_ = -1;
}

void LinuxInput::stop() {
    if (!running_.load(std::memory_order_relaxed) &&
        !m_init_in_progress_.load(std::memory_order_relaxed) &&
        !helper_connected_.load(std::memory_order_relaxed)) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Stop called but appears to be already stopped or not fully initialized.");
        return;
    }

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopping...");
    m_stop_requested_.store(true, std::memory_order_relaxed); 
    running_.store(false, std::memory_order_relaxed);      

    if (m_init_thread_.joinable()) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Joining initialization thread...");
        m_init_thread_.join();
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Initialization thread joined.");
    }

    m_init_in_progress_.store(false, std::memory_order_relaxed);


    if (helper_connected_.load(std::memory_order_relaxed) && ipc_socket_.is_open()) {
        sendCommandToHelper(IPCCommandType::Shutdown);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        asio::error_code ec;
        ipc_socket_.shutdown(asio::local::stream_protocol::socket::shutdown_both, ec);
        if (ec) { LT::Utils::Logger::GetInstance().Debug("LinuxInput: IPC socket shutdown error: " + ec.message());  }
        ipc_socket_.close(ec);
        if (ec) {  LT::Utils::Logger::GetInstance().Debug("LinuxInput: IPC socket close error: " + ec.message());  }
    }
    helper_connected_.store(false, std::memory_order_relaxed);

    if (!ipc_io_context_.stopped()) {
        ipc_io_context_.stop();
    }

    if (ipc_thread_.joinable()) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: Joining IPC thread...");
        ipc_thread_.join();
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: IPC thread joined.");
    }
    ipc_io_context_.reset();


    cleanupHelperProcess(); 
    close_and_unmap_shared_memory(); 

    LT::Utils::Logger::GetInstance().Info("LinuxInput: Stopped.");
}

void LinuxInput::readFromHelperLoop() {
    if (!helper_connected_ || !ipc_socket_.is_open() || !running_) {
        LT::Utils::Logger::GetInstance().Debug("LinuxInput: readFromHelperLoop preconditions not met. Helper connected: " + std::string(helper_connected_ ? "true":"false") + ", socket open: " + std::string(ipc_socket_.is_open() ? "true":"false") + ", running: " + std::string(running_ ? "true":"false"));
        return;
    }
    ipc_socket_.async_read_some(asio::buffer(ipc_read_buffer_),
        [this](const std::error_code& ec, std::size_t bytes_transferred) {
            if (!running_ || !helper_connected_) {
                LT::Utils::Logger::GetInstance().Debug("LinuxInput: readFromHelperLoop callback: running or helper_connected is false, exiting callback.");
                return;
            }

            if (!ec) {
                if (bytes_transferred > 0) {
                     
                    auto maybePayload = LT::Utils::deserializeInputPayload(
                        reinterpret_cast<const uint8_t*>(ipc_read_buffer_.data()), bytes_transferred);
                    if (maybePayload) {
                         
                        
                        
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        received_payloads_queue_.push_back(*maybePayload);
                    } else {
                        LT::Utils::Logger::GetInstance().Warning("LinuxInput: Failed to deserialize payload from helper.");
                    }
                }else {
                     LT::Utils::Logger::GetInstance().Debug("LinuxInput: Received 0 bytes from helper (no error).");
                }
                readFromHelperLoop();
            } else {
                if (ec != asio::error::operation_aborted && ec != asio::error::eof) {
                    LT::Utils::Logger::GetInstance().Error("LinuxInput: IPC read error: " + ec.message());
                } else {
                    LT::Utils::Logger::GetInstance().Info("LinuxInput: IPC connection closed or aborted.");
                }
                helper_connected_ = false;
            }
        });
}


void LinuxInput::checkAndTogglePauseCombo() {
    if (InputManager::pause_key_combo_.empty()) {
        if (m_combo_was_active_last_poll && InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
            LT::Utils::Logger::GetInstance().Debug("LinuxInput: Pause combo cleared while it was active and system was paused. Unpausing.");
            InputManager::input_globally_paused_.store(false, std::memory_order_relaxed);
            setInputPaused(false);
        }
        m_combo_was_active_last_poll = false;
        return;
    }

    bool combo_currently_held = true;
    for (uint8_t key_vk_code_in_combo : InputManager::pause_key_combo_) {
        bool key_found = false;
        if (key_vk_code_in_combo == VK_CONTROL) {
            if (m_currently_pressed_vk_codes.count(VK_LCONTROL) || m_currently_pressed_vk_codes.count(VK_RCONTROL)) {
                key_found = true;
            }
        } else if (key_vk_code_in_combo == VK_SHIFT) {
            if (m_currently_pressed_vk_codes.count(VK_LSHIFT) || m_currently_pressed_vk_codes.count(VK_RSHIFT)) {
                key_found = true;
            }
        } else if (key_vk_code_in_combo == VK_MENU) { 
            if (m_currently_pressed_vk_codes.count(VK_LMENU) || m_currently_pressed_vk_codes.count(VK_RMENU)) {
                key_found = true;
            }
        } else {
            if (m_currently_pressed_vk_codes.count(key_vk_code_in_combo)) {
                key_found = true;
            }
        }

        if (!key_found) {
            combo_currently_held = false;
            break;
        }
    }

    if (combo_currently_held && !m_combo_was_active_last_poll) {
        bool new_pause_state = !InputManager::input_globally_paused_.load(std::memory_order_relaxed);
        InputManager::input_globally_paused_.store(new_pause_state, std::memory_order_relaxed);
        setInputPaused(new_pause_state);

        if (new_pause_state) {
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Input PAUSED by combo toggle.");
        } else {
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Input RESUMED by combo toggle.");
        }
    }
    m_combo_was_active_last_poll = combo_currently_held;
}

std::vector<LT::Network::InputPayload> LinuxInput::pollEvents() {
    if (!running_.load(std::memory_order_relaxed)) {
        return {};
    }
    std::vector<LT::Network::InputPayload> helper_payloads;

    if (helper_connected_.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!received_payloads_queue_.empty()) {
            helper_payloads.swap(received_payloads_queue_); 
        }
    }

    if (!InputManager::pause_key_combo_.empty()) {
        for (const auto& payload : helper_payloads) {
            for (const auto& keyEvent : payload.keyEvents) {
                if (keyEvent.isPressed) {
                   
                    if (m_currently_pressed_vk_codes.find(keyEvent.keyCode) == m_currently_pressed_vk_codes.end()) {
                        m_currently_pressed_vk_codes.insert(keyEvent.keyCode);
                    }
                } else {
                    m_currently_pressed_vk_codes.erase(keyEvent.keyCode);
                }
            }
        }
        checkAndTogglePauseCombo();
    } else {
       
        if (m_combo_was_active_last_poll && InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
            InputManager::input_globally_paused_.store(false, std::memory_order_relaxed);
            setInputPaused(false); 
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Pause combo cleared, input resumed.");
        }
        m_combo_was_active_last_poll = false;
    }

    if (InputManager::isInputGloballyPaused()) {
        
        return {}; 
    }

    return helper_payloads;
}


void LinuxInput::sendCommandToHelper(IPCCommandType cmdType, const std::vector<uint8_t>& data) {
    if (!helper_connected_ || !ipc_socket_.is_open() || !running_) {
        return;
    }
    auto message = std::make_shared<std::vector<uint8_t>>();
    message->push_back(static_cast<uint8_t>(cmdType));
    message->insert(message->end(), data.begin(), data.end());

    asio::async_write(ipc_socket_, asio::buffer(*message),
        [this, cmdType, message](const std::error_code& ec, std::size_t ) {
            if (ec && ec != asio::error::operation_aborted) {
                LT::Utils::Logger::GetInstance().Error("LinuxInput: IPC Command Write Error (" +
                    std::to_string(static_cast<int>(cmdType)) + "): " + ec.message());
                helper_connected_ = false; 
            }
        });
}

void LinuxInput::sendPayloadToHelper(IPCCommandType cmdType, const LT::Network::InputPayload& payload) {
    if (cmdType != IPCCommandType::SimulateInput) return;  
    std::vector<uint8_t> serialized_payload = LT::Utils::serializeInputPayload(payload);
    sendCommandToHelper(cmdType, serialized_payload);
}

void LinuxInput::simulateInput( LT::Network::InputPayload payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) {
    if (!running_ || !helper_connected_) {
        return;
    }
    
    LT::Network::InputPayload payloadForHelper = payload; 
    
    sendPayloadToHelper(IPCCommandType::SimulateInput, payloadForHelper);
}

void LinuxInput::setInputPaused(bool paused) {
    if (local_pause_active_.load(std::memory_order_relaxed) == paused) {
        return; 
    }

    
    local_pause_active_.store(paused, std::memory_order_relaxed);
    
    if(is_host_mode_){
        if (paused) {
            sendCommandToHelper(IPCCommandType::UngrabDevices);
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Input processing PAUSED. Commanding helper.");
        } else {
            sendCommandToHelper(IPCCommandType::GrabDevices);
            LT::Utils::Logger::GetInstance().Info("LinuxInput: Input processing RESUMED. Commanding helper.");
        }
    }
}

bool LinuxInput::isInputPaused() const {
    return local_pause_active_.load(std::memory_order_relaxed);
}

void LinuxInput::setPauseKeyCombo(const std::vector<uint8_t>& combo) {
    InputManager::pause_key_combo_ = combo; 
    m_currently_pressed_vk_codes.clear();  
    
    bool old_combo_was_active = m_combo_was_active_last_poll;
    m_combo_was_active_last_poll = false;  

    if (!combo.empty()) {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Pause key combo set.");
    } else {
        LT::Utils::Logger::GetInstance().Info("LinuxInput: Pause key combo cleared.");
         
        if (InputManager::input_globally_paused_.load(std::memory_order_relaxed) && old_combo_was_active) {
            LT::Utils::Logger::GetInstance().Debug("LinuxInput: Pause combo cleared while it was active, unpausing global state.");
            InputManager::input_globally_paused_.store(false, std::memory_order_relaxed);
            setInputPaused(false);  
        }
    }
}
std::vector<uint8_t> LinuxInput::getPauseKeyCombo() const {
    return InputManager::pause_key_combo_;
}

} 
#endif  