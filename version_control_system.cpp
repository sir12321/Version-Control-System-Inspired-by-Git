//
// Simple in-memory versioned file system for a CLI assignment.
//
// Commands (type one per line):
//   CREATE <filename>
//   READ <filename>
//   INSERT <filename> <content...>
//   UPDATE <filename> <content...>
//   SNAPSHOT <filename> [message...]
//   ROLLBACK <filename> [version_id]
//   HISTORY <filename>
//   RECENT_FILES [k]
//   BIGGEST_TREES [k]
//   HELP
//   EXIT
//
// Notes:
// - INSERT appends to the active content; UPDATE replaces it.
// - You cannot modify an already snapshotted node; a new child version is created.
// - HISTORY shows snapshots along the current branch (root -> active).
// - Errors are reported to stderr as: "Error: <message>".
//

#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <stdexcept>
#include <queue>
using namespace std;

// Helper: check whether a string represents a non-negative integer
bool is_nonneg_integer(const string &s){
    if(s.empty()) return false;
    for(char c : s){ if(c < '0' || c > '9') return false; }
    return true;
}

// Function to split a string by spaces into a vector of words (command parser)
// Example: "INSERT file1 Hello World" -> ["INSERT", "file1", "Hello", "World"]
// This helps in parsing user commands from input
vector<string> separate(string s){
    vector<string> result;
    string token;
    int count = 0;
    for(char c : s){
        bool ws = (c == ' ' || c == '\t' || c == '\n' );
        if(ws && count != 2){
            if(!token.empty()){ result.push_back(token); token.clear(); count ++;}
        } else {
            token.push_back(c);
        }
    }
    if(!token.empty()) result.push_back(token);
    return result;
}

