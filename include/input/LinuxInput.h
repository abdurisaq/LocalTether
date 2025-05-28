#pragma once
#ifndef _WIN32
#include "input/InputManager.h"
#include "utils/KeycodeConverter.h"
#include "utils/Logger.h"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <asio.hpp>
#include <asio/local/stream_protocol.hpp>
#include <sys/types.h>
#include <unistd.h>   
#include <unordered_set>  

struct HelperSharedData;  

namespace LocalTether::Input {

class LinuxInput : public InputManager {
public:
    LinuxInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight,bool is_host_mode);
    ~LinuxInput() override;

    bool start() override;
    void stop() override;

    std::vector<LocalTether::Network::InputPayload> pollEvents() override;
    void simulateInput(LocalTether::Network::InputPayload payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) override;

    void setInputPaused(bool paused);
    bool isInputPaused() const;
    
    void setPauseKeyCombo(const std::vector<uint8_t>& combo) override;
    std::vector<uint8_t> getPauseKeyCombo() const override;


    bool isHelperConnected() const { 
        return helper_connected_.load(std::memory_order_relaxed);
    }
    bool isInitializationInProgress() const { 
        return m_init_in_progress_.load(std::memory_order_relaxed);
    }

    bool isRunning() const override {
        return running_.load(std::memory_order_relaxed);
    }

private:
    bool is_host_mode_;
    std::thread m_init_thread_; 
    std::atomic<bool> m_init_in_progress_{false}; 
    std::atomic<bool> m_stop_requested_{false}; 

    void helperInitializationRoutine();

    
    bool launchHelperProcess();
    bool connectToHelper();
    void readFromHelperLoop();
    
    enum class IPCCommandType : uint8_t {
        SimulateInput = 1,
        PauseStream = 2,  
        ResumeStream = 3, 
        Shutdown = 4,
        GrabDevices = 5,  
        UngrabDevices = 6  
    };
    void sendCommandToHelper(IPCCommandType cmdType, const std::vector<uint8_t>& data = {});
    void sendPayloadToHelper(IPCCommandType cmdType, const LocalTether::Network::InputPayload& payload);

     
    bool open_and_map_shared_memory();
    void close_and_unmap_shared_memory();
    bool read_info_from_shared_memory(pid_t& out_pid, std::string& out_socket_path);

    void checkAndTogglePauseCombo();

     
    std::unordered_set<uint8_t> m_currently_pressed_vk_codes;  
    bool m_combo_was_active_last_poll = false;  

    std::atomic<bool> running_{false};
    std::atomic<bool> helper_connected_{false};
    std::atomic<bool> local_pause_active_{false}; 
    
    pid_t pkexec_pid_ = -1; 
    pid_t helper_actual_pid_ = -1; 
    std::string actual_helper_socket_path_; 

    const char* shm_name_ = "/localtether_shm_helper_info"; 
    int shm_fd_ = -1;                                      
    HelperSharedData* shared_data_ptr_ = nullptr;          

    asio::io_context ipc_io_context_;
    asio::local::stream_protocol::socket ipc_socket_;
    std::thread ipc_thread_; 
    std::array<char, 2048> ipc_read_buffer_; 

    std::vector<LocalTether::Network::InputPayload> received_payloads_queue_;
    std::mutex queue_mutex_;

    uint16_t clientScreenWidth_;  
    uint16_t clientScreenHeight_; 

     
    static constexpr int MOUSE_DEADZONE_SQUARED = 5 * 5; 
    int32_t m_lastSentHostAbsX = -1;
    int32_t m_lastSentHostAbsY = -1;
     
    float m_lastSentRelativeX = -1.0f;
    float m_lastSentRelativeY = -1.0f;
    uint8_t m_lastSentMouseButtons = 0;
    
    std::string getExecutablePath();
    void cleanupHelperProcess(); 
};
} 
#endif