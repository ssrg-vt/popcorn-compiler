#!/bin/sh

echo "switching to popcorn ns!"
echo 0 > /proc/popcorn
exec bash

