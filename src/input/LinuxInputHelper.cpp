#ifndef _WIN32
#include "input/LinuxInputHelper.h"
#include "utils/Logger.h"
#include "network/Message.h"
#include "utils/KeycodeConverter.h"

#include <asio.hpp>
#include <asio/local/stream_protocol.hpp>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <libudev.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <fstream>
#include <filesystem>
#include <linux/input-event-codes.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/stat.h> // For mode constants

namespace LT = LocalTether;

// Define structure for shared memory
struct HelperSharedData {
    pid_t helper_pid;
    char socket_path[256]; // Max path length for socket
    bool ready;            // Flag to indicate data is ready
};

const char* SHM_NAME = "/localtether_shm_helper_info";
static HelperSharedData* g_shared_data = nullptr;
static int g_shm_fd = -1;
static std::string G_ACTUAL_SOCKET_PATH; // Will be determined at runtime

namespace LocalTether::Input {

static std::atomic<bool> g_helper_running = true;
static asio::io_context* g_ipc_io_context_ptr = nullptr;
static asio::local::stream_protocol::socket* g_client_socket_ptr = nullptr;

static std::vector<struct libevdev*> g_evdev_devices;
static std::vector<int> g_evdev_fds;
static struct libevdev_uinput* g_uinput_device = nullptr;
static std::atomic<bool> g_input_stream_paused(false);

void helper_signal_handler(int signum) {
    LT::Utils::Logger::GetInstance().Info("Input Helper: Received signal " + std::to_string(signum) + ". Shutting down.");
    g_helper_running = false;
    if (g_ipc_io_context_ptr) {
        g_ipc_io_context_ptr->stop();
    }
}

bool setup_shared_memory() {
    shm_unlink(SHM_NAME); // Clean up previous instance, if any

    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    fchmod(g_shm_fd,0666);
    if (g_shm_fd == -1) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: shm_open failed: " + std::string(strerror(errno)));
        return false;
    }

    if (ftruncate(g_shm_fd, sizeof(HelperSharedData)) == -1) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: ftruncate failed: " + std::string(strerror(errno)));
        close(g_shm_fd);
        shm_unlink(SHM_NAME);
        return false;
    }

    g_shared_data = (HelperSharedData*)mmap(NULL, sizeof(HelperSharedData), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shared_data == MAP_FAILED) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: mmap failed: " + std::string(strerror(errno)));
        close(g_shm_fd);
        shm_unlink(SHM_NAME);
        return false;
    }

    memset(g_shared_data, 0, sizeof(HelperSharedData)); // Initialize
    LT::Utils::Logger::GetInstance().Info("Input Helper: Shared memory segment " + std::string(SHM_NAME) + " created and mapped.");
    return true;
}

void write_info_to_shared_memory() {
    if (!g_shared_data) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Cannot write to uninitialized shared memory.");
        return;
    }
    g_shared_data->helper_pid = getpid();
    strncpy(g_shared_data->socket_path, G_ACTUAL_SOCKET_PATH.c_str(), sizeof(g_shared_data->socket_path) - 1);
    g_shared_data->socket_path[sizeof(g_shared_data->socket_path) - 1] = '\0'; // Ensure null termination
    g_shared_data->ready = true; // Signal that data is ready

    LT::Utils::Logger::GetInstance().Info("Input Helper: PID " + std::to_string(getpid()) + " and socket path '" + G_ACTUAL_SOCKET_PATH + "' written to shared memory.");
}

void cleanup_shared_memory() {
    if (g_shared_data != nullptr && g_shared_data != MAP_FAILED) {
        munmap(g_shared_data, sizeof(HelperSharedData));
        g_shared_data = nullptr;
    }
    if (g_shm_fd != -1) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }

    if (shm_unlink(SHM_NAME) == -1) {
    
        LT::Utils::Logger::GetInstance().Warning("Input Helper: shm_unlink failed: " + std::string(strerror(errno)));
    } else {
        LT::Utils::Logger::GetInstance().Info("Input Helper: Shared memory segment " + std::string(SHM_NAME) + " unlinked.");
    }
}

