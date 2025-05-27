#include "utils/KeycodeConverter.h"
#include <unordered_map>

//AI generated cuz i cant be bothered to write translation code like this myself

#ifndef _WIN32

namespace LocalTether {
namespace Utils {

namespace {

const std::unordered_map<uint16_t, uint8_t> evdev_to_vk_map = {
    {KEY_ESC, VK_ESCAPE}, {KEY_1, '1'}, {KEY_2, '2'}, {KEY_3, '3'}, {KEY_4, '4'}, {KEY_5, '5'},
    {KEY_6, '6'}, {KEY_7, '7'}, {KEY_8, '8'}, {KEY_9, '9'}, {KEY_0, '0'},
    {KEY_MINUS, VK_OEM_MINUS}, {KEY_EQUAL, VK_OEM_PLUS}, // VK_OEM_PLUS for =, VK_OEM_MINUS for -
    {KEY_BACKSPACE, VK_BACK}, {KEY_TAB, VK_TAB},
    {KEY_Q, 'Q'}, {KEY_W, 'W'}, {KEY_E, 'E'}, {KEY_R, 'R'}, {KEY_T, 'T'}, {KEY_Y, 'Y'},
    {KEY_U, 'U'}, {KEY_I, 'I'}, {KEY_O, 'O'}, {KEY_P, 'P'},
    {KEY_LEFTBRACE, VK_OEM_4}, {KEY_RIGHTBRACE, VK_OEM_6}, {KEY_ENTER, VK_RETURN},
    {KEY_LEFTCTRL, VK_LCONTROL}, {KEY_A, 'A'}, {KEY_S, 'S'}, {KEY_D, 'D'}, {KEY_F, 'F'},
    {KEY_G, 'G'}, {KEY_H, 'H'}, {KEY_J, 'J'}, {KEY_K, 'K'}, {KEY_L, 'L'},
    {KEY_SEMICOLON, VK_OEM_1}, {KEY_APOSTROPHE, VK_OEM_7}, {KEY_GRAVE, VK_OEM_3}, // `~
    {KEY_LEFTSHIFT, VK_LSHIFT}, {KEY_BACKSLASH, VK_OEM_5},
    {KEY_Z, 'Z'}, {KEY_X, 'X'}, {KEY_C, 'C'}, {KEY_V, 'V'}, {KEY_B, 'B'}, {KEY_N, 'N'},
    {KEY_M, 'M'}, {KEY_COMMA, VK_OEM_COMMA}, {KEY_DOT, VK_OEM_PERIOD}, {KEY_SLASH, VK_OEM_2},
    {KEY_RIGHTSHIFT, VK_RSHIFT}, {KEY_KPASTERISK, VK_MULTIPLY},
    {KEY_LEFTALT, VK_LMENU}, {KEY_SPACE, VK_SPACE}, {KEY_CAPSLOCK, VK_CAPITAL},
    {KEY_F1, VK_F1}, {KEY_F2, VK_F2}, {KEY_F3, VK_F3}, {KEY_F4, VK_F4}, {KEY_F5, VK_F5},
    {KEY_F6, VK_F6}, {KEY_F7, VK_F7}, {KEY_F8, VK_F8}, {KEY_F9, VK_F9}, {KEY_F10, VK_F10},
    {KEY_NUMLOCK, VK_NUMLOCK}, {KEY_SCROLLLOCK, VK_SCROLL},
    {KEY_KP7, VK_NUMPAD7}, {KEY_KP8, VK_NUMPAD8}, {KEY_KP9, VK_NUMPAD9}, {KEY_KPMINUS, VK_SUBTRACT},
    {KEY_KP4, VK_NUMPAD4}, {KEY_KP5, VK_NUMPAD5}, {KEY_KP6, VK_NUMPAD6}, {KEY_KPPLUS, VK_ADD},
    {KEY_KP1, VK_NUMPAD1}, {KEY_KP2, VK_NUMPAD2}, {KEY_KP3, VK_NUMPAD3},
    {KEY_KP0, VK_NUMPAD0}, {KEY_KPDOT, VK_DECIMAL}, {KEY_KPENTER, VK_RETURN}, // Numpad Enter often same as main
    {KEY_RIGHTCTRL, VK_RCONTROL}, {KEY_KPSLASH, VK_DIVIDE},
    {KEY_SYSRQ, VK_SNAPSHOT}, // PrintScreen
    {KEY_RIGHTALT, VK_RMENU},
    {KEY_HOME, VK_HOME}, {KEY_UP, VK_UP}, {KEY_PAGEUP, VK_PRIOR}, {KEY_LEFT, VK_LEFT},
    {KEY_RIGHT, VK_RIGHT}, {KEY_END, VK_END}, {KEY_DOWN, VK_DOWN}, {KEY_PAGEDOWN, VK_NEXT},
    {KEY_INSERT, VK_INSERT}, {KEY_DELETE, VK_DELETE},
    {KEY_MUTE, VK_VOLUME_MUTE}, {KEY_VOLUMEDOWN, VK_VOLUME_DOWN}, {KEY_VOLUMEUP, VK_VOLUME_UP},
    {KEY_POWER, VK_POWER}, {KEY_KPEQUAL, VK_OEM_PLUS}, // Numpad =
    {KEY_PAUSE, VK_PAUSE},
    {KEY_KPCOMMA, VK_SEPARATOR}, // Numpad comma
    {KEY_LEFTMETA, VK_LWIN}, {KEY_RIGHTMETA, VK_RWIN}, {KEY_COMPOSE, VK_APPS}, // Menu/Compose key

    // Mouse buttons
    {BTN_LEFT, VK_LBUTTON},
    {BTN_RIGHT, VK_RBUTTON},
    {BTN_MIDDLE, VK_MBUTTON},
    {BTN_SIDE, VK_XBUTTON1},   // Often BTN_EXTRA or BTN_SIDE
    {BTN_EXTRA, VK_XBUTTON2}, // Often BTN_FORWARD or BTN_EXTRA
    // BTN_FORWARD, BTN_BACK might also map to XBUTTONs depending on mouse
    {BTN_TASK, 0}, // No direct VK, could map to something custom if needed

    // For keys like Shift, Ctrl, Alt, we use the specific L/R versions.
    // Generic VK_SHIFT, VK_CONTROL, VK_MENU are less precise for simulation.
};

// Partial map: VK to evdev
const std::unordered_map<uint8_t, uint16_t> vk_to_evdev_map = {
    {VK_ESCAPE, KEY_ESC}, {'1', KEY_1}, {'2', KEY_2}, {'3', KEY_3}, {'4', KEY_4}, {'5', KEY_5},
    {'6', KEY_6}, {'7', KEY_7}, {'8', KEY_8}, {'9', KEY_9}, {'0', KEY_0},
    {VK_OEM_MINUS, KEY_MINUS}, {VK_OEM_PLUS, KEY_EQUAL},
    {VK_BACK, KEY_BACKSPACE}, {VK_TAB, KEY_TAB},
    {'Q', KEY_Q}, {'W', KEY_W}, {'E', KEY_E}, {'R', KEY_R}, {'T', KEY_T}, {'Y', KEY_Y},
    {'U', KEY_U}, {'I', KEY_I}, {'O', KEY_O}, {'P', KEY_P},
    {VK_OEM_4, KEY_LEFTBRACE}, {VK_OEM_6, KEY_RIGHTBRACE}, {VK_RETURN, KEY_ENTER},
    {VK_LCONTROL, KEY_LEFTCTRL}, {'A', KEY_A}, {'S', KEY_S}, {'D', KEY_D}, {'F', KEY_F},
    {'G', KEY_G}, {'H', KEY_H}, {'J', KEY_J}, {'K', KEY_K}, {'L', KEY_L},
    {VK_OEM_1, KEY_SEMICOLON}, {VK_OEM_7, KEY_APOSTROPHE}, {VK_OEM_3, KEY_GRAVE},
    {VK_LSHIFT, KEY_LEFTSHIFT}, {VK_OEM_5, KEY_BACKSLASH},
    {'Z', KEY_Z}, {'X', KEY_X}, {'C', KEY_C}, {'V', KEY_V}, {'B', KEY_B}, {'N', KEY_N},
    {'M', KEY_M}, {VK_OEM_COMMA, KEY_COMMA}, {VK_OEM_PERIOD, KEY_DOT}, {VK_OEM_2, KEY_SLASH},
    {VK_RSHIFT, KEY_RIGHTSHIFT}, {VK_MULTIPLY, KEY_KPASTERISK},
    {VK_LMENU, KEY_LEFTALT}, {VK_SPACE, KEY_SPACE}, {VK_CAPITAL, KEY_CAPSLOCK},
    {VK_F1, KEY_F1}, {VK_F2, KEY_F2}, {VK_F3, KEY_F3}, {VK_F4, KEY_F4}, {VK_F5, KEY_F5},
    {VK_F6, KEY_F6}, {VK_F7, KEY_F7}, {VK_F8, KEY_F8}, {VK_F9, KEY_F9}, {VK_F10, KEY_F10},
    {VK_NUMLOCK, KEY_NUMLOCK}, {VK_SCROLL, KEY_SCROLLLOCK},
    {VK_NUMPAD7, KEY_KP7}, {VK_NUMPAD8, KEY_KP8}, {VK_NUMPAD9, KEY_KP9}, {VK_SUBTRACT, KEY_KPMINUS},
    {VK_NUMPAD4, KEY_KP4}, {VK_NUMPAD5, KEY_KP5}, {VK_NUMPAD6, KEY_KP6}, {VK_ADD, KEY_KPPLUS},
    {VK_NUMPAD1, KEY_KP1}, {VK_NUMPAD2, KEY_KP2}, {VK_NUMPAD3, KEY_KP3},
    {VK_NUMPAD0, KEY_KP0}, {VK_DECIMAL, KEY_KPDOT}, // VK_RETURN for KP_ENTER handled by main ENTER
    {VK_RCONTROL, KEY_RIGHTCTRL}, {VK_DIVIDE, KEY_KPSLASH},
    {VK_SNAPSHOT, KEY_SYSRQ},
    {VK_RMENU, KEY_RIGHTALT},
    {VK_HOME, KEY_HOME}, {VK_UP, KEY_UP}, {VK_PRIOR, KEY_PAGEUP}, {VK_LEFT, KEY_LEFT},
    {VK_RIGHT, KEY_RIGHT}, {VK_END, KEY_END}, {VK_DOWN, KEY_DOWN}, {VK_NEXT, KEY_PAGEDOWN},
    {VK_INSERT, KEY_INSERT}, {VK_DELETE, KEY_DELETE},
    {VK_VOLUME_MUTE, KEY_MUTE}, {VK_VOLUME_DOWN, KEY_VOLUMEDOWN}, {VK_VOLUME_UP, KEY_VOLUMEUP},
    {VK_POWER, KEY_POWER},
    {VK_PAUSE, KEY_PAUSE},
    {VK_SEPARATOR, KEY_KPCOMMA},
    {VK_LWIN, KEY_LEFTMETA}, {VK_RWIN, KEY_RIGHTMETA}, {VK_APPS, KEY_COMPOSE},

    // Mouse buttons
    {VK_LBUTTON, BTN_LEFT},
    {VK_RBUTTON, BTN_RIGHT},
    {VK_MBUTTON, BTN_MIDDLE},
    {VK_XBUTTON1, BTN_SIDE},
    {VK_XBUTTON2, BTN_EXTRA},

    // Generic Shift, Ctrl, Alt - map to Left versions by default for simulation if specific L/R not given
    {VK_SHIFT, KEY_LEFTSHIFT},
    {VK_CONTROL, KEY_LEFTCTRL},
    {VK_MENU, KEY_LEFTALT}, // ALT key
};

} // anonymous namespace

uint8_t KeycodeConverter::evdevToVk(uint16_t evdevCode) {
    auto it = evdev_to_vk_map.find(evdevCode);
    if (it != evdev_to_vk_map.end()) {
        return it->second;
    }
    // Handle ASCII printable characters directly if not in map (e.g. KEY_MINUS, KEY_EQUAL etc might be better mapped)
    // This is a simplification; a full mapping is complex.
    // For unmapped keys, return 0 or a special "unmapped" VK code.
    return 0; 
}

uint16_t KeycodeConverter::vkToEvdev(uint8_t vkCode) {
    auto it = vk_to_evdev_map.find(vkCode);
    if (it != vk_to_evdev_map.end()) {
        return it->second;
    }
    return 0;
}

bool KeycodeConverter::isVkMouseButton(uint8_t vkCode) {
    switch (vkCode) {
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
            return true;
        default:
            return false;
    }
}

}
}

#endif 