
#!/bin/bash
# Compile version_control_system.cpp using g++
g++ -g version_control_system.cpp -o version_control_system.exe
if [ $? -ne 0 ]; then
	echo "Compilation failed."
	exit 1
else
	echo "Compilation successful. Output: version_control_system.exe"
fi
