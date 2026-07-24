import struct
import time
from adafruit_hid import find_device


class Gamepad:
    """Emulate a generic gamepad with 16 buttons and two joysticks."""

    def __init__(self, devices):
        self._gamepad_device = find_device(devices, usage_page=0x1, usage=0x05)
        self._report = bytearray(6)
        self._last_report = bytearray(6)
        self._buttons_state = 0
        self._joy_x = 0
        self._joy_y = 0
        self._joy_z = 0
        self._joy_r_z = 0
        try:
            self.reset_all()
        except OSError:
            time.sleep(1)
            self.reset_all()

    def press_buttons(self, *buttons):
        for button in buttons:
            self._buttons_state |= 1 << self._validate_button_number(button) - 1
        self._send()

    def release_buttons(self, *buttons):
        for button in buttons:
            self._buttons_state &= ~(1 << self._validate_button_number(button) - 1)
        self._send()

    def release_all_buttons(self):
        self._buttons_state = 0
        self._send()

    def click_buttons(self, *buttons):
        self.press_buttons(*buttons)
        self.release_buttons(*buttons)

    def move_joysticks(self, x=None, y=None, z=None, r_z=None):
        if x is not None:
            self._joy_x = self._validate_joystick_value(x)
        if y is not None:
            self._joy_y = self._validate_joystick_value(y)
        if z is not None:
            self._joy_z = self._validate_joystick_value(z)
        if r_z is not None:
            self._joy_r_z = self._validate_joystick_value(r_z)
        self._send()

    def reset_all(self):
        self._buttons_state = 0
        self._joy_x = 0
        self._joy_y = 0
        self._joy_z = 0
        self._joy_r_z = 0
        self._send(always=True)

    def _send(self, always=False):
        struct.pack_into(
            "<Hbbbb", self._report, 0,
            self._buttons_state, self._joy_x, self._joy_y, self._joy_z, self._joy_r_z,
        )
        if always or self._last_report != self._report:
            self._gamepad_device.send_report(self._report)
            self._last_report[:] = self._report

    @staticmethod
    def _validate_button_number(button):
        if not 1 <= button <= 16:
            raise ValueError("Button number must in range 1 to 16")
        return button

    @staticmethod
    def _validate_joystick_value(value):
        if not -127 <= value <= 127:
            raise ValueError("Joystick value must be in range -127 to 127")
        return value