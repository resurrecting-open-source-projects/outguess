#!/bin/bash

# Copyright 2021 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
#
# This file is under BSD-3-Clause license.

# Write message
echo -e "\nEmbedding a message..."
../src/outguess -k "secret-key-001" -d message.txt test.ppm test-with-message.ppm

# Retrieve message
echo -e "\nExtracting a message..."
../src/outguess -k "secret-key-001" -r test-with-message.ppm text.txt
cat text.txt | grep "inside of the image" || { echo ERROR; exit 1; }

# Remove files
rm -f test-with-message.ppm text.txt
