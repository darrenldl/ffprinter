#!/bin/bash

if [[ $# == 0 ]]; then
    echo "Please specify a file name for log file"
    exit
fi

tee -a "$1" | ./../build/ffprinter | tee -a "$1"
