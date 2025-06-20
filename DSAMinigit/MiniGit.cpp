#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include "MiniGit.hpp"
#include <set>
#include <map>

MiniGit::MiniGit() {}

void MiniGit::createDirectory(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
        std::cout << "Created: " << path << "\n";
    }
}

void MiniGit::init() {
    std::cout << "Initializing MiniGit Repository...\n";

    createDirectory(repoPath);
    createDirectory(objectsPath);
    createDirectory(repoPath + "/refs");
    createDirectory(commitsPath);

    std::ofstream headFile(repoPath + "/HEAD");
    if (headFile) {
        headFile << "ref: refs/master\n";
        headFile.close();
        std::cout << "Initialized HEAD to master branch.\n";
    } else {
        std::cerr << "Failed to write HEAD file.\n";
    }
}

std::string MiniGit::readFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("File not found: " + filename);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string MiniGit::computeHash(const std::string& content) {
    size_t sum = 0;
    for (char c : content) sum += static_cast<unsigned char>(c);
    return std::to_string(sum); 
}

void MiniGit::writeBlob(const std::string& hash, const std::string& content) {
    std::string path = objectsPath + "/" + hash;
    std::ofstream out(path);
    if (out) {
        out << content;
        std::cout << "Saved blob: " << path << "\n";
    } else {
        std::cerr << "Failed to write blob file.\n";
    }
}

