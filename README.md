Version Control System Inspired by Git

Notes and Features

Author: Manya Jain

------------------------------------------------------------
How to Compile and Run
------------------------------------------------------------

Windows:
1. Open a terminal in the assignment folder.
2. Run the provided batch file:
    compile_version_control_system.bat
   This will compile the code and create version_control_system.exe.
3. Run the program:
    version_control_system.exe
   (You can type commands interactively or redirect input from a file)

Linux/Mac:
1. Open a terminal in the assignment folder.
2. Use the provided shell script:
    compile_version_control_system.sh
   This will compile the code and create version_control_system.exe.
3. Make the script executable (if needed):
    chmod +x compile_version_control_system.sh
4. Run the script:
    ./compile_version_control_system.sh
5. Run the program:
    ./version_control_system.exe
   (You can type commands interactively or redirect input from a file)

Quickstart and Usage

This program is a simple in-memory versioned file system with a command-line interface (CLI).
Type commands one per line. All errors are handled and reported as:
    Error: <message>

------------------------------------------------------------
Input Types and Validation
------------------------------------------------------------
Commands:
    - Each command must be typed on a single line, with arguments separated by spaces.
    - Commands are case-sensitive and must match the documented format (e.g., CREATE, READ, INSERT).

Filenames:
    - Any non-empty string is allowed as a filename.
    - Filenames may contain special characters, or digits BUT must not contain spaces.
    - Filenames are case-sensitive.
    - Filenames must not be empty; empty names result in an error.

Content and Messages:
    - For INSERT, UPDATE, and SNAPSHOT, content or message can be any string (including spaces and special characters).
    - If content/message is omitted, an empty string is used.

Numeric Arguments:
    - RECENT_FILES, BIGGEST_TREES, and ROLLBACK accept non-negative integer arguments (e.g., number of files, version id).
    - If omitted, defaults are used (all files, previous version, etc.).
    - Invalid or negative numbers result in an error.

General Rules:
    - Arguments are separated by spaces; extra spaces are not ignored.
    - Unknown commands or missing/extra arguments are handled with error messages and do not crash the program.
    - All errors are reported to stderr in the format: Error: <message>
    - The program continues running after any error.

------------------------------------------------------------
Exception Handling and Error Reporting
------------------------------------------------------------
All errors (invalid arguments, file not found, unknown commands, etc.) are handled using C++ exceptions (try-catch blocks).
Whenever an error occurs, the program prints the error message to **stderr** in the format:
    Error: <message>
The program does not terminate or crash on invalid input; it continues to accept further commands.
This ensures robust CLI behavior and clear feedback for every user action.

Command Reference (in CLI order)

HELP
    - Shows the list of available commands and usage.
    - Output: Prints command list and usage notes.

EXIT
    - Exits the program cleanly.
    - Output: Prints "Exiting..." and terminates.

CREATE <filename>
    - Creates a new file with the given name.
    - Output: [CREATE] File created: <filename>
    - Errors:
        - Error: File name cannot be empty
        - Error: File already exists

READ <filename>
    - Prints the current content of the file.
    - Output: [READ] Content of file '<filename>': <content>
    - Errors:
        - Error: File name cannot be empty
        - Error: File not found

INSERT <filename> <content...>
    - Appends <content> to the file's current version. If omitted, inserts empty string.
    - Output: [INSERT] Content inserted into file '<filename>': <content>
             Current content: <content>
    - Errors:
        - Error: File name cannot be empty
        - Error: File not found

UPDATE <filename> <content...>
    - Replaces the file's current version content with <content>. If omitted, replaces with empty string.
    - Output: [UPDATE] Content updated in file '<filename>': <content>
             Current content: <content>
    - Errors:
        - Error: File name cannot be empty
        - Error: File not found

SNAPSHOT <filename> [message...]
    - Creates a snapshot of the current version, with optional message.
    - Output: [SNAPSHOT] Snapshot created for file '<filename>'. Message: <message>
    - Errors:
        - Error: File name cannot be empty
        - Error: File not found
        - Error: Current version is already a snapshot

ROLLBACK <filename> [version_id]
    - Rolls back to previous version or to the given version id.
    - Output: [ROLLBACK] File '<filename>' rolled back to previous/version <id>. Current content: <content>
    - Errors:
        - Error: File name cannot be empty
        - Error: File not found
        - Error: ROLLBACK command takes at most one argument
        - Error: ROLLBACK requires a non-negative integer version id
        - Error: Invalid version id for rollback
        - Error: No parent version to rollback to

HISTORY <filename>
    - Prints all snapshots for the file in chronological order (root to active).
    - Output: [HISTORY] Snapshots for file '<filename>':
             Version <id> | Created: <ts> | Snapshot: <ts> | Message: <msg>
             (no snapshots yet)
    - Errors:
        - Error: File name cannot be empty
        - Error: File not found

RECENT_FILES [k]
    - Lists the k most recently modified files. If k omitted, shows all.
    - Output: [RECENT_FILES] Showing k file(s): <filename> -> <timestamp>
    - Errors:
        - Error: RECENT_FILES requires a non-negative integer argument
        - Error: RECENT_FILES: requested number exceeds total files

BIGGEST_TREES [k]
    - Lists the k files with the largest number of versions. If k omitted, shows all.
    - Output: [BIGGEST_TREES] Showing k file(s) by version count: <filename> -> <number of versions>
    - Errors:
        - Error: BIGGEST_TREES requires a non-negative integer argument
        - Error: BIGGEST_TREES: requested number exceeds total files

Unknown command
    - Output: Error: Unknown command: <command>

------------------------------------------------------------
Notes
------------------------------------------------------------
- INSERT appends to the active content; UPDATE replaces it.
- You cannot modify an already snapshotted node; a new child version is created.
- HISTORY shows snapshots along the current branch (root -> active).
- RECENT_FILES and BIGGEST_TREES use max-heaps for efficient queries.
- All errors are handled and reported to stderr as "Error: <message>".
- Every command prints a clear output for user testing.
- Type HELP at any time to see this list of commands.
- Type EXIT to quit the CLI cleanly.

------------------------------------------------------------
Internal Data Structures
- TreeNode: Represents a version of a file, storing content, message, timestamps, parent, and children.
- File: Manages the version tree for a single file, supporting all file operations (read, insert, update, snapshot, rollback, history).
- Heaps: Used for efficiently listing files by recency (RECENT_FILES) and version count (BIGGEST_TREES).
- 3D Vector: Used to map filenames to File objects for O(1) access based on first and last character.

------------------------------------------------------------
Contact
------------------------------------------------------------
For any queries, contact: Manya Jain
Email: cs1240351@iitd.ac.in