// Helper to format time_t to human readable string without trailing newline
string format_time(time_t t){
    string s = ctime(&t);
    if(!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

// TreeNode represents a version of a file in the version tree
// Each node stores content, message, timestamps, parent, and children
class TreeNode
{
    public:
        int version_id; // Unique version identifier
        string content; // File content at this version
        string message; // Snapshot message (if any)
        time_t created_ts; // Creation timestamp
        time_t snapshot_ts; // Snapshot timestamp (0 if not a snapshot)
        TreeNode* parent; // Pointer to parent version
        vector<TreeNode*> children; // Children versions
        TreeNode(int id) : content(""), message(""), created_ts(time(0)), version_id(id), snapshot_ts(0), parent(nullptr) {}
};

// File class manages the version tree for a single file
// Supports operations: read, insert, update, snapshot, rollback, history
class File
{
    private:
        TreeNode* active_version; // Current version pointer
        TreeNode* root; // Root version (initial)
        int total_versions; // Total number of versions
        vector<TreeNode*> version_map;
        time_t last_modification; // Last modification timestamp

    public:
        File() : total_versions(1), last_modification(time(0)) {
            root = new TreeNode(0);
            active_version = root;
            version_map.push_back(root);
            snapshot("");
        }

        // Get last modification timestamp
        time_t last_ts(){return last_modification;}

        // Get total number of versions
        int total_ver(){return total_versions;}

        // Read content of current version
        string read(){return active_version->content;}

        // Insert content at current version (appends if not a snapshot, else creates new version)
        void insert(string content){
            last_modification = time(0);
            if (active_version->snapshot_ts == 0){
                active_version->content += content;
            }
            else{
                TreeNode* nd = new TreeNode(total_versions);
                total_versions ++;
                nd->content = active_version->content + content;
                active_version->children.push_back(nd);
                nd->parent = active_version;
                active_version = nd;
                version_map.push_back(active_version);
            }
        }

        // Update content at current version (replaces if not a snapshot, else creates new version)
        void update(string content){
            last_modification = time(0);
            if (active_version->snapshot_ts == 0){
                active_version->content = content;
            }
            else{
                TreeNode* nd = new TreeNode(total_versions );
                total_versions ++;
                nd->content = content;
                active_version->children.push_back(nd);
                nd->parent = active_version;
                active_version = nd;
                version_map.push_back(active_version);
            }
        }

        // Create a snapshot at current version with optional message
        void snapshot(string mess = ""){
            if(active_version->snapshot_ts != 0) {
                throw runtime_error("Current version is already a snapshot");
            }
            last_modification = time(0);
            active_version -> snapshot_ts = time(0);
            active_version -> message = mess;
        }

        // Rollback to a previous version by id, or to parent if no id given
        void rollback(int id = -1){
            if(id != -1) {
                if(id < 0 || id >= total_versions) {
                    throw out_of_range("Invalid version id for rollback");
                }
                active_version = version_map[id];
            } else {
                if(active_version->parent == nullptr) {
                    throw runtime_error("No parent version to rollback to");
                }
                active_version = active_version->parent;
            }
        }

    // Print history of all snapshots along the current branch (root -> active).
    // Returns the count of snapshot entries printed.
    int history(){
            vector<TreeNode*> path;
            TreeNode* curr = active_version;
            while(curr != nullptr){ path.push_back(curr); curr = curr->parent; }
            // path currently active->root, reverse to root->active
        int count = 0;
            for(auto it = path.rbegin(); it != path.rend(); ++it){
                TreeNode* node = *it;
                if(node->snapshot_ts != 0){
                    cout << "Version " << node->version_id << endl
                         << " | Created: " << format_time(node->created_ts)
                         << " | Snapshot: " << format_time(node->snapshot_ts)
                         << " | Message: " << node->message << endl;
            ++count;
                }
            }
        return count;
        }
};

// 3D vector to store files: first char ASCII, last char ASCII, list of (filename, File*)
// Allow full 8-bit character set for first/last chars safely (0..255)
vector<vector<vector<pair<string,File*>>>> all_files(256, vector<vector<pair<string,File*>>>(256));

int main()
{

    // Heap for BIGGEST_TREES (by version count)
    vector<pair<int,string>> version_heap;

    // Heap for RECENT_FILES (by last modification time)
    vector<pair<time_t,string>> recent_files_heap;

    while (true)
    {
        try {
            // Read user input (command)
            string input;
            if(!getline(cin,input)) {
                // Graceful exit on EOF (e.g., Ctrl+Z in Windows console)
                break;
            }
            vector<string> command = separate(input);

            if (command.empty()) continue;

            // HELP: show usage
            if(command[0] == "HELP"){
                cout << "Available commands:\n"
                     << "  CREATE <filename>\n"
                     << "  READ <filename>\n"
                     << "  INSERT <filename> <content...>\n"
                     << "  UPDATE <filename> <content...>\n"
                     << "  SNAPSHOT <filename> [message...]\n"
                     << "  ROLLBACK <filename> [version_id]\n"
                     << "  HISTORY <filename>\n"
                     << "  RECENT_FILES [k]\n"
                     << "  BIGGEST_TREES [k]\n"
                     << "  HELP\n"
                     << "  EXIT\n";
                cout << endl;
                continue;
            }
            // EXIT: terminate program
            if(command[0] == "EXIT"){
                cout << "Exiting..." << endl;
                break;
            }

            // CREATE <filename>: create a new file
            if (command[0] == "CREATE"){
                if(command.size() < 2) throw std::invalid_argument("CREATE command requires a file name");
                string name = command[1];
                if(name.empty()) throw invalid_argument("File name cannot be empty");
                int i = (unsigned char)name.front();
                int j = (unsigned char)name.back();
                for (auto &p : all_files[i][j]) {
                    if (p.first == name) throw invalid_argument("File already exists");
                }
                File* f = new File();
                all_files[i][j].push_back({name, f});

                cout << "[CREATE] File created: " << name << endl;
                cout << endl;
            }

            // RECENT_FILES: list files by most recent modification
            else if( command[0] == "RECENT_FILES") {
                using T = pair<time_t, string>;
                auto cmp = [](const T& a, const T& b) { return a.first < b.first; };
                priority_queue<T, vector<T>, decltype(cmp)> pq(cmp);
                for(int i = 0; i < 256; ++i) {
                    for(int j = 0; j < 256; ++j) {
                        for(auto &p : all_files[i][j]) {
                            File* f = p.second;
                            pq.push({f->last_ts(), p.first});
                        }
                    }
                }
                int num = pq.size();
                if(command.size() > 1) {
                    if(!is_nonneg_integer(command[1])) throw invalid_argument("RECENT_FILES requires a non-negative integer argument");
                    num = stoi(command[1]);
                }
                if(num > (int)pq.size()) throw invalid_argument("RECENT_FILES: requested number exceeds total files");
                cout << "[RECENT_FILES] Showing " << num << " file(s):" << endl;
                for(int k = 0; k < num && !pq.empty(); ++k){
                    auto pr = pq.top(); pq.pop();
                    cout << pr.second << " -> " << format_time(pr.first) << endl;
                }
                cout << endl;
            }

            // BIGGEST_TREES: list files by number of versions (largest first)
            else if( command[0] == "BIGGEST_TREES") {
                using T = pair<int, string>;
                auto cmp = [](const T& a, const T& b) { return a.first < b.first; };
                priority_queue<T, vector<T>, decltype(cmp)> pq(cmp);
                for(int i = 0; i < 256; ++i) {
                    for(int j = 0; j < 256; ++j) {
                        for(auto &p : all_files[i][j]) {
                            File* f = p.second;
                            pq.push({f->total_ver(), p.first});
                        }
                    }
                }
                int num = pq.size();
                if(command.size() > 1) {
                    if(!is_nonneg_integer(command[1])) throw invalid_argument("BIGGEST_TREES requires a non-negative integer argument");
                    num = stoi(command[1]);
                }
                if(num > (int)pq.size()) throw invalid_argument("BIGGEST_TREES: requested number exceeds total files");
                cout << "[BIGGEST_TREES] Showing " << num << " file(s) by version count:" << endl;
                for(int k = 0; k < num && !pq.empty(); ++k){
                    auto pr = pq.top(); pq.pop();
                    cout << pr.second << " -> " << pr.first << endl;
                }
                cout << endl;
            }

            // File-specific commands: READ, INSERT, UPDATE, SNAPSHOT, ROLLBACK, HISTORY
            else if(command[0] == "READ" || command[0] == "INSERT" || command[0] == "UPDATE" || command[0] == "SNAPSHOT" || command[0] == "ROLLBACK" || command[0] == "HISTORY"){
                if(command.size() < 2) throw invalid_argument("Command requires a file name");

                string name = command[1];
                if(name.empty()) throw invalid_argument("File name cannot be empty");
                int i = (unsigned char)name.front();
                int j = (unsigned char)name.back();
                File* z = nullptr;
                for(auto &p : all_files[i][j]) if(p.first == name) { z = p.second; break; }
                if(z == nullptr) throw runtime_error("File not found");
                
                // READ <filename>: print file content
                if(command[0] == "READ"){
                    cout << "[READ] Content of file '" << name << "':\n";
                    cout << z->read() << endl;
                    cout << endl;
                }

                // INSERT <filename> <content>: append content to file
                else if(command[0] == "INSERT"){
                    string content = (command.size() == 2) ? "" : command[2];
                    z->insert(content);
                    cout << "[INSERT] Content inserted into file '" << name << "':\n" << content << endl;
                    cout << "Current content:\n" << z->read() << endl;
                    cout << endl;
                }

                // UPDATE <filename> <content>: replace file content
                else if(command[0] == "UPDATE"){
                    string content = (command.size() == 2) ? "" : command[2];
                    z->update(content);
                    cout << "[UPDATE] Content updated in file '" << name << "':\n" << content << endl;
                    cout << "Current content:\n" << z->read() << endl;
                    cout << endl;
                }

                // SNAPSHOT <filename> <message>: create a snapshot with optional message
                else if(command[0] == "SNAPSHOT"){
                    string message = (command.size() == 2) ? "" : command[2];
                    z->snapshot(message);
                    cout << "[SNAPSHOT] Snapshot created for file '" << name << "'." << endl;
                    if(!message.empty()) cout << "Message: " << message << endl;
                    cout << endl;
                }

                // ROLLBACK <filename> [version_id]: rollback to previous version or to given version id
                else if(command[0] == "ROLLBACK"){
                    if(command.size() > 3) throw invalid_argument("ROLLBACK command takes at most one argument");
                    if(command.size() == 2){
                        z->rollback();
                        cout << "[ROLLBACK] File '" << name << "' rolled back to previous version." << endl;
                        cout << "Current content:\n" << z->read() << endl;
                        cout << endl;
                    }
                    else{
                        if(!is_nonneg_integer(command[2])) throw invalid_argument("ROLLBACK requires a non-negative integer version id");
                        int ver = stoi(command[2]);
                        z->rollback(ver);
                        cout << "[ROLLBACK] File '" << name << "' rolled back to version " << ver << "." << endl;
                        cout << "Current content:\n" << z->read() << endl;
                        cout << endl;
                    }
                }

                // HISTORY <filename>: print all snapshots for the file
                else if(command[0] == "HISTORY"){
                    cout << "[HISTORY] Snapshots for file '" << name << "':" << endl;
                    int cnt = z->history();
                    if(cnt == 0){
                        cout << "(no snapshots yet)" << endl;
                    }
                    cout << endl;
                }
            }

            // Unknown command handler
            else{
                    throw invalid_argument("Unknown command: " + command[0]);
                }

    } catch(const exception& e) {
            cerr << "Error: " << e.what() << endl;
            cout << endl;
        }
    }
    return 0;
}