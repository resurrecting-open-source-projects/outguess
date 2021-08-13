#!/bin/bash

# Copyright 2021 Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
#
# This file is under BSD-3-Clause license.

# Write message
echo -s "\nEmbedding a message..."
outguess -k "secret-key-001" -d message.txt test.jpg test-with-message.jpg

# Retrieve message
echo -s "\nExtracting a message..."
outguess -k "secret-key-001" -r test-with-message.jpg text.txt
cat text.txt | grep "inside of the image" || { echo ERROR; exit 1; }

# Remove files
rm -f test-with-message.jpg text.txt
