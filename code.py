import time
import board
import digitalio
import busio
import usb_hid
from hid_gamepad import Gamepad
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

gp = Gamepad(usb_hid.devices)

button_pins = [
    (board.GP0, 1),
    (board.GP1, 2),
    (board.GP2, 3),
    (board.GP3, 4),
    (board.GP4, 5),
    (board.GP5, 6),
    (board.GP6, 7),
    (board.GP7, 8),
]

buttons = []
for pin, number in button_pins:
    b = digitalio.DigitalInOut(pin)
    b.direction = digitalio.Direction.INPUT
    b.pull = digitalio.Pull.UP
    buttons.append([b, number, True])

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

    gp.move_joysticks(
        x=scale_stick(left_x.value),
        y=scale_stick(left_y.value),
        z=scale_stick(right_x.value),
        r_z=scale_stick(right_y.value),
    )

    time.sleep(0.01)