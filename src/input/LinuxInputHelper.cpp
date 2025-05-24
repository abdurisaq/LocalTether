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

namespace LT = LocalTether;

namespace LocalTether::Input {

static std::atomic<bool> g_helper_running = true;
static asio::io_context* g_ipc_io_context_ptr = nullptr;
static asio::local::stream_protocol::socket* g_client_socket_ptr = nullptr;
static const std::string G_HELPER_SOCKET_PATH = "/tmp/localtether_inputhelper.sock";
static const std::string G_PID_FILE_PATH = "/tmp/localtether_helper.pid";

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

void write_pid_file() {
    std::ofstream pid_file(G_PID_FILE_PATH);
    if (pid_file.is_open()) {
        pid_file << getpid();
        pid_file.close();
        LT::Utils::Logger::GetInstance().Info("Input Helper: PID " + std::to_string(getpid()) + " written to " + G_PID_FILE_PATH);
    } else {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Could not write PID file: " + G_PID_FILE_PATH + " Error: " + strerror(errno));
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

    std::filesystem::remove(G_HELPER_SOCKET_PATH);
    std::filesystem::remove(G_PID_FILE_PATH);
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
                LT::Utils::Logger::GetInstance().Info("Input Helper: Found potential keyboard: " + std::string(devnode));
                is_potential_device = true;
            }
            if (prop_mouse && strcmp(prop_mouse, "1") == 0) {
                 LT::Utils::Logger::GetInstance().Info("Input Helper: Found potential mouse: " + std::string(devnode));
                is_potential_device = true;
            }

            if(is_potential_device) {
                int fd = open(devnode, O_RDONLY | O_NONBLOCK);
                if (fd < 0) {
                    LT::Utils::Logger::GetInstance().Warning("Input Helper: Could not open " + std::string(devnode) + ". Error: " + std::string(strerror(errno)));
                    udev_device_unref(dev_udev);
                    continue;
                }

                struct libevdev *ev_dev = libevdev_new();
                if (libevdev_set_fd(ev_dev, fd) < 0) {
                    LT::Utils::Logger::GetInstance().Warning("Input Helper: libevdev_set_fd failed for " + std::string(devnode) + ". Error: " + std::string(strerror(errno)));
                    libevdev_free(ev_dev);
                    close(fd);
                    udev_device_unref(dev_udev);
                    continue;
                }
                
                if ((libevdev_has_event_type(ev_dev, EV_KEY) && libevdev_has_event_code(ev_dev, EV_KEY, KEY_A)) ||
                    (libevdev_has_event_type(ev_dev, EV_REL) && libevdev_has_event_code(ev_dev, EV_REL, REL_X))) {
                    LT::Utils::Logger::GetInstance().Info("Input Helper: Adding device " + std::string(devnode) + " (" + libevdev_get_name(ev_dev) + ") for monitoring.");
                    g_evdev_devices.push_back(ev_dev);
                    g_evdev_fds.push_back(fd);
                } else {
                    LT::Utils::Logger::GetInstance().Info("Input Helper: Device " + std::string(devnode) + " (" + libevdev_get_name(ev_dev) + ") lacks essential kbd/mouse capabilities, skipping.");
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
        LT::Utils::Logger::GetInstance().Warning("Input Helper: No suitable input devices found to monitor via udev. Input capture may not work.");
    } else {
        LT::Utils::Logger::GetInstance().Info("Input Helper: Will monitor " + std::to_string(g_evdev_devices.size()) + " input devices.");
    }

    // Create uinput device for simulation
    struct libevdev* uinput_base_dev = libevdev_new();
    libevdev_set_name(uinput_base_dev, "LocalTether Virtual Input Device");
    libevdev_enable_event_type(uinput_base_dev, EV_KEY);
    libevdev_enable_event_type(uinput_base_dev, EV_REL);
    libevdev_enable_event_type(uinput_base_dev, EV_SYN);

    // Enable all keys we might want to simulate
    for (uint16_t vk = 0; vk < 256; ++vk) {
        uint16_t evdev_code = LT::Utils::KeycodeConverter::vkToEvdev(static_cast<uint8_t>(vk));
        if (evdev_code != 0 && evdev_code < KEY_MAX) {
            libevdev_enable_event_code(uinput_base_dev, EV_KEY, evdev_code, nullptr);
        }
    }
    
    // Ensure mouse buttons and movement are enabled
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_LEFT, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_RIGHT, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_MIDDLE, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_SIDE, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_KEY, BTN_EXTRA, nullptr);

    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_X, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_Y, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_WHEEL, nullptr);
    libevdev_enable_event_code(uinput_base_dev, EV_REL, REL_HWHEEL, nullptr);

