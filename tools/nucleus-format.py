#!/usr/bin/python

import os
import re

# Directories
NUCLEUS_DIR = os.path.abspath("..")
NUCLEUS_PROJECT = os.path.join(NUCLEUS_DIR, "nucleus")
NUCLEUS_SHADERS = os.path.join(NUCLEUS_DIR, "resources", "shaders")
NUCLEUS_TESTS = os.path.join(NUCLEUS_DIR, "tests", "unit")


# Formatting rules for *.c, *.cpp and *.h files
def formatGeneric(codeInput):

    # Replace LF with CRLF
    codeInput = re.sub(r'^[\r]\n', r'\r\n', codeInput)

    # Replace tabs with 4 spaces
    codeInput = re.sub(r'\t', r'    ', codeInput)

    # Remove tabs or spaces at the end of lines
    codeInput = re.sub(r'([ \t]+)\r', r'\r', codeInput)
    
    return codeInput


# Search and edit files of the Nucleus project
def main():
    included = (".c", ".cc", ".cpp", ".h", ".hpp", ".glsl")
    excluded = (".l.cpp", ".y.cpp", ".y.hpp")
    for path in [NUCLEUS_PROJECT, NUCLEUS_SHADERS, NUCLEUS_TESTS]:
        for root, dirs, files in os.walk(path):
            for filename in files:
                if not filename.endswith(included):
                    continue
                if filename.endswith(excluded):
                    continue

                # Read and format the code
                f = open(os.path.join(root, filename), "rb")
                codeInput = f.read()
                codeOutput = formatGeneric(codeInput)
                f.close()

                # Update file if necessary
                if codeInput != codeOutput:
                    w = open(os.path.join(root, filename), "wb")
                    w.write(codeOutput)
                    w.close()


if __name__ == "__main__":
    main()
