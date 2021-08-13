#!/bin/bash

# Copyright 2021 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
#
# This file is under BSD-3-Clause license.
#
# This is a test for the seek_script

echo "This is my test" > /tmp/fortune

../src/seek_script | grep "Best data" || { echo ERROR; exit 1; }
