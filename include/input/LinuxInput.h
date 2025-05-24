#pragma once
#ifndef _WIN32
#include "input/InputManager.h"
#include "utils/Logger.h"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <asio.hpp>
#include <asio/local/stream_protocol.hpp>

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


    std::atomic<bool> running_{false};
    std::atomic<bool> helper_connected_{false};
    std::atomic<bool> local_pause_active_{false}; 
    
    pid_t helper_pid_ = -1; // PID of the actual helper process
    std::string helper_socket_path_ = "/tmp/localtether_inputhelper.sock";
    std::string helper_pid_file_path_ = "/tmp/localtether_helper.pid";

    asio::io_context ipc_io_context_;
    asio::local::stream_protocol::socket ipc_socket_;
    std::thread ipc_thread_; 
    std::array<char, 2048> ipc_read_buffer_; 

    std::vector<LocalTether::Network::InputPayload> received_payloads_queue_;
    std::mutex queue_mutex_;

    std::string getExecutablePath();
    bool readHelperPidFile();
    void cleanupHelperProcess();
};
}
#endif