#!/bin/bash

echo "Building hatlag..."
cd hatlag
./build.sh

echo "Building hatser..."
cd ../hatser
./build.sh
