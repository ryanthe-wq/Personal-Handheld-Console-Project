import time
import board
import digitalio
import usb_hid
from hid_gamepad import Gamepad

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
    (board.GP8, 9),
    (board.GP9, 10),
]

buttons = []
for pin, number in button_pins:
    b = digitalio.DigitalInOut(pin)
    b.direction = digitalio.Direction.INPUT
    b.pull = digitalio.Pull.UP
    buttons.append([b, number, True])

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
                    print("pressed", number)
                else:
                    gp.release_buttons(number)
                    print("released", number)
                entry[2] = current_state
    time.sleep(0.01)