
#!/bin/bash
# Compile long_assignment_1.cpp using g++
g++ -g long_assignment_1.cpp -o long_assignment_1.exe
if [ $? -ne 0 ]; then
	echo "Compilation failed."
	exit 1
else
	echo "Compilation successful. Output: long_assignment_1.exe"
fi