void cleanup_helper_resources() {
    LT::Utils::Logger::GetInstance().Info("Input Helper: Cleaning up resources...");
    if (g_client_socket_ptr && g_client_socket_ptr->is_open()) {
        asio::error_code ec;
        g_client_socket_ptr->shutdown(asio::socket_base::shutdown_both, ec);
        g_client_socket_ptr->close(ec);
    }

    if (g_uinput_device) {
        libevdev_uinput_destroy(g_uinput_device);
        g_uinput_device = nullptr;
    }
    for (struct libevdev* dev : g_evdev_devices) {
        int fd = libevdev_get_fd(dev);
        libevdev_free(dev);
        if (fd >= 0) close(fd);
    }
    g_evdev_devices.clear();
    g_evdev_fds.clear();

    if (!G_ACTUAL_SOCKET_PATH.empty()) {
        std::error_code ec_fs;
        std::filesystem::remove(G_ACTUAL_SOCKET_PATH, ec_fs);
        if (ec_fs) {
             LT::Utils::Logger::GetInstance().Warning("Input Helper: Failed to remove socket file " + G_ACTUAL_SOCKET_PATH + ": " + ec_fs.message());
        }
    }
    
    cleanup_shared_memory(); // Cleanup shared memory here
    LT::Utils::Logger::GetInstance().Info("Input Helper: Resources cleaned up.");
}


bool initialize_input_devices() {
    LT::Utils::Logger::GetInstance().Info("Input Helper: Initializing input devices using libudev...");

    struct udev *udev = udev_new();
    if (!udev) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Cannot create udev context.");
        return false;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *dev_list_entry;

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *dev_udev = udev_device_new_from_syspath(udev, path);
        if (!dev_udev) continue;

        const char *devnode = udev_device_get_devnode(dev_udev);
        if (devnode && strncmp(devnode, "/dev/input/event", 16) == 0) {
            const char* prop_kbd = udev_device_get_property_value(dev_udev, "ID_INPUT_KEYBOARD");
            const char* prop_mouse = udev_device_get_property_value(dev_udev, "ID_INPUT_MOUSE");

            bool is_potential_device = false;
            if (prop_kbd && strcmp(prop_kbd, "1") == 0) {
                is_potential_device = true;
            }
            if (prop_mouse && strcmp(prop_mouse, "1") == 0) {
                is_potential_device = true;
            }

            if(is_potential_device) {
                int fd = open(devnode, O_RDONLY | O_NONBLOCK);
                if (fd < 0) {
                    udev_device_unref(dev_udev);
                    continue;
                }

                struct libevdev *ev_dev = libevdev_new();
                if (libevdev_set_fd(ev_dev, fd) < 0) {
                    libevdev_free(ev_dev);
                    close(fd);
                    udev_device_unref(dev_udev);
                    continue;
                }
                
                if ((libevdev_has_event_type(ev_dev, EV_KEY) && libevdev_has_event_code(ev_dev, EV_KEY, KEY_A)) ||
                    (libevdev_has_event_type(ev_dev, EV_REL) && libevdev_has_event_code(ev_dev, EV_REL, REL_X))) {
                    g_evdev_devices.push_back(ev_dev);
                    g_evdev_fds.push_back(fd);
                } else {
                    libevdev_free(ev_dev);
                    close(fd);
                }
            }
        }
        udev_device_unref(dev_udev);
    }
    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    if (g_evdev_devices.empty()) {
        LT::Utils::Logger::GetInstance().Warning("Input Helper: No suitable input devices found.");
    }

    struct libevdev* uinput_base_dev = libevdev_new();
    libevdev_set_name(uinput_base_dev, "LocalTether Virtual Input Device");
    libevdev_enable_event_type(uinput_base_dev, EV_KEY);
    libevdev_enable_event_type(uinput_base_dev, EV_REL);
    libevdev_enable_event_type(uinput_base_dev, EV_SYN);
    for (unsigned int key_code = 0; key_code < KEY_MAX; ++key_code) {
        libevdev_enable_event_code(uinput_base_dev, EV_KEY, key_code, nullptr);
    }
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_X, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_Y, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_WHEEL, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_HWHEEL, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_LEFT, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_RIGHT, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_MIDDLE, nullptr);


    int uinput_err = libevdev_uinput_create_from_device(uinput_base_dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &g_uinput_device);
    libevdev_free(uinput_base_dev);

    if (uinput_err != 0) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to create uinput device: " + std::string(strerror(-uinput_err)));
        g_uinput_device = nullptr;
    }
    return true;
}

