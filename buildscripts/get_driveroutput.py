import sys

# get_driveroutput.py - Returns the filename to output to (hacky).

try:
    file = open("driver.conf", "r")
except FileNotFoundError:
    print("Error: driver.conf not found!")
    sys.exit(-1)

# Get the filename line using some funny python string manipulation
lines = file.read().splitlines()
line = [line for line in lines if line.startswith("FILENAME = ")]
line = line[0].replace("FILENAME = ", "")
line = line.replace("\"", "")

print(line)