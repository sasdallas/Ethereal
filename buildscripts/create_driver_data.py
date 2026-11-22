#!/usr/bin/python3

import sys
import json
from collections import defaultdict

# create_driver_data.py - Create a JSON object with driver data
# Expects a bunch of directories to be passed containing driver.confs

# !!!!!!!!!!!!!!!! EDIT THESE VARIABLES IF YOU CHANGED driver.h
DRIVER_ENVIRONMENT_NORMAL = 0
DRIVER_ENVIRONMENT_PRELOAD = 1
DRIVER_ENVIRONMENT_ANY = 2

DRIVER_VERSION = 1

DRIVER_CRITICAL = 0
DRIVER_WARN = 1
DRIVER_IGNORE = 2


class Driver:
    def __init__(self, filename: str, priority: str, environment: str, load_before: list):
        self.filename = filename
        if priority == "CRITICAL":
            self.priority = DRIVER_CRITICAL
        elif priority == "WARN":
            self.priority = DRIVER_WARN
        elif priority == "IGNORE":
            self.priority = DRIVER_IGNORE
        else:
            print(f"WARNING: Driver \"{filename}\" has unrecognized priority \"{priority}\". Assuming IGNORE")
            self.priority = DRIVER_IGNORE

        if environment == "NORMAL":
            self.environment = DRIVER_ENVIRONMENT_NORMAL
        elif environment == "PRELOAD":
            self.environment = DRIVER_ENVIRONMENT_PRELOAD
        elif environment == "ANY":
            self.environment = DRIVER_ENVIRONMENT_ANY
        else:
            print(f"WARNING: Driver \"{filename}\" has unrecognized environment \"{environment}\". Assuming NORMAL")
            self.environment = DRIVER_ENVIRONMENT_NORMAL

        self.load_before = load_before



# Solve driver constraints (LOAD_BEFORE)
def solve_driver_constraints(driver_filenames, constraints):
    graph = defaultdict(list)
    in_degree = {driver: 0 for driver in driver_filenames}

    for before, after in constraints:
        graph[before].append(after)
        in_degree[after] += 1
    
    queue = [driver for driver in driver_filenames if in_degree[driver] == 0]
    ordered_items = []

    while queue:
        driver = queue.pop(0)
        ordered_items.append(driver)
        for neighbor in graph[driver]:
            in_degree[neighbor] -= 1
            if in_degree[neighbor] == 0:
                queue.append(neighbor)

    if len(ordered_items) != len(driver_filenames):
        # Failed to solve the constraint.
        raise RuntimeError("Impossible constraints detected. Cannot continue.")

    return ordered_items



data = {"version": DRIVER_VERSION, "drivers": []}

drivers = {} # Dict of drivers to load
constraints = [] # List of must-come-before tuples, i.e. (A, B) means that A must come before B
driver_filenames = [] # !!!: Quick annoying hack for solving the constraints

for i in range(1, len(sys.argv)):
    driver = sys.argv[i]
    
    # Try to open the config file
    conflines = []

    try:
        conf = open(f"{driver}/driver.conf", "r")
        conflines = [line.strip() for line in conf.readlines()]
        conf.close()
    except Exception:
        raise FileNotFoundError(f"{driver}/driver.conf not found")
    
    filename = [line for line in conflines if line.startswith("FILENAME = ")][0].replace("\"", "").replace("FILENAME = ", "")
    priority = [line for line in conflines if line.startswith("PRIORITY = ")][0].replace("PRIORITY = ", "")
    environment = [line for line in conflines if line.startswith("ENVIRONMENT = ")][0].replace("ENVIRONMENT = ", "")
    loadbefore = [line for line in conflines if line.startswith("LOAD_BEFORE = ")]

    if len(loadbefore) > 0:
        loadbefore = loadbefore[0].replace("LOAD_BEFORE = ", "")
        loadbefore = loadbefore.split(",")
        for i in loadbefore:
            constraints.append((filename, i))
    else:
        loadbefore = []

    # Dictionary for quicker access
    drivers[filename] = Driver(filename, priority, environment, loadbefore)

    # Solve list (hate this..)
    driver_filenames.append(filename)



# Solve!
solved = solve_driver_constraints(driver_filenames, constraints)

# Now resolve this back...
final_driver_list = [drivers[filename] for filename in solved]

for driver in final_driver_list:
    driver_data = {"filename": driver.filename, "priority": driver.priority, "environment": driver.environment, "load_before": driver.load_before}
    data["drivers"].append(driver_data)

# Write the data to the file
output = open("driver_conf.json", "w+")
output.write(json.dumps(data))
output.close()

# All done!