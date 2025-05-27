#ifndef _WIN32
#include "input/LinuxInputHelper.h"
#include "utils/Logger.h"
#include "network/Message.h"
#include "utils/KeycodeConverter.h"
#include "utils/Serialization.h"
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
#include <sys/stat.h>
#include <pwd.h>
#include <map>
#include <optional>
#include <algorithm>
#include <array>

namespace LT = LocalTether;

struct HelperSharedData {
    pid_t helper_pid;
    char socket_path[256];
    bool ready;
};

enum class IPCCommandType : uint8_t {
    SimulateInput = 1,
    PauseStream = 2,
    ResumeStream = 3,
    Shutdown = 4
};

const char* SHM_NAME = "/localtether_shm_helper_info";
static HelperSharedData* g_shared_data_ptr = nullptr;
static int g_shm_fd = -1;

namespace LocalTether::Input {

static std::atomic<bool> g_helper_running = true;
static asio::io_context* g_ipc_io_context_ptr = nullptr;
static asio::local::stream_protocol::socket* g_main_app_socket_ptr = nullptr;

static std::vector<struct libevdev*> g_evdev_devices;
static std::vector<int> g_evdev_fds;
static struct libevdev_uinput* g_uinput_device = nullptr;

static int g_client_screen_width = 0;
static int g_client_screen_height = 0;
static std::string G_ACTUAL_SOCKET_PATH;

static int32_t g_helper_abs_x = 0;
static int32_t g_helper_abs_y = 0;
static uint8_t g_helper_mouse_buttons_state = 0;
static bool g_last_processed_abs_move_was_trackpad = false;

static int32_t g_helper_last_sent_abs_x = 0;
static int32_t g_helper_last_sent_abs_y = 0;
static uint8_t g_helper_last_sent_mouse_buttons = 0;

static bool g_helper_mouse_state_initialized = false;

static std::map<int, struct input_absinfo> g_abs_x_info_map;
static std::map<int, struct input_absinfo> g_abs_y_info_map;

static const int HELPER_MOUSE_DEADZONE_SQUARED = 2 * 2;

static std::map<int, bool> g_device_is_touch_pointer;
static std::map<int, bool> g_device_is_part_of_touchpad_system;
static std::map<int, bool> g_device_touch_is_active;
static std::map<int, std::optional<std::pair<int32_t, int32_t>>> g_device_initial_raw_abs_at_touch_start;
static std::map<int, std::optional<std::pair<int32_t, int32_t>>> g_device_screen_coords_at_touch_start;
static std::map<int, std::optional<int32_t>> g_pending_abs_x_for_fd;
static std::map<int, std::optional<int32_t>> g_pending_abs_y_for_fd;

static constexpr size_t VK_KEY_STATE_ARRAY_SIZE = (256 / 8);
static std::array<uint8_t, VK_KEY_STATE_ARRAY_SIZE> g_helper_vk_key_states_bitmask;

static std::atomic<float> g_h_lastSimulatedRelativeX{-1.0f};
static std::atomic<float> g_h_lastSimulatedRelativeY{-1.0f};
static std::atomic<float> g_h_anchorDeviceRelativeX{-1.0f};
static std::atomic<float> g_h_anchorDeviceRelativeY{-1.0f};

static constexpr float G_H_SIMULATION_JUMP_THRESHOLD = 0.02f;

static void helper_reset_simulation_state() {
    g_h_lastSimulatedRelativeX.store(-1.0f);
    g_h_lastSimulatedRelativeY.store(-1.0f);
    g_h_anchorDeviceRelativeX.store(-1.0f);
    g_h_anchorDeviceRelativeY.store(-1.0f);
    LT::Utils::Logger::GetInstance().Debug("LinuxInputHelper: Simulation state reset.");
}

static void helper_process_simulated_mouse_coordinates(float payloadX, float payloadY, LT::Network::InputSourceDeviceType sourceDevice, float& outSimX, float& outSimY) {
    float lastSimX_val = g_h_lastSimulatedRelativeX.load(std::memory_order_relaxed);
    float lastSimY_val = g_h_lastSimulatedRelativeY.load(std::memory_order_relaxed);
    float anchorDevX_val = g_h_anchorDeviceRelativeX.load(std::memory_order_relaxed);
    float anchorDevY_val = g_h_anchorDeviceRelativeY.load(std::memory_order_relaxed);

    if (payloadX < 0.0f || payloadY < 0.0f) {
        outSimX = (lastSimX_val >= 0.0f) ? lastSimX_val : 0.5f;
        outSimY = (lastSimY_val >= 0.0f) ? lastSimY_val : 0.5f;
        return;
    }

    if (sourceDevice == LT::Network::InputSourceDeviceType::TRACKPAD_ABSOLUTE) {
        if (lastSimX_val < 0.0f || anchorDevX_val < 0.0f) {
            outSimX = payloadX;
            outSimY = payloadY;
            g_h_anchorDeviceRelativeX.store(payloadX, std::memory_order_relaxed);
            g_h_anchorDeviceRelativeY.store(payloadY, std::memory_order_relaxed);
        } else {
            float deltaPayloadToAnchorX = payloadX - anchorDevX_val;
            float deltaPayloadToAnchorY = payloadY - anchorDevY_val;
            float distSqPayloadToAnchor = (deltaPayloadToAnchorX * deltaPayloadToAnchorX) +
                                          (deltaPayloadToAnchorY * deltaPayloadToAnchorY);
            float thresholdSq = G_H_SIMULATION_JUMP_THRESHOLD * G_H_SIMULATION_JUMP_THRESHOLD;

            if (distSqPayloadToAnchor > thresholdSq) {
                outSimX = lastSimX_val;
                outSimY = lastSimY_val;
                g_h_anchorDeviceRelativeX.store(payloadX, std::memory_order_relaxed);
                g_h_anchorDeviceRelativeY.store(payloadY, std::memory_order_relaxed);
            } else {
                outSimX = lastSimX_val + deltaPayloadToAnchorX;
                outSimY = lastSimY_val + deltaPayloadToAnchorY;
                g_h_anchorDeviceRelativeX.store(payloadX, std::memory_order_relaxed);
                g_h_anchorDeviceRelativeY.store(payloadY, std::memory_order_relaxed);
            }
        }
    } else {
        outSimX = payloadX;
        outSimY = payloadY;
        if (anchorDevX_val >= 0.0f) {
             g_h_anchorDeviceRelativeX.store(-1.0f, std::memory_order_relaxed);
             g_h_anchorDeviceRelativeY.store(-1.0f, std::memory_order_relaxed);
        }
    }

    outSimX = std::max(0.0f, std::min(1.0f, outSimX));
    outSimY = std::max(0.0f, std::min(1.0f, outSimY));

    g_h_lastSimulatedRelativeX.store(outSimX, std::memory_order_relaxed);
    g_h_lastSimulatedRelativeY.store(outSimY, std::memory_order_relaxed);
}

static void update_helper_vk_key_state(uint8_t vk_code, bool pressed) {
    if (vk_code == 0) return;
    size_t byte_index = vk_code / 8;
    size_t bit_index = vk_code % 8;
    if (byte_index < VK_KEY_STATE_ARRAY_SIZE) {
        if (pressed) {
            g_helper_vk_key_states_bitmask[byte_index] |= (1 << bit_index);
        } else {
            g_helper_vk_key_states_bitmask[byte_index] &= ~(1 << bit_index);
        }
    }
}

static bool is_helper_vk_key_pressed(uint8_t vk_code) {
    if (vk_code == 0) return false;
    size_t byte_index = vk_code / 8;
    size_t bit_index = vk_code % 8;
    if (byte_index < VK_KEY_STATE_ARRAY_SIZE) {
        return (g_helper_vk_key_states_bitmask[byte_index] & (1 << bit_index)) != 0;
    }
    return false;
}

static inline int32_t scale_abs_value_to_screen(int32_t value, const struct input_absinfo* absinfo, int32_t screen_dim) {
    if (!absinfo || absinfo->maximum == absinfo->minimum || screen_dim <= 0) {
        return screen_dim / 2;
    }
    value = std::max(absinfo->minimum, std::min(value, absinfo->maximum));
    double ratio = static_cast<double>(value - absinfo->minimum) / (absinfo->maximum - absinfo->minimum);
    return static_cast<int32_t>(ratio * (screen_dim - 1));
}

void helper_signal_handler(int signum) {
    LT::Utils::Logger::GetInstance().Info("Input Helper: Signal " + std::to_string(signum) + " received. Shutting down.");
    g_helper_running = false;
    if (g_ipc_io_context_ptr && !g_ipc_io_context_ptr->stopped()) {
        g_ipc_io_context_ptr->stop();
    }
}

void cleanup_shared_memory() {
    if (g_shared_data_ptr && g_shared_data_ptr != MAP_FAILED) {
        munmap(g_shared_data_ptr, sizeof(HelperSharedData));
        g_shared_data_ptr = nullptr;
    }
    if (g_shm_fd != -1) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
    if (shm_unlink(SHM_NAME) == -1 && errno != ENOENT) {
        LT::Utils::Logger::GetInstance().Warning("Input Helper: shm_unlink failed: " + std::string(strerror(errno)));
    }
}

bool setup_shared_memory() {
    shm_unlink(SHM_NAME);

    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_shm_fd == -1) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: shm_open failed: " + std::string(strerror(errno)));
        return false;
    }
    if (ftruncate(g_shm_fd, sizeof(HelperSharedData)) == -1) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: ftruncate failed: " + std::string(strerror(errno)));
        close(g_shm_fd); shm_unlink(SHM_NAME); return false;
    }
    g_shared_data_ptr = (HelperSharedData*)mmap(NULL, sizeof(HelperSharedData), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shared_data_ptr == MAP_FAILED) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: mmap failed: " + std::string(strerror(errno)));
        close(g_shm_fd); shm_unlink(SHM_NAME); return false;
    }
    memset(g_shared_data_ptr, 0, sizeof(HelperSharedData));
    g_shared_data_ptr->ready = false;
    return true;
}

