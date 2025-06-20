#ifndef MINIGIT_HPP
#define MINIGIT_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <ctime>

struct Commit {
    std::string hash;
    std::string message;
    std::time_t timestamp;
    std::vector<std::string> stagedFiles;
    std::string parentHash;
};
class MiniGit {
public:
    MiniGit();
    void init();
    void add(const std::string& filename);
    void commit(const std::string& message);
    void log();
    void branch(const std::string& name);
    void checkout(const std::string& branchName);
    void merge(const std::string& branchName); 
    bool hasConflict(const std::string& fileA, const std::string& fileB);
    void diff(const std::string& hash1, const std::string& hash2);




private:
    std::string repoPath = ".minigit";
    std::string objectsPath = ".minigit/objects";
    std::string commitsPath = ".minigit/commits";

    std::unordered_set<std::string> stagedFiles;
    std::string headHash = "";

    void createDirectory(const std::string& path);
    std::string computeHash(const std::string& content);
    std::string readFile(const std::string& filename);
    void writeBlob(const std::string& hash, const std::string& content);
    void saveCommit(const Commit& commit);
    std::string generateCommitHash(const Commit& commit);
};

#endif