    int uinput_err = libevdev_uinput_create_from_device(uinput_base_dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &g_uinput_device);
    libevdev_free(uinput_base_dev);

    if (uinput_err != 0) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to create uinput device: " + std::string(strerror(-uinput_err)));
        g_uinput_device = nullptr;
    } else {
        LT::Utils::Logger::GetInstance().Info("Input Helper: uinput device created: " + std::string(libevdev_uinput_get_devnode(g_uinput_device)));
    }
    return true;
}

void poll_and_send_input_events(asio::local::stream_protocol::socket& client_socket) {
    if (!g_helper_running.load(std::memory_order_relaxed) || g_evdev_fds.empty()) {
        usleep(10000);
        return;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;

    for (int fd : g_evdev_fds) {
        FD_SET(fd, &read_fds);
        if (fd > max_fd) {
            max_fd = fd;
        }
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 20000; // 20ms timeout

    int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, &tv);

    if (ret < 0) {
        if (errno != EINTR) {
            LT::Utils::Logger::GetInstance().Error("Input Helper: select() error: " + std::string(strerror(errno)));
        }
        return;
    }
    if (ret == 0) {
        return;
    }

    LT::Network::InputPayload current_payload;
    bool events_accumulated = false;

    for (size_t i = 0; i < g_evdev_fds.size(); ++i) {
        if (FD_ISSET(g_evdev_fds[i], &read_fds)) {
            struct libevdev* dev = g_evdev_devices[i];
            struct input_event ev;
            int rc;
            
            while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC) {
                 if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                    LT::Utils::Logger::GetInstance().Warning("Input Helper: Dropped SYN event for " + std::string(libevdev_get_name(dev)));
                    
                    if (events_accumulated) {
                        std::vector<uint8_t> buffer(sizeof(LT::Network::InputPayload));
                        memcpy(buffer.data(), &current_payload, sizeof(LT::Network::InputPayload));
                        asio::error_code ec_write;
                        asio::write(client_socket, asio::buffer(buffer), ec_write);
                        if (ec_write) {
                            LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to send (on sync) input event: " + ec_write.message());
                            g_helper_running = false; return;
                        }
                        current_payload = LT::Network::InputPayload();
                        events_accumulated = false;
                    }
                    continue;
                }

                if (g_input_stream_paused.load(std::memory_order_relaxed)) {
                    if (ev.type == EV_SYN && ev.code == SYN_REPORT && events_accumulated) {
                        current_payload = LT::Network::InputPayload();
                        events_accumulated = false;
                    }
                    continue;
                }

                if (ev.type == EV_KEY) {
                    uint8_t vk_code = LT::Utils::KeycodeConverter::evdevToVk(ev.code);
                    if (vk_code != 0) {
                        current_payload.keyEvents.push_back({vk_code, (ev.value == 1 || ev.value == 2)});
                        events_accumulated = true;
                    }
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
                        if (ec_write) {
                            LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to send input event to main app: " + ec_write.message());
                            g_helper_running = false; return;
                        }
                        current_payload = LT::Network::InputPayload();
                        events_accumulated = false;
                    }
                }
            }

            if (rc < 0 && rc != -EAGAIN) {
                LT::Utils::Logger::GetInstance().Error("Input Helper: libevdev_next_event failed for " + std::string(libevdev_get_name(dev)) + ": " + std::string(strerror(-rc)));
            }
        }
    }
}

