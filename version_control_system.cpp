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

// Helper function to join command arguments after the second index (for content/message)
// Used for INSERT, UPDATE, SNAPSHOT commands to get the full string after the command and filename
string connector(const vector<string>& a){
    if(a.size() <= 2) return string("");
    string s = a[2];
    for(size_t i = 3; i < a.size(); ++i){ s += ' ' + a[i]; }
    return s;
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
        int recent_index; // Index in recent files heap (for RECENT_FILES command)
        int version_index; // Index in version count heap (for BIGGEST_TREES command)
        File() : total_versions(1), last_modification(time(0)), recent_index(-1), version_index(-1) {
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

// --- Max Heap utilities for sorting files by version count or recency ---
// Used for BIGGEST_TREES and RECENT_FILES commands
int parent(int i) { return (i - 1) / 2; }
int l_child(int i) { return 2 * i + 1; }
int r_child(int i) { return 2 * i + 2; }

void change(vector<pair<int,string>>& a, int i, int j){
    string name = a[i].second;
    int k = (unsigned char)name.front();
    int l = (unsigned char)name.back();
    File* z = nullptr;
    for(auto &p : all_files[k][l]) if(p.first == name) { z = p.second; break; }
    z->version_index = j;
}

// Heapify up for version count heap
void shiftup_int(vector<pair<int,string>> & a, int i){
    while( i != 0 && a[i].first > a[parent(i)].first){
        change(a,parent(i),i);
        change(a,i,parent(i));
        swap(a[i],a[parent(i)]);
        i = parent(i);
    }
}

// Insert into version count heap
void insert_int(vector<pair<int,string>> & a, int value, string s){
    a.push_back({value,s});
    change(a,a.size()-1,a.size()-1);
    shiftup_int(a,a.size()-1);
}

// Heapify down for version count heap
void shiftdown_int(vector<pair<int,string>> & a, int i){
    while (l_child(i) < a.size()-1 && (a[i].first < a[l_child(i)].first || a[i].first < a[r_child(i)].first)){
        if (a[l_child(i)].first < a[r_child(i)].first ){change(a,r_child(i),i); change(a,i,r_child(i)); swap(a[i],a[r_child(i)]); i = r_child(i);}
        else {change(a,l_child(i),i); change(a,i,l_child(i)); swap(a[i],a[l_child(i)]); i = l_child(i);}
    }
    if (l_child(i) == a.size()-1 && a[i].first < a[l_child(i)].first){change(a,l_child(i),i); change(a,i,l_child(i)); swap(a[i],a[l_child(i)]); i = l_child(i);}
}
void shiftdown_int1(vector<pair<int,string>> & a, int i){
    while (l_child(i) < a.size()-1 && (a[i].first < a[l_child(i)].first || a[i].first < a[r_child(i)].first)){
        if (a[l_child(i)].first < a[r_child(i)].first ){swap(a[i],a[r_child(i)]); i = r_child(i);}
        else {swap(a[i],a[l_child(i)]); i = l_child(i);}
    }
    if (l_child(i) == a.size()-1 && a[i].first < a[l_child(i)].first){swap(a[i],a[l_child(i)]); i = l_child(i);}
}

// Extract max from version count heap
pair<int,string> getmax_int(vector<pair<int,string>> & a){
    int ver = a[0].first;
    string temp = a[0].second;
    swap(a[0],a[a.size()-1]);
    a.pop_back();
    shiftdown_int1(a,0);
    return {ver,temp};
}

void changet(vector<pair<time_t,string>>& a, int i, int j){
    string name = a[i].second;
    int k = (unsigned char)name.front();
    int l = (unsigned char)name.back();
    File* z = nullptr;
    for(auto &p : all_files[k][l]) if(p.first == name) { z = p.second; break; }
    z->recent_index = j;
}
// Heapify up for recency heap
void shiftup_time(vector<pair<time_t,string>> & a, int i){
    while( i != 0 && a[i].first > a[parent(i)].first){
        changet(a,parent(i),i);
        changet(a,i,parent(i));
        swap(a[i],a[parent(i)]);
        i = parent(i);
    }
}

// Insert into recency heap
void insert_time(vector<pair<time_t,string>> & a, time_t time, string s){
    a.push_back({time,s});
    changet(a,a.size()-1,a.size()-1);
    shiftup_time(a,a.size()-1);
}

// Heapify down for recency heap
void shiftdown_time(vector<pair<time_t,string>> & a, int i){
    while (l_child(i) < a.size()-1 && (a[i].first < a[l_child(i)].first || a[i].first < a[r_child(i)].first)){
        if (a[l_child(i)].first < a[r_child(i)].first ){changet(a,r_child(i),i); changet(a,i,r_child(i)); swap(a[i],a[r_child(i)]); i = r_child(i);}
        else {changet(a,l_child(i),i); changet(a,i,l_child(i)); swap(a[i],a[l_child(i)]); i = l_child(i);}
    }
    if (l_child(i) == a.size()-1 && a[i].first < a[l_child(i)].first){changet(a,l_child(i),i); changet(a,i,l_child(i)); swap(a[i],a[l_child(i)]); i = l_child(i);}
}
void shiftdown_time1(vector<pair<time_t,string>> & a, int i){
    while (l_child(i) < a.size()-1 && (a[i].first < a[l_child(i)].first || a[i].first < a[r_child(i)].first)){
        if (a[l_child(i)].first < a[r_child(i)].first ){swap(a[i],a[r_child(i)]); i = r_child(i);}
        else {swap(a[i],a[l_child(i)]); i = l_child(i);}
    }
    if (l_child(i) == a.size()-1 && a[i].first < a[l_child(i)].first){swap(a[i],a[l_child(i)]); i = l_child(i);}
}

// Extract max from recency heap
pair<string,time_t> getmax_time(vector<pair<time_t,string>> & a){
    string temp = a[0].second;
    time_t time = a[0].first;
    swap(a[0],a[a.size()-1]);
    a.pop_back();
    shiftdown_time1(a,0);
    return {temp,time};
}

// Remove a file by filename from recent_files_heap
void remove_from_recent_heap(vector<pair<time_t,string>>& heap, const string& filename) {
    int pos = -1;

    int i = (unsigned char)filename.front();
    int j = (unsigned char)filename.back();
    File* z = nullptr;
    for(auto &p : all_files[i][j]) if(p.first == filename) { z = p.second; pos = z->recent_index; break; }
    if (pos == -1) return; // not present
    changet(heap,heap.size()-1,pos);
    changet(heap,pos,heap.size()-1);
    swap(heap[pos], heap.back());
    heap.pop_back();
    if (pos < heap.size()) {
    // Repair heap property in both directions to be safe
    shiftup_time(heap, pos);
    shiftdown_time(heap, pos);
    }
}

//Remove a file by filename from version_heap
void remove_from_version_heap(vector<pair<int,string>>& heap, const string& filename) {
    int pos = -1;    
    int i = (unsigned char)filename.front();
    int j = (unsigned char)filename.back();
    File* z = nullptr;
    for(auto &p : all_files[i][j]) if(p.first == filename) { z = p.second; pos = z->version_index; break; }
    if (pos == -1) return; // not present
    change(heap,heap.size()-1,pos);
    change(heap,pos,heap.size()-1);
    swap(heap[pos], heap.back());
    heap.pop_back();
    if (pos < heap.size()) {
    // Repair heap property in both directions to be safe
    shiftup_int(heap, pos);
    shiftdown_int(heap, pos);
    }
}

int main()
{

    // Heap for BIGGEST_TREES (by version count)
    vector<pair<int,string>> version_heap;

    // Heap for RECENT_FILES (by last modification time)
    vector<pair<time_t,string>> recent_files_heap;

    vector<pair<int,string>> version_heap_copy;
    vector<pair<time_t,string>> recent_files_heap_copy;

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

                // Update heaps
                remove_from_recent_heap(recent_files_heap, name);
                insert_time(recent_files_heap, f->last_ts(), name);
                remove_from_version_heap(version_heap, name);
                insert_int(version_heap, f->total_ver(), name);

                cout << "[CREATE] File created: " << name << endl;
                cout << endl;
            }

            // RECENT_FILES: list files by most recent modification
            else if( command[0] == "RECENT_FILES") {
                recent_files_heap_copy.clear();
                recent_files_heap_copy.insert(recent_files_heap_copy.end(), recent_files_heap.begin(), recent_files_heap.end());
                int num;

                if(command.size() == 1) {
                    num = recent_files_heap.size();
                } else {
                    if(!is_nonneg_integer(command[1])) throw invalid_argument("RECENT_FILES requires a non-negative integer argument");
                    num = stoi(command[1]);
                }
                if(num > recent_files_heap.size()) throw invalid_argument("RECENT_FILES: requested number exceeds total files");

                cout << "[RECENT_FILES] Showing " << num << " file(s):" << endl;
                for(int k = 0; k < num && !recent_files_heap_copy.empty(); ++k){
                    auto pr = getmax_time(recent_files_heap_copy);
                    string name = pr.first;
                    time_t t = pr.second;
                    cout << name << " -> " << format_time(t) << endl;
                }
                cout << endl;
                
            }

            // BIGGEST_TREES: list files by number of versions (largest first)
            else if( command[0] == "BIGGEST_TREES") {
                version_heap_copy.clear();
                version_heap_copy.insert(version_heap_copy.end(), version_heap.begin(), version_heap.end());
                int num;
                if(command.size() == 1) {
                    num = version_heap.size();
                } else {
                    if(!is_nonneg_integer(command[1])) throw invalid_argument("BIGGEST_TREES requires a non-negative integer argument");
                    num = stoi(command[1]);
                }
                if(num > version_heap.size()) throw invalid_argument("BIGGEST_TREES: requested number exceeds total files");
                cout << "[BIGGEST_TREES] Showing " << num << " file(s) by version count:" << endl;
                for(int k = 0; k < num && !version_heap_copy.empty(); ++k){
                    auto pr = getmax_int(version_heap_copy);
                    cout << pr.first << " -> " << pr.second << endl;
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
                    string content = (command.size() == 2) ? "" : connector(command);
                    z->insert(content);
                    cout << "[INSERT] Content inserted into file '" << name << "':\n" << content << endl;
                    cout << "Current content:\n" << z->read() << endl;
                    // Update heaps
                    remove_from_recent_heap(recent_files_heap, name);
                    insert_time(recent_files_heap, time(0), name);
                    remove_from_version_heap(version_heap, name);
                    insert_int(version_heap, z->total_ver(), name);
                    cout << endl;
                }

                // UPDATE <filename> <content>: replace file content
                else if(command[0] == "UPDATE"){
                    string content = (command.size() == 2) ? "" : connector(command);
                    z->update(content);
                    cout << "[UPDATE] Content updated in file '" << name << "':\n" << content << endl;
                    cout << "Current content:\n" << z->read() << endl;
                    // Update heaps
                    remove_from_recent_heap(recent_files_heap, name);
                    insert_time(recent_files_heap, time(0), name);
                    remove_from_version_heap(version_heap, name);
                    insert_int(version_heap, z->total_ver(), name);
                    cout << endl;
                }

                // SNAPSHOT <filename> <message>: create a snapshot with optional message
                else if(command[0] == "SNAPSHOT"){
                    string message = (command.size() == 2) ? "" : connector(command);
                    z->snapshot(message);
                    cout << "[SNAPSHOT] Snapshot created for file '" << name << "'." << endl;
                    if(!message.empty()) cout << "Message: " << message << endl;
                    // Update heaps
                    remove_from_recent_heap(recent_files_heap, name);
                    insert_time(recent_files_heap, time(0), name);
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
                    // Update recent_files_heap only
                    remove_from_recent_heap(recent_files_heap, name);
                    insert_time(recent_files_heap, time(0), name);
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