import busio
import digitalio
import board
import time
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

spi = busio.SPI(clock=board.GP18, MISO=board.GP16, MOSI=board.GP19)
cs = digitalio.DigitalInOut(board.GP17)
mcp = MCP.MCP3008(spi, cs)

left_x = AnalogIn(mcp, MCP.P0)
left_y = AnalogIn(mcp, MCP.P1)

while True:
    print(left_x.value, left_y.value)
    time.sleep(0.2)