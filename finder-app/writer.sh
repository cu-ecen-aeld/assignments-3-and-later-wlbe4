#!/bin/bash

# Check if the required arguments are provided
if [ $# -ne 2 ]; then
    echo "Error: Two arguments required - <file path> <write string>"
    exit 1
fi

# Assign arguments to variables
writefile=$1
writestr=$2

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Try to write to the file
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Could not create or write to file $writefile"
    exit 1
fi

echo "Successfully wrote to $writefile"
exit 0