void poll_and_send_input_events(asio::local::stream_protocol::socket& client_socket) {
    if (!g_helper_running.load(std::memory_order_relaxed) || g_evdev_fds.empty()) {
        usleep(10000); return;
    }
    fd_set read_fds; FD_ZERO(&read_fds); int max_fd = 0;
    for (int fd : g_evdev_fds) { FD_SET(fd, &read_fds); if (fd > max_fd) max_fd = fd; }
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 20000;
    int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, &tv);
    if (ret <= 0) return;

    LT::Network::InputPayload current_payload; bool events_accumulated = false;
    for (size_t i = 0; i < g_evdev_fds.size(); ++i) {
        if (FD_ISSET(g_evdev_fds[i], &read_fds)) {
            struct libevdev* dev = g_evdev_devices[i]; struct input_event ev; int rc;
            while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) >= 0) {
                if (rc == LIBEVDEV_READ_STATUS_SYNC) continue;
                if (g_input_stream_paused.load(std::memory_order_relaxed)) {
                    if (ev.type == EV_SYN && ev.code == SYN_REPORT) events_accumulated = false; // Reset on pause
                    continue;
                }
                if (ev.type == EV_KEY) {
                    uint8_t vk_code = LT::Utils::KeycodeConverter::evdevToVk(ev.code);
                    if (vk_code != 0) { current_payload.keyEvents.push_back({vk_code, (ev.value == 1 || ev.value == 2)}); events_accumulated = true; }
                } else if (ev.type == EV_REL) {
                    current_payload.isMouseEvent = true;
                    if (ev.code == REL_X) current_payload.deltaX += static_cast<int16_t>(ev.value);
                    else if (ev.code == REL_Y) current_payload.deltaY += static_cast<int16_t>(ev.value);
                    else if (ev.code == REL_WHEEL) current_payload.scrollDeltaY += static_cast<int16_t>(ev.value);
                    else if (ev.code == REL_HWHEEL) current_payload.scrollDeltaX += static_cast<int16_t>(ev.value);
                    events_accumulated = true;
                } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    if (events_accumulated) {
                        std::vector<uint8_t> buffer(sizeof(LT::Network::InputPayload));
                        memcpy(buffer.data(), &current_payload, sizeof(LT::Network::InputPayload));
                        asio::error_code ec_write;
                        asio::write(client_socket, asio::buffer(buffer), ec_write);
                        if (ec_write) { g_helper_running = false; return; }
                        current_payload = LT::Network::InputPayload(); events_accumulated = false;
                    }
                }
            }
        }
    }
}
void simulate_input_event(const LT::Network::InputPayload& payload_to_simulate) {
    if (!g_uinput_device) return;
    for (const auto& keyEvent : payload_to_simulate.keyEvents) {
        uint16_t evdev_code = LT::Utils::KeycodeConverter::vkToEvdev(keyEvent.keyCode);
        if (evdev_code != 0) libevdev_uinput_write_event(g_uinput_device, EV_KEY, evdev_code, keyEvent.isPressed ? 1 : 0);
    }
    if (payload_to_simulate.isMouseEvent) {
        if (payload_to_simulate.deltaX != 0) libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_X, payload_to_simulate.deltaX);
        if (payload_to_simulate.deltaY != 0) libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_Y, payload_to_simulate.deltaY);
        if (payload_to_simulate.scrollDeltaY != 0) libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_WHEEL, payload_to_simulate.scrollDeltaY);
        if (payload_to_simulate.scrollDeltaX != 0) libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_HWHEEL, payload_to_simulate.scrollDeltaX);
    }
    libevdev_uinput_write_event(g_uinput_device, EV_SYN, SYN_REPORT, 0);
}
void handle_ipc_command(const char* data, size_t length) {
    if (length == 0) return; uint8_t command_type = static_cast<uint8_t>(data[0]);
    if (command_type == 1) { // SimulateInput
        if (length -1 >= sizeof(LT::Network::InputPayload)) {
            LT::Network::InputPayload payload_to_simulate;
            memcpy(&payload_to_simulate, data + 1, sizeof(LT::Network::InputPayload));
            simulate_input_event(payload_to_simulate);
        }
    } else if (command_type == 2) { g_input_stream_paused = true; } // Pause
    else if (command_type == 3) { g_input_stream_paused = false; } // Resume
}