void write_info_to_shared_memory() {
    if (!g_shared_data_ptr || g_shared_data_ptr == MAP_FAILED) return;
    g_shared_data_ptr->helper_pid = getpid();
    strncpy(g_shared_data_ptr->socket_path, G_ACTUAL_SOCKET_PATH.c_str(), sizeof(g_shared_data_ptr->socket_path) - 1);
    g_shared_data_ptr->socket_path[sizeof(g_shared_data_ptr->socket_path) - 1] = '\0';
    g_shared_data_ptr->ready = true;
    LT::Utils::Logger::GetInstance().Info("Input Helper: PID " + std::to_string(getpid()) + " and socket '" + G_ACTUAL_SOCKET_PATH + "' written to SHM.");
}

void cleanup_helper_resources() {
    LT::Utils::Logger::GetInstance().Info("Input Helper: Cleaning up resources...");
    if (g_main_app_socket_ptr && g_main_app_socket_ptr->is_open()) {
        asio::error_code ec;
        g_main_app_socket_ptr->shutdown(asio::local::stream_protocol::socket::shutdown_both, ec);
        g_main_app_socket_ptr->close(ec);
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
        if (ec_fs && ec_fs.value() != ENOENT) {
             LT::Utils::Logger::GetInstance().Warning("Input Helper: Failed to remove socket file " + G_ACTUAL_SOCKET_PATH + ": " + ec_fs.message());
        }
    }
    cleanup_shared_memory();
    LT::Utils::Logger::GetInstance().Info("Input Helper: Resources cleaned up.");
}

