#include <iostream>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include "networkUtils.h"
#include "utils.h"
#include "md5.h"

// TODO: 1) check local files still work
//       2) check that uploading fully works
//       3) get downloading sorted (cvcs download <projectName> <saveID>?)

// General logging functions

template <typename T>
void error(T errMessage){
    std::cout << "[-] " << errMessage << std::endl;
}

template <typename T>
void log(T message){
    std::cout << "[!] " << message << std::endl;
}

std::string convertToServerPath(std::string projectName, std::string filePath) {
    std::stringstream ss(filePath);
    std::string currentSplit;
    std::string serverPath;
    bool projectNameFound = false;

    if(filePath.find_first_of('/') == std::string::npos){
        return "projects/" + projectName + "/" + filePath;
    }

    while(std::getline(ss, currentSplit, '/')){
        if(currentSplit.empty()){
            continue;
        }

        if(currentSplit == projectName){
            projectNameFound = true;
        }

        if(projectNameFound){
            if(!serverPath.empty()) {
                serverPath += '/';
            }
            serverPath += currentSplit;
        }
    }

    if(!projectNameFound){
        return "";
    }

    return "projects/" + serverPath;
}

// Function for getting the ID of the last save under a given project
// projectName -> project to get the last save ID from
int getLastSaveID(std::string projectName){
    int maxID = -1;

    // Iterate over the saves directory
    for(auto file : std::filesystem::directory_iterator(std::filesystem::current_path().string() + "/projects/" + projectName + "/saves")){
        std::string fileName = file.path().filename().string();

        try{
            int saveID = std::stoi(fileName);
            maxID = std::max(maxID, saveID);
        }catch(std::invalid_argument e){
            // Ignore non-integer filenames
        }
    }

    return maxID;
}

// Function for checking if a file has changed from previous save
// projectName -> name of project the file is a part of
// filePath -> path to file that is being checked
bool hasUploadedFileChanged(std::string projectName, std::string serverPath, std::string uploadedContent){
    std::string oldHash = "";
    std::string newHash = "";

    // Sort directories first to keep oldHash finding updated to the most recent hash
    std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

    for(auto saveDir : std::filesystem::directory_iterator("projects/" + projectName + "/saves/")){
        directoryEntries.push_back(saveDir);
    }

    // Sort according to the filename of the path, which is the save ID (otherwise would be lexographically)
    std::sort(directoryEntries.begin(), directoryEntries.end(), [](std::filesystem::__cxx11::directory_entry a, std::filesystem::__cxx11::directory_entry b){
        return std::stoi(a.path().filename().string()) < std::stoi(b.path().filename().string());
    });

    // Iterate over all change files in order
    for(auto saveDir : directoryEntries){
        std::string changesFilePath = saveDir.path().string() + "/.changes";

        std::ifstream changesFile(changesFilePath);

        // Get the most updated hash
        std::string line;
        while(std::getline(changesFile, line)){
            if(line == serverPath){
                std::getline(changesFile, oldHash);
                break;
            }
        }

        changesFile.close();
    }

    log("old hash: " + oldHash);

    if(uploadedContent == ""){
        // Hash of empty string
        newHash = "d41d8cd98f00b204e9800998ecf8427e";
    }else{
        // Calculate new hash
        newHash = md5(uploadedContent);
    }

    log("new hash: " + newHash);

    if(oldHash != newHash && oldHash != ""){
        return true;
    }

    if(oldHash == "" && newHash != ""){
        return true;
    }

    return false;
}

// Function for checking if a certain file has a beginning save entry
// projectName -> name of project that the file is a part of
// serverPath -> path to file to check
bool hasNoFullEntry(std::string projectName, std::string serverPath){
    for(auto saveDir : std::filesystem::directory_iterator("projects/" + projectName + "/saves/")){
        std::string changesFilePath = saveDir.path().string() + "/.changes"; 
    
        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            std::string line;
            while(std::getline(changesFile, line)){
                if(line == serverPath){
                    return false;
                }
            }
        }
    }

    return true;
}

// Function for rebuilding an old version of a file
// serverPath -> path of file to rebuild
// saveIDFinal -> the save ID to rebuild up to (and including)
std::string rebuildOldFile(std::string projectName, std::string serverPath, int saveIDFinal){
    if(saveIDFinal < 0){
        // Trying to rebuild when there isn't a previous save to rebuild from
        return "";
    }

    std::vector<std::string> oldFileSplit;
    std::vector<Change> changes;
    bool foundContent = false;

    // Sort so it applies changes in the correct order
    std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

    for(auto saveDir : std::filesystem::directory_iterator("projects/" + projectName + "/saves/")){
        directoryEntries.push_back(saveDir);
    }

    // Sort according to the filename of the path, which is the save ID (otherwise would be lexographically)
    std::sort(directoryEntries.begin(), directoryEntries.end(), [](std::filesystem::__cxx11::directory_entry a, std::filesystem::__cxx11::directory_entry b){
        return std::stoi(a.path().filename().string()) < std::stoi(b.path().filename().string());
    });

    // Iterate over the save directories in order
    for(auto saveDir : directoryEntries){
        // If we have gone past the saveIDFinal, break
        if(saveDir.path().string() == "projects/" + projectName + "/saves/" + std::to_string(saveIDFinal+1)){
            break;
        }

        std::string changesFilePath = saveDir.path().string() + "/.changes";
    
        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            std::string line;
            while(std::getline(changesFile, line)){
                if(line == serverPath){
                    if(!foundContent){
                        // Ignore hash
                        std::getline(changesFile, line);

                        std::string changesLine = "";
                        while(std::getline(changesFile, changesLine) && changesLine != "--------------------"){
                            int splitPos = changesLine.find_first_of(':');

                            // Store the content of the original save of the file
                            oldFileSplit.push_back(changesLine.substr(splitPos+1));
                        }

                        foundContent = true;

                        break;
                    
                    }else{
                        // Ignore hash
                        std::getline(changesFile, line);

                        std::string changesLine = "";
                        while(std::getline(changesFile, changesLine) && changesLine != "--------------------"){
                            int splitPos = changesLine.find_first_of(':');

                            // Store the change represented by the changesLine
                            Change change = {std::stoi(changesLine.substr(0, splitPos)), changesLine.substr(splitPos+1)};
                            changes.push_back(change);
                        }
                    }
                }
            }
        }
    }

    // Apply the changes
    for(Change change : changes){
        if(change.lineNum > oldFileSplit.size()){
            oldFileSplit.resize(change.lineNum);
        }

        oldFileSplit[change.lineNum-1] = change.lineContent;
    }

    std::string oldFileUpdated = reconstructSplitString(oldFileSplit);

    return oldFileUpdated;
}

