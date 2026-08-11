import time
import board
import digitalio
import busio
import usb_hid
from hid_gamepad import Gamepad
from adafruit_hid.mouse import Mouse
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

gp = Gamepad(usb_hid.devices)
mouse = Mouse(usb_hid.devices)

button_pins = [
    (board.GP0, 1),
    (board.GP1, 2),
    (board.GP2, 3),
    (board.GP3, 4),
    (board.GP4, 5),
    (board.GP5, 6),
    (board.GP6, 7),
    (board.GP7, 8),
    (board.GP8, 9),
    (board.GP9, 10),
    (board.GP10, 11),
    (board.GP11, 12),
    (board.GP12, 13),
    (board.GP13, 14),
    (board.GP14, 15),
    (board.GP15, 16),
]

buttons = []
for pin, number in button_pins:
    b = digitalio.DigitalInOut(pin)
    b.direction = digitalio.Direction.INPUT
    b.pull = digitalio.Pull.UP
    buttons.append([b, number, True])

mode_button = digitalio.DigitalInOut(board.GP20)
mode_button.direction = digitalio.Direction.INPUT
mode_button.pull = digitalio.Pull.UP
mouse_mode = False
last_mode_button_state = True

spi = busio.SPI(clock=board.GP18, MISO=board.GP16, MOSI=board.GP19)
cs = digitalio.DigitalInOut(board.GP17)
mcp = MCP.MCP3008(spi, cs)

left_x = AnalogIn(mcp, MCP.P0)
left_y = AnalogIn(mcp, MCP.P1)
right_x = AnalogIn(mcp, MCP.P2)
right_y = AnalogIn(mcp, MCP.P3)

def scale_stick(raw_value, center=32768, deadzone=1500):
    offset = raw_value - center
    if abs(offset) < deadzone:
        return 0
    scaled = int((offset / 32768) * 127)
    return max(-127, min(127, scaled))

MOUSE_SENSITIVITY = 8 

while True:
    for entry in buttons:
        b, number, last_state = entry
        current_state = b.value
        if current_state != last_state:
            time.sleep(0.02)
            current_state = b.value
            if current_state != last_state:
                if current_state is False:
                    gp.press_buttons(number)
                else:
                    gp.release_buttons(number)
                entry[2] = current_state

    current_mode_button_state = mode_button.value
    if current_mode_button_state != last_mode_button_state:
        time.sleep(0.02)
        current_mode_button_state = mode_button.value
        if current_mode_button_state is False:
            mouse_mode = not mouse_mode
        last_mode_button_state = current_mode_button_state

    left_x_scaled = scale_stick(left_x.value)
    left_y_scaled = scale_stick(left_y.value)

    right_x_scaled = scale_stick(right_x.value)
    right_y_scaled = scale_stick(right_y.value)

    if mouse_mode:
        gp.move_joysticks(x=left_x_scaled, y=left_y_scaled)
        if right_x_scaled != 0 or right_y_scaled != 0:
            mouse.move(x=right_x_scaled // MOUSE_SENSITIVITY, y=right_y_scaled // MOUSE_SENSITIVITY)
    else:
        gp.move_joysticks(x=left_x_scaled, y=left_y_scaled, z=right_x_scaled, r_z=right_y_scaled)

    time.sleep(0.01)