bool initialize_input_devices() {
    struct udev *udev = udev_new();
    if (!udev) { LT::Utils::Logger::GetInstance().Error("Input Helper: udev_new failed."); return false; }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *dev_list_entry;

    g_helper_vk_key_states_bitmask.fill(0);
    g_abs_x_info_map.clear();
    g_abs_y_info_map.clear();
    g_device_is_touch_pointer.clear();
    g_device_is_part_of_touchpad_system.clear();
    g_device_touch_is_active.clear();
    g_device_initial_raw_abs_at_touch_start.clear();
    g_device_screen_coords_at_touch_start.clear();
    g_pending_abs_x_for_fd.clear();
    g_pending_abs_y_for_fd.clear();

    if (g_client_screen_width > 0 && g_client_screen_height > 0) {
        g_helper_abs_x = g_client_screen_width / 2;
        g_helper_abs_y = g_client_screen_height / 2;
        g_helper_last_sent_abs_x = g_helper_abs_x;
        g_helper_last_sent_abs_y = g_helper_abs_y;
        g_helper_mouse_state_initialized = true;
    } else {
        g_helper_mouse_state_initialized = false;
    }

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *dev_udev = udev_device_new_from_syspath(udev, path);
        if (!dev_udev) continue;

        const char *devnode = udev_device_get_devnode(dev_udev);
        if (!devnode || strncmp(devnode, "/dev/input/event", 16) != 0) {
            udev_device_unref(dev_udev); continue;
        }

        bool is_relevant_for_polling = false;
        const char* id_keyboard = udev_device_get_property_value(dev_udev, "ID_INPUT_KEYBOARD");
        const char* id_mouse = udev_device_get_property_value(dev_udev, "ID_INPUT_MOUSE");
        const char* id_touchpad_prop_val = udev_device_get_property_value(dev_udev, "ID_INPUT_TOUCHPAD");
        const char* id_input = udev_device_get_property_value(dev_udev, "ID_INPUT");

        if ((id_keyboard && strcmp(id_keyboard, "1") == 0) ||
            (id_mouse && strcmp(id_mouse, "1") == 0) ||
            (id_touchpad_prop_val && strcmp(id_touchpad_prop_val, "1") == 0) ||
            (id_input && strcmp(id_input, "1") == 0)
            ) {
            is_relevant_for_polling = true;
        }

        if (is_relevant_for_polling) {
            int fd = open(devnode, O_RDONLY | O_NONBLOCK);
            if (fd < 0) { udev_device_unref(dev_udev); continue; }

            struct libevdev *ev_dev = libevdev_new();
            if (libevdev_set_fd(ev_dev, fd) < 0) {
                libevdev_free(ev_dev); close(fd); udev_device_unref(dev_udev); continue;
            }

            bool has_keys = libevdev_has_event_type(ev_dev, EV_KEY);
            bool has_rel_motion = libevdev_has_event_type(ev_dev, EV_REL) &&
                                  (libevdev_has_event_code(ev_dev, EV_REL, REL_X) || libevdev_has_event_code(ev_dev, EV_REL, REL_Y));
            bool has_abs_motion = libevdev_has_event_type(ev_dev, EV_ABS) &&
                                  (libevdev_has_event_code(ev_dev, EV_ABS, ABS_X) || libevdev_has_event_code(ev_dev, EV_ABS, ABS_Y) ||
                                   libevdev_has_event_code(ev_dev, EV_ABS, ABS_MT_POSITION_X) || libevdev_has_event_code(ev_dev, EV_ABS, ABS_MT_POSITION_Y));
            bool has_scroll = libevdev_has_event_type(ev_dev, EV_REL) &&
                              (libevdev_has_event_code(ev_dev, EV_REL, REL_WHEEL) || libevdev_has_event_code(ev_dev, EV_REL, REL_HWHEEL));

            if (has_keys || has_rel_motion || has_abs_motion || has_scroll) {
                g_evdev_devices.push_back(ev_dev);
                g_evdev_fds.push_back(fd);
                LT::Utils::Logger::GetInstance().Info("Input Helper: Polling device: " + std::string(devnode) + " (" + libevdev_get_name(ev_dev) + ")");

                if (libevdev_has_event_code(ev_dev, EV_ABS, ABS_X)) {
                    const struct input_absinfo *absinfo = libevdev_get_abs_info(ev_dev, ABS_X);
                    if (absinfo) g_abs_x_info_map[fd] = *absinfo;
                }
                if (libevdev_has_event_code(ev_dev, EV_ABS, ABS_Y)) {
                    const struct input_absinfo *absinfo = libevdev_get_abs_info(ev_dev, ABS_Y);
                    if (absinfo) g_abs_y_info_map[fd] = *absinfo;
                }
                if (libevdev_has_event_code(ev_dev, EV_ABS, ABS_MT_POSITION_X) && g_abs_x_info_map.find(fd) == g_abs_x_info_map.end()) {
                    const struct input_absinfo *absinfo = libevdev_get_abs_info(ev_dev, ABS_MT_POSITION_X);
                    if (absinfo) g_abs_x_info_map[fd] = *absinfo;
                }
                if (libevdev_has_event_code(ev_dev, EV_ABS, ABS_MT_POSITION_Y) && g_abs_y_info_map.find(fd) == g_abs_y_info_map.end()) {
                    const struct input_absinfo *absinfo = libevdev_get_abs_info(ev_dev, ABS_MT_POSITION_Y);
                    if (absinfo) g_abs_y_info_map[fd] = *absinfo;
                }

                bool is_udev_touchpad = (id_touchpad_prop_val && strcmp(id_touchpad_prop_val, "1") == 0);
                bool dev_has_abs_xy_for_surface = (g_abs_x_info_map.count(fd) && g_abs_y_info_map.count(fd));
                bool dev_has_btn_touch_for_surface = libevdev_has_event_code(ev_dev, EV_KEY, BTN_TOUCH);

                if (dev_has_abs_xy_for_surface && dev_has_btn_touch_for_surface) {
                    g_device_is_touch_pointer[fd] = true;
                    g_device_is_part_of_touchpad_system[fd] = true;
                    LT::Utils::Logger::GetInstance().Debug("Input Helper: Device " + std::string(devnode) + " registered as a touch pointer surface.");
                } else {
                    g_device_is_touch_pointer[fd] = false;
                    if (is_udev_touchpad) {
                        g_device_is_part_of_touchpad_system[fd] = true;
                        LT::Utils::Logger::GetInstance().Debug("Input Helper: Device " + std::string(devnode) + " identified as part of touchpad system by udev.");
                    } else {
                        g_device_is_part_of_touchpad_system[fd] = false;
                    }
                }
                g_device_touch_is_active[fd] = false;
            } else {
                libevdev_free(ev_dev); close(fd);
            }
        }
        udev_device_unref(dev_udev);
    }
    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    struct libevdev* uinput_template_dev = libevdev_new();
    if (!uinput_template_dev) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: libevdev_new failed for uinput_template_dev.");
        return false;
    }
    libevdev_set_name(uinput_template_dev, "LocalTether Virtual Input");

    libevdev_enable_event_type(uinput_template_dev, EV_SYN);
    libevdev_enable_event_code(uinput_template_dev, EV_SYN, SYN_REPORT, nullptr);

    libevdev_enable_event_type(uinput_template_dev, EV_KEY);
    for (unsigned int key = KEY_ESC; key <= KEY_KPDOT; ++key) {
        libevdev_enable_event_code(uinput_template_dev, EV_KEY, key, nullptr);
    }
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_LEFTSHIFT, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_RIGHTSHIFT, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_LEFTCTRL, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_RIGHTCTRL, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_LEFTALT, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_RIGHTALT, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_LEFTMETA, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, KEY_RIGHTMETA, nullptr);

    libevdev_enable_event_code(uinput_template_dev, EV_KEY, BTN_LEFT, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, BTN_RIGHT, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, BTN_MIDDLE, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, BTN_SIDE, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_KEY, BTN_EXTRA, nullptr);

    libevdev_enable_event_type(uinput_template_dev, EV_REL);
    libevdev_enable_event_code(uinput_template_dev, EV_REL, REL_X, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_REL, REL_Y, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_REL, REL_WHEEL, nullptr);
    libevdev_enable_event_code(uinput_template_dev, EV_REL, REL_HWHEEL, nullptr);

    libevdev_enable_event_type(uinput_template_dev, EV_ABS);
    struct input_absinfo abs_info_x_virt = {0}, abs_info_y_virt = {0};
    abs_info_x_virt.minimum = 0;
    abs_info_x_virt.maximum = g_client_screen_width > 0 ? g_client_screen_width - 1 : 1919;
    libevdev_enable_event_code(uinput_template_dev, EV_ABS, ABS_X, &abs_info_x_virt);

    abs_info_y_virt.minimum = 0;
    abs_info_y_virt.maximum = g_client_screen_height > 0 ? g_client_screen_height - 1 : 1079;
    libevdev_enable_event_code(uinput_template_dev, EV_ABS, ABS_Y, &abs_info_y_virt);

    libevdev_enable_property(uinput_template_dev, INPUT_PROP_POINTER);

    int uinput_err = libevdev_uinput_create_from_device(uinput_template_dev,
                                                      LIBEVDEV_UINPUT_OPEN_MANAGED, &g_uinput_device);
    libevdev_free(uinput_template_dev);
    if (uinput_err != 0) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Failed to create uinput device: " + std::string(strerror(-uinput_err)));
        return false;
    }
    LT::Utils::Logger::GetInstance().Info("Input Helper: uinput device created.");
    return true;
}