int main(int argc, char* argv[]){
    log("Checking if projects directory exists");
    
    bool projectsFound = false;
    for(auto file : std::filesystem::directory_iterator(std::filesystem::current_path())){
        std::string fileName = file.path().filename().string();

        if(fileName == "projects"){
            projectsFound = true;
        }
    }

    if(!projectsFound){
        log("Creating projects directory");
        std::filesystem::create_directory("projects");
    }

    int serverSocketFD = socket(AF_INET, SOCK_STREAM, 0);

    char* ip = argv[1];

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(2956);
    inet_pton(AF_INET, ip, &serverAddress.sin_addr);

    bind(serverSocketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    listen(serverSocketFD, 10);

    log("Waiting for connections on " + std::string(ip) + ":2956");

    int clientSocketFD = accept(serverSocketFD, nullptr, nullptr);

    std::string command = receiveMessage(clientSocketFD);

    if(command == "upload"){
        std::string projectName = receiveMessage(clientSocketFD);
        int fileCount = std::stoi(receiveMessage(clientSocketFD));
        std::string saveMessage = receiveMessage(clientSocketFD);

        // Check if project exists
        bool foundProject = false;
        for(auto file : std::filesystem::directory_iterator("projects")){
            if(file.path().filename().string() == projectName){
                foundProject = true;
            }
        }

        if(!foundProject){
            // Set up the project file structure

            // Create project folder
            std::filesystem::create_directory("projects/" + projectName);
            
            // Create saves folder
            std::filesystem::create_directory("projects/" + projectName + "/saves");
        }

        int saveID = getLastSaveID(projectName) + 1;

        // Create current save directory
        std::filesystem::create_directory("projects/" + projectName + "/saves/" + std::to_string(saveID));

        // Create changes file
        std::ofstream changesFile("projects/" + projectName + "/saves/" + std::to_string(saveID) + "/.changes");

        // Create save file
        std::ofstream saveFile("projects/" + projectName + "/saves/" + std::to_string(saveID) + "/.save");

        log("Opening save file at " + ("projects/" + projectName) + "/saves/" + std::to_string(saveID) + "/.save");

        saveFile << getDateTime();
        saveFile << saveMessage;

        saveFile.close();

        for(int i=0; i<fileCount; i++){
            std::string filePath = receiveMessage(clientSocketFD);
            std::string uploadedContent = receiveMessage(clientSocketFD);

            log(filePath + " from project " + projectName + " has been uploaded");

            log("File path is: " + filePath);

            std::string serverPath = convertToServerPath(projectName, filePath);

            log("Server path is: " + serverPath);

            // Check if file has been changed
            if(hasUploadedFileChanged(projectName, serverPath, uploadedContent)){
                log("File has changed: " + serverPath);

                // Store path and hash of file
                changesFile << serverPath << std::endl;

                if(uploadedContent != ""){
                    changesFile << md5(uploadedContent) << std::endl;
                }else{
                    // Hash of empty string
                    changesFile << "d41d8cd98f00b204e9800998ecf8427e" << std::endl;
                }

                std::string rebuiltFile = rebuildOldFile(projectName, serverPath, getLastSaveID(projectName)-1);

                log("Rebuilt file: " + rebuiltFile);

                std::vector<std::string> changes = getChanges(rebuiltFile, uploadedContent);

                for(std::string change : changes){
                    changesFile << change << std::endl;
                }

                changesFile << "--------------------" << std::endl;

            }else if(hasNoFullEntry(projectName, serverPath)){
                log("File has no full entry (first time saving): " + serverPath);
            
                changesFile << serverPath << std::endl;
                changesFile << md5(uploadedContent) << std::endl;
                changesFile << uploadedContent << std::endl;
                changesFile << "--------------------" << std::endl;
            
            }

            changesFile.close();

            log("Saved successfully");
        }

    }else if(command == "download"){
        std::string projectName = receiveMessage(clientSocketFD);

        // Search for project and send over all project files
    
    }else if(command == "list"){
        // dummy for now
        sendMessage(clientSocketFD, "3");
        sendMessage(clientSocketFD, "project1");
        sendMessage(clientSocketFD, "project2");
        sendMessage(clientSocketFD, "project3");
    }

    close(serverSocketFD);

    return 0;
}