void MiniGit::add(const std::string& filename) {
    try {
        std::string content = readFile(filename);
        std::string hash = computeHash(content);

        if (stagedFiles.find(filename) == stagedFiles.end()) {
            stagedFiles.insert(filename);
            writeBlob(hash, content);
            std::cout << "Staged: " << filename << "\n";
        } else {
            std::cout << filename << " is already staged.\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Add failed: " << e.what() << "\n";
    }
}
void MiniGit::commit(const std::string& message) {
    if (stagedFiles.empty()) {
        std::cout << "No changes to commit.\n";
        return;
    }

    Commit commit;
    commit.message = message;
    commit.timestamp = std::time(nullptr);
    commit.parentHash = headHash;

    for (const auto& file : stagedFiles) {
        commit.stagedFiles.push_back(file);
    }

    commit.hash = generateCommitHash(commit);
    saveCommit(commit);
    headHash = commit.hash;

    stagedFiles.clear();

    std::cout << "Committed with hash: " << commit.hash << "\n";
}

void MiniGit::saveCommit(const Commit& commit) {
    std::string path = commitsPath + "/" + commit.hash;
    std::ofstream out(path);
    if (out) {
        out << "Message: " << commit.message << "\n";
        out << "Timestamp: " << commit.timestamp << "\n";
        out << "Parent: " << commit.parentHash << "\n";
        out << "Files:\n";
        for (const auto& f : commit.stagedFiles)
            out << f << "\n";
    } else {
        std::cerr << "Failed to save commit.\n";
    }

    std::ofstream headFile(repoPath + "/HEAD");
    if (headFile) {
        headFile << "ref: " << commit.hash << "\n";
    }
}

std::string MiniGit::generateCommitHash(const Commit& commit) {
    size_t sum = 0;
    sum += std::hash<std::string>{}(commit.message);
    sum += commit.timestamp;
    for (const auto& file : commit.stagedFiles)
        sum += std::hash<std::string>{}(file);
    return std::to_string(sum);
}
void MiniGit::log() {
    std::string currentHash = headHash;

    if (currentHash.empty()) {
        std::cout << "No commits yet.\n";
        return;
    }

    while (!currentHash.empty()) {
        std::string commitPath = commitsPath + "/" + currentHash;
        std::ifstream in(commitPath);
        if (!in) {
            std::cerr << "Failed to read commit: " << currentHash << "\n";
            break;
        }

        std::cout << "\n=== Commit " << currentHash << " ===\n";

        std::string line;
        std::string parentHash;

        while (std::getline(in, line)) {
            if (line.rfind("Parent:", 0) == 0) {
                parentHash = line.substr(7);
                parentHash.erase(0, parentHash.find_first_not_of(" \t"));
            }
            std::cout << line << "\n";
        }

        currentHash = parentHash;
    }
}
void MiniGit::branch(const std::string& name) {
    if (headHash.empty()) {
        std::cout << "No commits to branch from.\n";
        return;
    }

    std::string refPath = repoPath + "/refs/" + name;

    std::ofstream out(refPath);
    if (out) {
        out << headHash;
        std::cout << "Created branch '" << name << "' pointing to " << headHash << "\n";
    } else {
        std::cerr << "Failed to create branch.\n";
    }
}
void MiniGit::checkout(const std::string& branchName) {
    std::string refPath = repoPath + "/refs/" + branchName;

    std::ifstream refIn(refPath);
    if (!refIn) {
        std::cerr << "Branch not found: " << branchName << "\n";
        return;
    }

    std::string commitHash;
    std::getline(refIn, commitHash);

    std::string commitPath = commitsPath + "/" + commitHash;
    std::ifstream commitIn(commitPath);
    if (!commitIn) {
        std::cerr << "Commit not found: " << commitHash << "\n";
        return;
    }

    std::vector<std::string> files;
    std::string line;
    bool fileSection = false;
    while (std::getline(commitIn, line)) {
        if (line == "Files:") {
            fileSection = true;
            continue;
        }
        if (fileSection && !line.empty()) {
            files.push_back(line);
        }
    }

    for (const auto& file : files) {
        std::string content = readFile(objectsPath + "/" + computeHash(readFile(file)));
        std::ofstream out(file);
        if (out) {
            out << content;
            std::cout << "Restored: " << file << "\n";
        }
    }

    std::ofstream headOut(repoPath + "/HEAD");
    if (headOut) {
        headOut << "ref: refs/" << branchName << "\n";
    }

    headHash = commitHash;
    std::cout << "Switched to branch: " << branchName << "\n";
}
void MiniGit::merge(const std::string& branchName) {
    std::string targetRefPath = repoPath + "/refs/" + branchName;

    std::ifstream refIn(targetRefPath);
    if (!refIn) {
        std::cerr << "Branch not found: " << branchName << "\n";
        return;
    }

    std::string targetHash;
    std::getline(refIn, targetHash);
    std::string targetCommitPath = commitsPath + "/" + targetHash;

    std::ifstream targetIn(targetCommitPath);
    if (!targetIn) {
        std::cerr << "Target commit not found.\n";
        return;
    }

    std::vector<std::string> targetFiles;
    std::string line;
    bool inFiles = false;
    while (std::getline(targetIn, line)) {
        if (line == "Files:") {
            inFiles = true;
            continue;
        }
        if (inFiles) targetFiles.push_back(line);
    }
    for (const auto& file : targetFiles) {
        std::ifstream f(file);
        if (f) {
            std::cout << "CONFLICT: both modified " << file << "\n";
            std::ofstream out(file + ".conflict");
            out << "<<<<<<< HEAD\n";
            out << readFile(file) << "\n=======\n";
            out << readFile(objectsPath + "/" + computeHash(readFile(file))) << "\n>>>>>>>\n";
            std::cout << "Conflict written to " << file << ".conflict\n";
        } else {
            std::string blobPath = objectsPath + "/" + computeHash(readFile(file));
            std::string content = readFile(blobPath);
            std::ofstream out(file);
            out << content;
            std::cout << "Merged: " << file << "\n";
        }

        stagedFiles.insert(file);
    }

    commit("Merged branch: " + branchName);
}


void MiniGit::diff(const std::string& hash1, const std::string& hash2) {
    std::string path1 = commitsPath + "/" + hash1;
    std::string path2 = commitsPath + "/" + hash2;

    std::ifstream in1(path1), in2(path2);
    if (!in1 || !in2) {
        std::cerr << "One or both commit hashes are invalid.\n";
        return;
    }

    auto extractFiles = [](std::ifstream& in) {
        std::set<std::string> files;
        std::string line;
        bool inFiles = false;
        while (std::getline(in, line)) {
            if (line == "Files:") {
                inFiles = true;
                continue;
            }
            if (inFiles && !line.empty()) {
                files.insert(line);
            }
        }
        return files;
    };

    std::set<std::string> files1 = extractFiles(in1);
    std::set<std::string> files2 = extractFiles(in2);

    std::set<std::string> allFiles;
    allFiles.insert(files1.begin(), files1.end());
    allFiles.insert(files2.begin(), files2.end());

    for (const auto& file : allFiles) {
        bool in1 = files1.count(file);
        bool in2 = files2.count(file);

        std::cout << "\n=== File: " << file << " ===\n";

        if (in1 && !in2) {
            std::cout << "- File removed in commit2\n";
            continue;
        }
        if (!in1 && in2) {
            std::cout << "+ File added in commit2\n";
            continue;
        }

        std::string content1 = readFile(file);
        std::string content2 = readFile(file);

        std::istringstream ss1(content1);
        std::istringstream ss2(content2);
        std::string line1, line2;

        while (std::getline(ss1, line1) && std::getline(ss2, line2)) {
            if (line1 != line2) {
                std::cout << "- " << line1 << "\n";
                std::cout << "+ " << line2 << "\n";
            }
        }

        while (std::getline(ss1, line1))
            std::cout << "- " << line1 << "\n";
        while (std::getline(ss2, line2))
            std::cout << "+ " << line2 << "\n";
    }
}
