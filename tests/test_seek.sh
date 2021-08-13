#!/bin/bash

# Copyright 2021 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
#
# This file is under BSD-3-Clause license.
#
# This is a test for the seek_script

seek_script | grep "Best data" || { echo ERROR; exit 1; }
rm out.jpg
