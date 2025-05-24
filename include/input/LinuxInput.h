#pragma once
#ifndef _WIN32
#include "input/InputManager.h"
#include "utils/Logger.h" // Assuming Logger.h is correctly included for LT::Utils::Logger
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <asio.hpp>
#include <asio/local/stream_protocol.hpp>
#include <sys/types.h> // For pid_t
#include <unistd.h>   

struct HelperSharedData;

namespace LocalTether::Input {

class LinuxInput : public InputManager {
public:
    LinuxInput();
    ~LinuxInput() override;

    bool start() override;
    void stop() override;

    std::vector<LocalTether::Network::InputPayload> pollEvents() override;
    void simulateInput(const LocalTether::Network::InputPayload& payload) override;

    void setInputPaused(bool paused);
    bool isInputPaused() const;

private:
    bool launchHelperProcess();
    bool connectToHelper();
    void readFromHelperLoop();
    
    enum class IPCCommandType : uint8_t {
        SimulateInput = 1,
        PauseStream = 2,
        ResumeStream = 3,
        Shutdown = 4
    };
    void sendCommandToHelper(IPCCommandType cmdType, const std::vector<uint8_t>& data = {});
    void sendPayloadToHelper(IPCCommandType cmdType, const LocalTether::Network::InputPayload& payload);

    // Shared memory related methods
    bool open_and_map_shared_memory();
    void close_and_unmap_shared_memory();
    bool read_info_from_shared_memory(pid_t& out_pid, std::string& out_socket_path);

    std::atomic<bool> running_{false};
    std::atomic<bool> helper_connected_{false};
    std::atomic<bool> local_pause_active_{false}; 
    
    pid_t pkexec_pid_ = -1; // PID of the pkexec process itself
    pid_t helper_actual_pid_ = -1; 
    std::string actual_helper_socket_path_; 


    const char* shm_name_ = "/localtether_shm_helper_info"; 
    int shm_fd_ = -1;                                       // File descriptor for shared memory
    HelperSharedData* shared_data_ptr_ = nullptr;          

    asio::io_context ipc_io_context_;
    asio::local::stream_protocol::socket ipc_socket_;
    std::thread ipc_thread_; 
    std::array<char, 2048> ipc_read_buffer_; 

    std::vector<LocalTether::Network::InputPayload> received_payloads_queue_;
    std::mutex queue_mutex_;

    std::string getExecutablePath();
    void cleanupHelperProcess(); 
};
} 
#endif 