void poll_events_once_and_send(asio::local::stream_protocol::socket& target_socket) {
    if (g_evdev_fds.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return;
    }

    fd_set read_fds; FD_ZERO(&read_fds); int max_fd = 0;
    for (int fd : g_evdev_fds) { FD_SET(fd, &read_fds); if (fd > max_fd) max_fd = fd; }
    struct timeval tv = {0, 20000};

    int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, &tv);
    if (ret <= 0) return;

    LT::Network::InputPayload current_payload;
    bool events_accumulated = false;
    bool raw_mouse_moved_this_cycle = false;
    bool raw_mouse_button_changed_this_cycle = false;

    if (!g_helper_mouse_state_initialized && g_client_screen_width > 0 && g_client_screen_height > 0) {
        g_helper_abs_x = g_client_screen_width / 2;
        g_helper_abs_y = g_client_screen_height / 2;
        g_helper_last_sent_abs_x = g_helper_abs_x;
        g_helper_last_sent_abs_y = g_helper_abs_y;
        g_helper_mouse_state_initialized = true;
    }

    for (size_t i = 0; i < g_evdev_fds.size(); ++i) {
        if (FD_ISSET(g_evdev_fds[i], &read_fds)) {
            struct input_event ev;
            int rc;
            int current_fd = g_evdev_fds[i];

            g_pending_abs_x_for_fd[current_fd] = std::nullopt;
            g_pending_abs_y_for_fd[current_fd] = std::nullopt;

            while ((rc = libevdev_next_event(g_evdev_devices[i], LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {
                events_accumulated = true;

                if (ev.type == EV_KEY) {
                    uint8_t vk_code = LT::Utils::KeycodeConverter::evdevToVk(ev.code);
                    bool is_touch_pointer_dev = g_device_is_touch_pointer.count(current_fd) && g_device_is_touch_pointer[current_fd];
                    bool event_is_pressed_state = (ev.value == 1 || ev.value == 2);

                    if (is_touch_pointer_dev && ev.code == BTN_TOUCH) {
                        if (event_is_pressed_state) {
                            g_device_touch_is_active[current_fd] = true;
                            g_device_initial_raw_abs_at_touch_start[current_fd] = std::nullopt;
                            g_device_screen_coords_at_touch_start[current_fd] = {g_helper_abs_x, g_helper_abs_y};
                        } else {
                            g_device_touch_is_active[current_fd] = false;
                        }
                    } else if (vk_code != 0) {
                        bool currently_pressed_in_helper_state = is_helper_vk_key_pressed(vk_code);
                        if (event_is_pressed_state && !currently_pressed_in_helper_state) {
                            current_payload.keyEvents.push_back({vk_code, true});
                            update_helper_vk_key_state(vk_code, true);
                        } else if (!event_is_pressed_state && currently_pressed_in_helper_state) {
                            current_payload.keyEvents.push_back({vk_code, false});
                            update_helper_vk_key_state(vk_code, false);
                        }

                        uint8_t old_buttons = g_helper_mouse_buttons_state;
                        if (ev.code == BTN_LEFT) { if (event_is_pressed_state) g_helper_mouse_buttons_state |= 0x01; else g_helper_mouse_buttons_state &= ~0x01; }
                        else if (ev.code == BTN_RIGHT) { if (event_is_pressed_state) g_helper_mouse_buttons_state |= 0x02; else g_helper_mouse_buttons_state &= ~0x02; }
                        else if (ev.code == BTN_MIDDLE) { if (event_is_pressed_state) g_helper_mouse_buttons_state |= 0x04; else g_helper_mouse_buttons_state &= ~0x04; }
                        if (old_buttons != g_helper_mouse_buttons_state) {
                            raw_mouse_button_changed_this_cycle = true;
                        }
                    }
                } else if (ev.type == EV_REL) {
                    if (g_helper_mouse_state_initialized) {
                        if (ev.code == REL_X) {
                            g_helper_abs_x += ev.value;
                            raw_mouse_moved_this_cycle = true;
                            g_last_processed_abs_move_was_trackpad = false;
                        }
                        else if (ev.code == REL_Y) {
                            g_helper_abs_y += ev.value;
                            raw_mouse_moved_this_cycle = true;
                            g_last_processed_abs_move_was_trackpad = false;
                         }
                    }
                    if (ev.code == REL_WHEEL) { current_payload.scrollDeltaY += static_cast<int16_t>(ev.value); }
                    else if (ev.code == REL_HWHEEL) { current_payload.scrollDeltaX += static_cast<int16_t>(ev.value); }

                } else if (ev.type == EV_ABS) {
                    if (g_helper_mouse_state_initialized) {
                        bool is_touch_pointer_dev = g_device_is_touch_pointer.count(current_fd) && g_device_is_touch_pointer[current_fd];
                        bool touch_is_active_for_dev = g_device_touch_is_active.count(current_fd) && g_device_touch_is_active[current_fd];
                        bool abs_event_caused_move = false;
                        if (is_touch_pointer_dev && touch_is_active_for_dev) {
                            if (ev.code == ABS_X || (ev.code == ABS_MT_POSITION_X && g_abs_x_info_map.count(current_fd))) {
                                g_pending_abs_x_for_fd[current_fd] = ev.value;
                            } else if (ev.code == ABS_Y || (ev.code == ABS_MT_POSITION_Y && g_abs_y_info_map.count(current_fd))) {
                                g_pending_abs_y_for_fd[current_fd] = ev.value;
                            }
                        } else {
                            if (ev.code == ABS_X || (ev.code == ABS_MT_POSITION_X && g_abs_x_info_map.count(current_fd))) {
                                 const auto it = g_abs_x_info_map.find(current_fd);
                                 if (it != g_abs_x_info_map.end()) {
                                    int32_t old_abs_x = g_helper_abs_x;
                                    g_helper_abs_x = scale_abs_value_to_screen(ev.value, &it->second, g_client_screen_width);
                                    if (g_helper_abs_x != old_abs_x) {
                                        raw_mouse_moved_this_cycle = true;
                                        abs_event_caused_move = true;
                                    }
                                 }
                            } else if (ev.code == ABS_Y || (ev.code == ABS_MT_POSITION_Y && g_abs_y_info_map.count(current_fd))) {
                                const auto it = g_abs_y_info_map.find(current_fd);
                                if (it != g_abs_y_info_map.end()) {
                                   int32_t old_abs_y = g_helper_abs_y;
                                   g_helper_abs_y = scale_abs_value_to_screen(ev.value, &it->second, g_client_screen_height);
                                   if (g_helper_abs_y != old_abs_y) {
                                       raw_mouse_moved_this_cycle = true;
                                       abs_event_caused_move = true;
                                   }
                                }
                            }
                        }
                        if (abs_event_caused_move) {
                           g_last_processed_abs_move_was_trackpad = is_touch_pointer_dev;
                        }
                    }
                }

                if (ev.type == EV_SYN && ev.code == SYN_REPORT && events_accumulated) {
                    bool is_touch_pointer_dev_syn = g_device_is_touch_pointer.count(current_fd) && g_device_is_touch_pointer[current_fd];
                    bool touch_is_active_for_dev_syn = g_device_touch_is_active.count(current_fd) && g_device_touch_is_active[current_fd];

                    if (is_touch_pointer_dev_syn && touch_is_active_for_dev_syn &&
                        g_pending_abs_x_for_fd.count(current_fd) && g_pending_abs_x_for_fd[current_fd].has_value() &&
                        g_pending_abs_y_for_fd.count(current_fd) && g_pending_abs_y_for_fd[current_fd].has_value()) {

                        int32_t current_raw_dev_x = g_pending_abs_x_for_fd[current_fd].value();
                        int32_t current_raw_dev_y = g_pending_abs_y_for_fd[current_fd].value();

                        if (!g_device_initial_raw_abs_at_touch_start[current_fd].has_value()) {
                            g_device_initial_raw_abs_at_touch_start[current_fd] = {current_raw_dev_x, current_raw_dev_y};
                        } else {
                            int32_t initial_raw_x = g_device_initial_raw_abs_at_touch_start[current_fd]->first;
                            int32_t initial_raw_y = g_device_initial_raw_abs_at_touch_start[current_fd]->second;

                            int32_t raw_delta_x = current_raw_dev_x - initial_raw_x;
                            int32_t raw_delta_y = current_raw_dev_y - initial_raw_y;

                            double screen_delta_x = 0.0, screen_delta_y = 0.0;
                            const auto& abs_x_info = g_abs_x_info_map[current_fd];
                            const auto& abs_y_info = g_abs_y_info_map[current_fd];

                            if (abs_x_info.maximum > abs_x_info.minimum && (g_client_screen_width -1) > 0) {
                                screen_delta_x = static_cast<double>(raw_delta_x) /
                                                   (abs_x_info.maximum - abs_x_info.minimum) *
                                                   (g_client_screen_width - 1);
                            }
                            if (abs_y_info.maximum > abs_y_info.minimum && (g_client_screen_height -1) > 0) {
                                screen_delta_y = static_cast<double>(raw_delta_y) /
                                                   (abs_y_info.maximum - abs_y_info.minimum) *
                                                   (g_client_screen_height - 1);
                            }

                            if (g_device_screen_coords_at_touch_start[current_fd].has_value()) {
                                int32_t old_abs_x = g_helper_abs_x;
                                int32_t old_abs_y = g_helper_abs_y;
                                g_helper_abs_x = g_device_screen_coords_at_touch_start[current_fd]->first + static_cast<int32_t>(screen_delta_x);
                                g_helper_abs_y = g_device_screen_coords_at_touch_start[current_fd]->second + static_cast<int32_t>(screen_delta_y);
                                 if (g_helper_abs_x != old_abs_x || g_helper_abs_y != old_abs_y) {
                                    raw_mouse_moved_this_cycle = true;
                                    g_last_processed_abs_move_was_trackpad = true;
                                }
                            }
                        }
                    }
                    g_pending_abs_x_for_fd[current_fd] = std::nullopt;
                    g_pending_abs_y_for_fd[current_fd] = std::nullopt;

                    if (raw_mouse_moved_this_cycle && g_helper_mouse_state_initialized) {
                        g_helper_abs_x = std::max(0, std::min(g_helper_abs_x, static_cast<int32_t>(g_client_screen_width - 1)));
                        g_helper_abs_y = std::max(0, std::min(g_helper_abs_y, static_cast<int32_t>(g_client_screen_height - 1)));
                    }

                    bool mouse_moved_significantly_this_report = false;
                    if (raw_mouse_moved_this_cycle && g_helper_mouse_state_initialized) {
                        int dx = g_helper_abs_x - g_helper_last_sent_abs_x;
                        int dy = g_helper_abs_y - g_helper_last_sent_abs_y;
                        if ((dx * dx + dy * dy) >= HELPER_MOUSE_DEADZONE_SQUARED) {
                            mouse_moved_significantly_this_report = true;
                        }
                    }

                    bool mouse_buttons_changed_this_report = raw_mouse_button_changed_this_cycle;
                    bool send_mouse_update_this_report = mouse_moved_significantly_this_report || mouse_buttons_changed_this_report;

                    bool key_event_is_mouse_button = false;
                    for(const auto& ke : current_payload.keyEvents) {
                        if (LT::Utils::KeycodeConverter::isVkMouseButton(ke.keyCode)) {
                            key_event_is_mouse_button = true; break;
                        }
                    }

                    current_payload.isMouseEvent = (current_payload.scrollDeltaX != 0 || current_payload.scrollDeltaY != 0 ||
                                                    send_mouse_update_this_report || key_event_is_mouse_button);

                    if (current_payload.isMouseEvent) {
                        bool current_device_is_touchpad_system_component = g_device_is_part_of_touchpad_system.count(current_fd) &&
                                                                          g_device_is_part_of_touchpad_system[current_fd];

                        if ((g_last_processed_abs_move_was_trackpad && raw_mouse_moved_this_cycle) ||
                            (current_device_is_touchpad_system_component && mouse_buttons_changed_this_report)
                           ) {
                            current_payload.sourceDeviceType = LT::Network::InputSourceDeviceType::TRACKPAD_ABSOLUTE;
                        } else {
                            current_payload.sourceDeviceType = LT::Network::InputSourceDeviceType::MOUSE_ABSOLUTE;
                        }

                        if (send_mouse_update_this_report && g_helper_mouse_state_initialized && g_client_screen_width > 0 && g_client_screen_height > 0) {
                            current_payload.relativeX = static_cast<float>(g_helper_abs_x) / std::max(1, (g_client_screen_width -1));
                            current_payload.relativeY = static_cast<float>(g_helper_abs_y) / std::max(1, (g_client_screen_height-1));
                            current_payload.relativeX = std::max(0.0f, std::min(1.0f, current_payload.relativeX));
                            current_payload.relativeY = std::max(0.0f, std::min(1.0f, current_payload.relativeY));

                            g_helper_last_sent_abs_x = g_helper_abs_x;
                            g_helper_last_sent_abs_y = g_helper_abs_y;
                        }
                        current_payload.mouseButtons = g_helper_mouse_buttons_state;
                        if (mouse_buttons_changed_this_report) {
                            g_helper_last_sent_mouse_buttons = g_helper_mouse_buttons_state;
                        }
                    }


                    if (!current_payload.keyEvents.empty() || current_payload.isMouseEvent) {
                        std::vector<uint8_t> buffer = LT::Utils::serializeInputPayload(current_payload);
                        asio::error_code ec_write;
                        size_t bytes_written = asio::write(target_socket, asio::buffer(buffer), ec_write);
                        if (ec_write) {
                            LT::Utils::Logger::GetInstance().Error("Input Helper: IPC write error: " + ec_write.message() + ". Bytes written: " + std::to_string(bytes_written));
                            g_helper_running = false; return;
                        }
                    }

                    current_payload = LT::Network::InputPayload();
                    events_accumulated = false;
                    raw_mouse_moved_this_cycle = false;
                    raw_mouse_button_changed_this_cycle = false;
                }
            }
            if (rc == LIBEVDEV_READ_STATUS_SYNC) { }
            else if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {
                LT::Utils::Logger::GetInstance().Warning("Input Helper: libevdev_next_event error on fd " + std::to_string(current_fd) + ": " + strerror(-rc));
            }
        }
    }
}

void simulate_input_event(LT::Network::InputPayload payload) {
    if (!g_uinput_device) {
        LT::Utils::Logger::GetInstance().Warning("Simulating: uinput device not available.");
        return;
    }

    for (const auto& keyEvent : payload.keyEvents) {
        uint16_t evdev_code = LT::Utils::KeycodeConverter::vkToEvdev(keyEvent.keyCode);
        if (evdev_code != 0) {
            libevdev_uinput_write_event(g_uinput_device, EV_KEY, evdev_code, keyEvent.isPressed ? 1 : 0);
        } else {
            LT::Utils::Logger::GetInstance().Warning("Simulating: No evdev_code for vk_code: " + LT::Utils::Logger::getKeyName(keyEvent.keyCode) + " (" + std::to_string(keyEvent.keyCode) + ")");
        }
    }

    if (payload.isMouseEvent && payload.relativeX != -1.0f && payload.relativeY != -1.0f) {
        if (g_client_screen_width > 0 && g_client_screen_height > 0) {

            float processedSimX, processedSimY;
            helper_process_simulated_mouse_coordinates(payload.relativeX, payload.relativeY, payload.sourceDeviceType, processedSimX, processedSimY);
            payload.relativeX = processedSimX;
            payload.relativeY = processedSimY;

            int32_t target_abs_x = static_cast<int32_t>(payload.relativeX * (g_client_screen_width -1) );
            int32_t target_abs_y = static_cast<int32_t>(payload.relativeY * (g_client_screen_height -1));

            target_abs_x = std::max(0, std::min(target_abs_x, static_cast<int32_t>(g_client_screen_width - 1)));
            target_abs_y = std::max(0, std::min(target_abs_y, static_cast<int32_t>(g_client_screen_height - 1)));

            libevdev_uinput_write_event(g_uinput_device, EV_ABS, ABS_X, target_abs_x);
            libevdev_uinput_write_event(g_uinput_device, EV_ABS, ABS_Y, target_abs_y);
        } else {
            LT::Utils::Logger::GetInstance().Warning("Simulating: Screen dimensions unknown in helper, cannot scale relative mouse move.");
        }
    }

    if (payload.scrollDeltaY != 0) {
        libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_WHEEL, payload.scrollDeltaY);
    }
    if (payload.scrollDeltaX != 0) {
        libevdev_uinput_write_event(g_uinput_device, EV_REL, REL_HWHEEL, payload.scrollDeltaX);
    }

    libevdev_uinput_write_event(g_uinput_device, EV_SYN, SYN_REPORT, 0);
}

void handle_ipc_command(const char* data, size_t length, asio::local::stream_protocol::socket& source_socket) {
    if (length == 0) return;
    IPCCommandType command_type = static_cast<IPCCommandType>(data[0]);

    switch (command_type) {
        case IPCCommandType::SimulateInput: {
            if (length > 1) {
                auto payload_opt = LT::Utils::deserializeInputPayload(reinterpret_cast<const uint8_t*>(data + 1), length - 1);
                if (payload_opt) simulate_input_event(*payload_opt);
                else LT::Utils::Logger::GetInstance().Warning("Input Helper: Failed to deserialize SimulateInput payload.");
            }
            break;
        }
        case IPCCommandType::PauseStream:
            LT::Utils::Logger::GetInstance().Info("Input Helper: PauseStream command received (IGNORED - helper always polls).");
            break;
        case IPCCommandType::ResumeStream:
            LT::Utils::Logger::GetInstance().Info("Input Helper: ResumeStream command received (IGNORED - helper always polls).");
            helper_reset_simulation_state();
            break;
        case IPCCommandType::Shutdown:
            LT::Utils::Logger::GetInstance().Info("Input Helper: Shutdown command received.");
            g_helper_running = false;
            if (g_ipc_io_context_ptr) g_ipc_io_context_ptr->stop();
            if (source_socket.is_open()) { asio::error_code ec; source_socket.close(ec); }
            break;
        default:
            LT::Utils::Logger::GetInstance().Warning("Input Helper: Unknown IPC command: " + std::to_string(static_cast<int>(command_type)));
            break;
    }
}

int runInputHelperMode(int argc, char **argv) {
    if (argc < 6) {
        std::cerr << "Input Helper ERROR: Insufficient arguments. Expected 6, got " << argc << std::endl;
        return 1;
    }
    uid_t original_user_uid = static_cast<uid_t>(atoi(argv[2]));
    const char *original_username = argv[3];
    g_client_screen_width = atoi(argv[4]);
    g_client_screen_height = atoi(argv[5]);

    LT::Utils::Logger::GetInstance().Info("--- Input Helper Mode Started (PID: " + std::to_string(getpid()) +
                                         ", User: " + original_username + " (" + std::to_string(original_user_uid) + ")" +
                                         ", Screen: " + std::to_string(g_client_screen_width) + "x" + std::to_string(g_client_screen_height) + ") ---");

    if (!setup_shared_memory()) { return 1; }

    signal(SIGINT, helper_signal_handler);
    signal(SIGTERM, helper_signal_handler);
    signal(SIGHUP, helper_signal_handler);
    signal(SIGPIPE, SIG_IGN);

    if (!initialize_input_devices()) {
        cleanup_helper_resources(); return 1;
    }

    G_ACTUAL_SOCKET_PATH = "/tmp/localtether_helper_" + std::string(original_username) + "_" + std::to_string(getpid());

    asio::io_context local_io_context;
    g_ipc_io_context_ptr = &local_io_context;
    asio::local::stream_protocol::socket main_app_socket(local_io_context);
    g_main_app_socket_ptr = &main_app_socket;

    std::thread input_polling_thread_obj;

    try {
        std::filesystem::remove(G_ACTUAL_SOCKET_PATH);
        asio::local::stream_protocol::acceptor acceptor(local_io_context, asio::local::stream_protocol::endpoint(G_ACTUAL_SOCKET_PATH));

        if (getuid() == 0 && original_user_uid != 0) {
            struct passwd *pw = getpwuid(original_user_uid);
            if (pw) {
                if (chown(G_ACTUAL_SOCKET_PATH.c_str(), original_user_uid, pw->pw_gid) == -1) {
                    LT::Utils::Logger::GetInstance().Warning("Input Helper: chown socket to " + std::to_string(original_user_uid) + " failed: " + strerror(errno));
                }else{
                    LT::Utils::Logger::GetInstance().Info("Input Helper: Socket ownership changed to " + std::string(pw->pw_name) + " (" + std::to_string(original_user_uid) + ")");
                }
            }
        }

        if (chmod(G_ACTUAL_SOCKET_PATH.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
             LT::Utils::Logger::GetInstance().Warning("Input Helper: chmod socket failed: " + std::string(strerror(errno)));
        }else{
            LT::Utils::Logger::GetInstance().Info("Input Helper: Socket permissions set to 0777.");
        }

        LT::Utils::Logger::GetInstance().Info("Input Helper: Listening on " + G_ACTUAL_SOCKET_PATH);
        write_info_to_shared_memory();

        acceptor.accept(main_app_socket);
        LT::Utils::Logger::GetInstance().Info("Input Helper: Main application connected.");
        acceptor.close();

        input_polling_thread_obj = std::thread([&main_app_socket]() {
            LT::Utils::Logger::GetInstance().Info("Input Helper: Input polling thread started.");
            while (g_helper_running.load(std::memory_order_relaxed)) {
                poll_events_once_and_send(main_app_socket);
            }
            LT::Utils::Logger::GetInstance().Info("Input Helper: Input polling thread finished.");
        });

        helper_reset_simulation_state();
        std::array<char, 2048> ipc_read_buffer_local;
        while (g_helper_running.load(std::memory_order_relaxed)) {
            asio::error_code error;
            size_t length = main_app_socket.read_some(asio::buffer(ipc_read_buffer_local), error);
            if (!g_helper_running) break;

            if (error == asio::error::eof || error == asio::error::connection_reset) {
                LT::Utils::Logger::GetInstance().Info("Input Helper: Main app disconnected.");
                g_helper_running = false; break;
            } else if (error) {
                if (error != asio::error::operation_aborted) {
                     LT::Utils::Logger::GetInstance().Error("Input Helper: IPC read error: " + error.message());
                }
                g_helper_running = false; break;
            }
            if (length > 0) {
                handle_ipc_command(ipc_read_buffer_local.data(), length, main_app_socket);
            }
        }
    } catch (const std::exception& e) {
        LT::Utils::Logger::GetInstance().Error("Input Helper: Exception: " + std::string(e.what()));
        g_helper_running = false;
    }

    g_helper_running = false;
    if (g_ipc_io_context_ptr && !g_ipc_io_context_ptr->stopped()) {
         g_ipc_io_context_ptr->stop();
    }
    if (input_polling_thread_obj.joinable()) {
        input_polling_thread_obj.join();
    }

    cleanup_helper_resources();
    LT::Utils::Logger::GetInstance().Info("--- Input Helper Mode Terminated ---");
    return 0;
}

}
#endif