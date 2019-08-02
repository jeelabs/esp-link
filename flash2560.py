#!/usr/bin/env python

# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# Fabio Manz wrote this file. As long as you retain 
# this notice you can do whatever you want with this stuff. If we meet some day, 
# and you think this stuff is worth it, you can buy me a beer in return. 
# ----------------------------------------------------------------------------

import sys
import re
import requests
import platform  # For getting the operating system name
import subprocess  # For executing a shell command
import os
import time

ip_regex = "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"


def main(argv):
    print("--- flash2560 - Created by Fabio Manz ---")

    hostname = "192.168.4.1"
    input_file = "none"

    # Handle the command line arguments
    for index, arg in enumerate(argv):
        if arg == "-h" or arg == "--help":
            print_help()
            sys.exit(0)
        elif arg == "-H" or arg == "--hostname":
            if index + 1 < len(argv) and re.search(ip_regex, argv[index + 1]):
                hostname = argv[index + 1]
                if not ping(hostname):
                    print("IP is not reachable:")
                    sys.exit(2)
            else:
                print("IP address is not right")
                print_help()
                sys.exit(1)
        elif arg == "-f" or arg == "--file":
            if index + 1 < len(argv) and os.path.isfile(argv[index + 1]):
                input_file = argv[index + 1]
            else:
                print("Can't open file")
                print_help()
                sys.exit(3)

    if input_file == "none":
        print("No input file")
        print_help()
        sys.exit(4)

    response = requests.post('http://' + hostname + '/pgmmega/sync')

    # ------------  GET AVR in SYNC  ----------------------------------------

    if response.status_code != 204:
        print("Failed to reset the AVR (HTML ERROR: " + response.status_code + ")")
        sys.exit(5)

    while True:
        response = requests.get('http://' + hostname + '/pgmmega/sync')

        if "SYNC" in response.content.decode('ASCII'):
            print(response.content)
            break
        elif "NOT READY" not in response.content.decode('ASCII'):
            print("Could not get in Sync with AVR")
            sys.exit(7)

        time.sleep(0.1)

    # -------------- Upload HEX file -----------------------------------------

    hex_file = open(input_file).read()

    response = requests.post('http://' + hostname + '/pgmmega/upload', data=hex_file, timeout=20.0)

    if "Success" in response.content.decode('ASCII'):
        print("+++++ Success :) ++++++")
    else:
        print("Failed :(")
        sys.exit(8)

    # Reset the avr to solve a bug in the bootloader that the program dows not start immediately
    time.sleep(0.1)
    requests.post('http://' + hostname + '/console/reset')

    sys.exit(0)


def print_help():
    print('\n')
    print("Usage: ")
    print("flash2560.py -H <hostname> -f <hex_file>")
    print("\nExample:")
    print("flash2560.py -H 192.168.4.1 -f Sketch.hex")


def ping(host):
    param = '-n' if platform.system().lower() == 'windows' else '-c'

    command = ['ping', param, '1', host]
    output = open(os.devnull, 'w')
    return subprocess.call(command, stdout=output) == 0


if __name__ == "__main__":
    main(sys.argv[1:])
