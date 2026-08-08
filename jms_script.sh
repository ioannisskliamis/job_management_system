#!/bin/bash

# Argument parsing
while getopts "l:c:" opt; do
  case $opt in
    l) PATH_DIR="$OPTARG" ;;
    c) CMD="$OPTARG" ;;
    *) echo "Usage: $0 -l path -c command [n]" ; exit 1 ;;
  esac
done

shift $((OPTIND - 1))
N=$1

if [ -z "$PATH_DIR" ] || [ -z "$CMD" ]; then
  echo "Missing arguments"
  exit 1
fi

if [ ! -d "$PATH_DIR" ]; then
  echo "Directory does not exist: $PATH_DIR"
  exit 1
fi

cd "$PATH_DIR" || exit 1

# List
if [[ "$CMD" == "list" ]]; then
  ls -d outputs_* 2>/dev/null
  exit 0
fi

# Size
if [[ "$CMD" == "size" ]]; then
  if [ -z "$N" ]; then
    du -sh outputs_* 2>/dev/null | sort -h
  else
    du -sh outputs_* 2>/dev/null | sort -h | tail -n "$N"
  fi
  exit 0
fi

# Purge
if [[ "$CMD" == "purge" ]]; then
  find . -maxdepth 1 -type d -name "outputs_*" -exec rm -rf {} +
  echo "All job directories deleted!"
  exit 0
fi


echo "Unknown command"
exit 1