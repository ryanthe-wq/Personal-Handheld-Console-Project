#include "bsp/board.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <algorithm>
#include <cstdint>

spi_inst_t *const MCP_SPI = spi0;
constexpr uint8_t MCP_SCK = 18;
constexpr uint8_t MCP_MOSI = 19;
constexpr uint8_t MCP_MISO = 16;
constexpr uint8_t MCP_CS = 17;
constexpr uint8_t MODE_BUTTON = 20;
constexpr uint8_t BUTTON_16 = 15;
constexpr uint8_t BUTTON_COUNT = 16;

const uint8_t button_pins[BUTTON_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, BUTTON_16};

bool button_states[BUTTON_COUNT];
bool last_mode_button_state = true;
bool mouse_mode = false;
uint16_t gamepad_buttons = 0;
absolute_time_t next_button_read;

enum : uint8_t { REPORT_ID_MOUSE = 1, REPORT_ID_GAMEPAD = 2 };

// ====================================================================
// MANDATORY TINYUSB USB DESCRIPTORS (Fixes "Device Not Recognized")
// ====================================================================

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCAFE, // Generic Development VID
    .idProduct          = 0x4005, // Generic Dev PID
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

extern "C" uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

uint8_t const hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(REPORT_ID_GAMEPAD)),
};

extern "C" uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
    (void) itf;
    return hid_report_descriptor;
}

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID         0x81

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
};

extern "C" uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: Supported language is English (0x0409)
    "Raspberry Pi",                // 1: Manufacturer
    "Pico Gamepad/Mouse",          // 2: Product
    "123456",                      // 3: Serials (Keep dummy or leave blank)
};

static uint16_t _desc_str[32];
extern "C" uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;
    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;
        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        for(uint8_t i=0; i<chr_count; i++) _desc_str[1+i] = str[i];
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);
    return _desc_str;
}

// ====================================================================
// GAMEPAD LOGIC & CORE FUNCTIONS
// ====================================================================

uint16_t read_mcp3008(uint8_t channel) {
    if (channel > 7) return 0;
    uint8_t tx[3] = {0x01, static_cast<uint8_t>(0x80 | (channel << 4)), 0x00};
    uint8_t rx[3] = {};
    gpio_put(MCP_CS, 0);
    spi_write_read_blocking(MCP_SPI, tx, rx, 3);
    gpio_put(MCP_CS, 1);
    return static_cast<uint16_t>(((rx[1] & 0x03) << 8) | rx[2]);
}

int scale_stick(uint16_t raw_value, int center = 32768, int deadzone = 1500) {
    const int offset = static_cast<int>(raw_value) - center;
    if (std::abs(offset) < deadzone) return 0;
    return std::clamp((offset * 127) / 32768, -127, 127);
}

void send_gamepad_report(int left_x, int left_y, int right_x, int right_y) {
    if (!tud_hid_ready()) return;
    hid_gamepad_report_t report = {};
    report.x = static_cast<int8_t>(left_x);
    report.y = static_cast<int8_t>(left_y);
    report.z = static_cast<int8_t>(right_x);
    report.rz = static_cast<int8_t>(right_y);
    report.buttons = gamepad_buttons;
    tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
}

void send_mouse_report(int x, int y) {
    if (!tud_hid_ready()) return;
    hid_mouse_report_t report = {};
    report.x = static_cast<int8_t>(std::clamp(x, -127, 127));
    report.y = static_cast<int8_t>(std::clamp(y, -127, 127));
    tud_hid_report(REPORT_ID_MOUSE, &report, sizeof(report));
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) itf; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) itf; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

int main() {
    board_init();

    for (uint8_t index = 0; index < BUTTON_COUNT; ++index) {
        gpio_init(button_pins[index]);
        gpio_set_dir(button_pins[index], GPIO_IN);
        gpio_pull_up(button_pins[index]);
        button_states[index] = true;
    }

    gpio_init(MODE_BUTTON);
    gpio_set_dir(MODE_BUTTON, GPIO_IN);
    gpio_pull_up(MODE_BUTTON);

    gpio_init(MCP_CS);
    gpio_set_dir(MCP_CS, GPIO_OUT);
    gpio_put(MCP_CS, 1);

    spi_init(MCP_SPI, 1000 * 1000);
    spi_set_format(MCP_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(MCP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(MCP_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(MCP_MISO, GPIO_FUNC_SPI);

    tusb_init();
    next_button_read = get_absolute_time();

    while (true) {
        tud_task();
        if (!time_reached(next_button_read)) continue;
        next_button_read = make_timeout_time_ms(10);

        for (uint8_t index = 0; index < BUTTON_COUNT; ++index) {
            const bool current_state = gpio_get(button_pins[index]);
            if (current_state == button_states[index]) continue;
            sleep_ms(20);
            const bool confirmed_state = gpio_get(button_pins[index]);
            if (confirmed_state == button_states[index]) continue;

            const uint16_t mask = 1u << index;
            if (!confirmed_state) {
                gamepad_buttons |= mask;
            } else {
                gamepad_buttons &= static_cast<uint16_t>(~mask);
            }
            button_states[index] = confirmed_state;
        }

        bool current_mode_button_state = gpio_get(MODE_BUTTON);
        if (current_mode_button_state != last_mode_button_state) {
            sleep_ms(20);
            current_mode_button_state = gpio_get(MODE_BUTTON);
            if (!current_mode_button_state) {
                mouse_mode = !mouse_mode;
            }
            last_mode_button_state = current_mode_button_state;
        }

        const int left_x = scale_stick(read_mcp3008(0) << 6);
        const int left_y = scale_stick(read_mcp3008(1) << 6);
        const int right_x = scale_stick(read_mcp3008(2) << 6);
        const int right_y = scale_stick(read_mcp3008(3) << 6);

        if (mouse_mode) {
            send_gamepad_report(left_x, left_y, 0, 0);
            if (right_x != 0 || right_y != 0) {
                send_mouse_report(right_x / 8, right_y / 8);
            }
        } else {
            send_gamepad_report(left_x, left_y, right_x, right_y);
        }
        sleep_ms(10);
    }
}