int runInputHelperMode(int argc, char ** argv) {
    if(argc < 4){
        LT::Utils::Logger::GetInstance().Error("Input Helper: insufficient arguements");
        return 1;
    }
    uid_t user_uid = static_cast<uid_t>(atoi(argv[2]));
    const char * username = argv[3];

    std::cout<<"now running input helper"<<std::endl;
    LT::Utils::Logger::GetInstance().Info("--- Input Helper Mode Started (PID: " + std::to_string(getpid()) + ") ---");

    if (!setup_shared_memory()) { // Setup shared memory first
        LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to setup shared memory. Exiting.");
        return 1;
    }

    signal(SIGINT, helper_signal_handler);
    signal(SIGTERM, helper_signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE

    if (!initialize_input_devices()) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to initialize input devices.");
        cleanup_helper_resources(); // This will also cleanup shared memory
        return 1;
    }
    
    G_ACTUAL_SOCKET_PATH = "/tmp/localtether_helper_socket_" + std::string(username) +"_" + std::to_string(user_uid) +  "_" + std::to_string(getpid());


    asio::io_context io_context;
    g_ipc_io_context_ptr = &io_context;
    asio::local::stream_protocol::socket client_socket(io_context);
    g_client_socket_ptr = &client_socket;

    std::thread input_polling_thread;

    try {
        std::filesystem::remove(G_ACTUAL_SOCKET_PATH); // Clean up old socket if any
        asio::local::stream_protocol::acceptor acceptor(io_context, asio::local::stream_protocol::endpoint(G_ACTUAL_SOCKET_PATH));
        if(chown(G_ACTUAL_SOCKET_PATH.c_str(), user_uid, -1) == -1){

            LT::Utils::Logger::GetInstance().Error("INput helper: failed to change ownership: " + std::string(strerror(errno)));
        }else{
            LT::Utils::Logger::GetInstance().Info("Input helper: successfully changed owner ship to " +std::to_string(user_uid));
        }

        chmod(G_ACTUAL_SOCKET_PATH.c_str(), 0777); // Make socket accessible

        LT::Utils::Logger::GetInstance().Info("Input Helper: Listening on " + G_ACTUAL_SOCKET_PATH);
        
        // Now that socket is ready, write info to shared memory
        write_info_to_shared_memory();

        acceptor.accept(client_socket);
        LT::Utils::Logger::GetInstance().Info("Input Helper: Main application connected.");
        acceptor.close(); // Stop accepting new connections

        input_polling_thread = std::thread([&client_socket]() {
            LT::Utils::Logger::GetInstance().Info("Input Helper: Input polling thread started.");
            while (g_helper_running.load(std::memory_order_relaxed)) {
                poll_and_send_input_events(client_socket);
            }
            LT::Utils::Logger::GetInstance().Info("Input Helper: Input polling thread finished.");
        });

        std::array<char, 2048> ipc_read_buffer;
        while (g_helper_running.load(std::memory_order_relaxed)) {
            asio::error_code error;
            size_t length = client_socket.read_some(asio::buffer(ipc_read_buffer), error);

            if (error == asio::error::eof || error == asio::error::connection_reset) {
                LT::Utils::Logger::GetInstance().Info("Input Helper: Main app disconnected (EOF/reset).");
                g_helper_running = false; break;
            } else if (error) {
                if (error != asio::error::operation_aborted) {
                     LT::Utils::Logger::GetInstance().Error("Input Helper: IPC read error: " + error.message());
                }
                g_helper_running = false; break;
            }
            handle_ipc_command(ipc_read_buffer.data(), length);
        }
    } catch (const std::exception& e) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Exception: " + std::string(e.what()));
        g_helper_running = false;
    }

    g_helper_running = false; // Ensure flag is set for threads to exit
    if (g_ipc_io_context_ptr && !g_ipc_io_context_ptr->stopped()) {
         g_ipc_io_context_ptr->stop(); // Stop io_context to unblock read_some
    }

    if (input_polling_thread.joinable()) {
        input_polling_thread.join();
    }

    cleanup_helper_resources(); // This will also cleanup shared memory
    LT::Utils::Logger::GetInstance().Info("--- Input Helper Mode Terminated ---");
    return 0;
}

} // namespace LocalTether::Input
#endif // !_WIN32
