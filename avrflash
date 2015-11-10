#! /bin/bash
#
# Flash an AVR with optiboot using the esp-link built-in programmer
# Basically we first reset the AVR and get in sync, and then send the hex file
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# Thorsten von Eicken wrote this file. As long as you retain 
# this notice you can do whatever you want with this stuff. If we meet some day, 
# and you think this stuff is worth it, you can buy me a beer in return. 
# ----------------------------------------------------------------------------

show_help() {
  cat <<EOT
Usage: ${0##*/} [-options...] hostname sketch.hex
Flash the AVR running optiboot attached to esp-link with the sketch.
  -v                    Be verbose
  -h                    show this help

Example: ${0##*/} -v esp-link mysketch.hex
         ${0##*/} 192.168.4.1 mysketch.hex
EOT
}

if ! which curl >/dev/null; then
  echo "ERROR: Cannot find curl: it is required for this script." >&2
  exit 1
fi

start=`date +%s`

# ===== Parse arguments

verbose=

while getopts "hvx:" opt; do
  case "$opt" in
    h) show_help; exit 0 ;;
    v) verbose=1 ;;
    x) foo="$OPTARG" ;;
    '?') show_help >&2; exit 1 ;;
  esac
done

# Shift off the options and optional --.
shift "$((OPTIND-1))"

# Get the fixed arguments
if [[ $# != 2 ]]; then
  show_help >&2
  exit 1
fi
hostname=$1
hex=$2

re='[-A-Za-z0-9.]+'
if [[ ! "$hostname" =~ $re ]]; then
  echo "ERROR: hostname ${hostname} is not a valid hostname or ip address" >&2
  exit 1
fi

if [[ ! -r "$hex" ]]; then
  echo "ERROR: cannot read hex file ($hex)" >&2
  exit 1
fi

# ===== Get AVR in sync

[[ -n "$verbose" ]] && echo "Resetting AVR with http://$hostname/pgm/sync" >&2
v=; [[ -n "$verbose" ]] && v=-v
sync=`curl -m 10 $v -s -w '%{http_code}' -XPOST "http://$hostname/pgm/sync"`
if [[ $? != 0 || "$sync" != 204 ]]; then
  echo "Error resetting AVR" >&2
  exit 1
fi

while true; do
  sync=`curl -m 10 $v -s "http://$hostname/pgm/sync"`
  if [[ $? != 0 ]]; then
    echo "Error checking sync" >&2
    exit 1
  fi
  case "$sync" in
  SYNC*)
    echo "AVR in $sync"  >&2
    break;;
  "NOT READY"*)
    [[ -n "$verbose" ]] && echo "  Waiting for sync..." >&2
    ;;
  *)
    echo "Error checking sync: $sync" >&2
    exit 1
    ;;
  esac
  sleep 0.1
done

# ===== Send HEX file

[[ -n "$verbose" ]] && echo "Sending HEX file for programming" >&2
sync=`curl -m 10 $v -s -g -d "@$hex" "http://$hostname/pgm/upload"`
echo $sync
if [[ $? != 0 || ! "$sync" =~ ^Success ]]; then
  echo "Error programming AVR" >&2
  exit 1
fi

sec=$(( `date +%s` - $start ))
echo "Success, took $sec seconds" >&2
exit 0