void simulate_input_event(const LT::Network::InputPayload& payload_to_simulate) {
    if (!g_uinput_device) {
        LT::Utils::Logger::GetInstance().Warning("Input Helper: uinput device not available for simulation.");
        return;
    }

    for (const auto& keyEvent : payload_to_simulate.keyEvents) {
        uint16_t evdev_code = LT::Utils::KeycodeConverter::vkToEvdev(keyEvent.keyCode);
        if (evdev_code != 0) {
            libevdev_uinput_write_event(g_uinput_device, EV_KEY, evdev_code, keyEvent.isPressed ? 1 : 0);
        } else {
            LT::Utils::Logger::GetInstance().Warning("Input Helper: Unmapped VK_Code for simulation: " + std::to_string(keyEvent.keyCode));
        }
    }

    if (payload_to_simulate.isMouseEvent) {
        if (payload_to_simulate.deltaX != 0) {
            libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_X, payload_to_simulate.deltaX);
        }
        if (payload_to_simulate.deltaY != 0) {
            libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_Y, payload_to_simulate.deltaY);
        }
        if (payload_to_simulate.scrollDeltaY != 0) {
            libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_WHEEL, payload_to_simulate.scrollDeltaY);
        }
        if (payload_to_simulate.scrollDeltaX != 0) {
            libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_HWHEEL, payload_to_simulate.scrollDeltaX);
        }
    }
    libevdev_uinput_write_event(g_uinput_device, EV_SYN, SYN_REPORT, 0);
}

void handle_ipc_command(const char* data, size_t length) {
    if (length == 0) return;
    uint8_t command_type = static_cast<uint8_t>(data[0]);

    if (command_type == 1) { // SimulateInput
        if (length -1 >= sizeof(LT::Network::InputPayload)) {
            LT::Network::InputPayload payload_to_simulate;
            memcpy(&payload_to_simulate, data + 1, sizeof(LT::Network::InputPayload));
            simulate_input_event(payload_to_simulate);
        } else {
             LT::Utils::Logger::GetInstance().Warning("Input Helper: SimulateInput command payload too small. Len: " + std::to_string(length-1) + ", Expected: " + std::to_string(sizeof(LT::Network::InputPayload)));
        }
    } else if (command_type == 2) { // Pause
        g_input_stream_paused = true;
        LT::Utils::Logger::GetInstance().Info("Input Helper: Input stream PAUSED by main app.");
    } else if (command_type == 3) { // Resume
        g_input_stream_paused = false;
        LT::Utils::Logger::GetInstance().Info("Input Helper: Input stream RESUMED by main app.");
    } else {
        LT::Utils::Logger::GetInstance().Warning("Input Helper: Received unknown IPC command type: " + std::to_string(command_type));
    }
}

int runInputHelperMode() {
    LT::Utils::Logger::GetInstance().Info("--- Input Helper Mode Started (PID: " + std::to_string(getpid()) + ") ---");
    write_pid_file();

    signal(SIGINT, helper_signal_handler);
    signal(SIGTERM, helper_signal_handler);
    signal(SIGPIPE, SIG_IGN);

    if (!initialize_input_devices()) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to initialize input devices.");
        cleanup_helper_resources();
        return 1;
    }

    asio::io_context io_context;
    g_ipc_io_context_ptr = &io_context;
    asio::local::stream_protocol::socket client_socket(io_context);
    g_client_socket_ptr = &client_socket;

    std::thread input_polling_thread;

    try {
        std::filesystem::remove(G_HELPER_SOCKET_PATH);
        asio::local::stream_protocol::acceptor acceptor(io_context, asio::local::stream_protocol::endpoint(G_HELPER_SOCKET_PATH));
        LT::Utils::Logger::GetInstance().Info("Input Helper: Listening on " + G_HELPER_SOCKET_PATH);

        acceptor.accept(client_socket);
        LT::Utils::Logger::GetInstance().Info("Input Helper: Main application connected.");
        acceptor.close();

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
                LT::Utils::Logger::GetInstance().Error("Input Helper: IPC read error: " + error.message());
                g_helper_running = false; break;
            }
            handle_ipc_command(ipc_read_buffer.data(), length);
        }
    } catch (const std::exception& e) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Exception: " + std::string(e.what()));
        g_helper_running = false;
    }

    g_helper_running = false;
    if (input_polling_thread.joinable()) {
        input_polling_thread.join();
    }

    cleanup_helper_resources();
    LT::Utils::Logger::GetInstance().Info("--- Input Helper Mode Terminated ---");
    return 0;
}

} 
#endif 