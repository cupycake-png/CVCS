#include <iostream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>

#include "md5.h"
#include "networkUtils.h"
#include "utils.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2956

// General logging functions

template <typename T>
void error(T errMessage){
    std::cout << "[-] " << errMessage << std::endl;
}

template <typename T>
void log(T message){
    std::cout << "[!] " << message << std::endl;
}

// Function for initialising cvcs
// directory -> directory to initialise in
int initialise(std::string directory){
    try{
        for(auto file : std::filesystem::directory_iterator(directory)){
            std::filesystem::path path = file.path();
            std::string fileName = path.filename().string();

            if(fileName == ".cupy"){
                throw -7;
            }
        }

        std::filesystem::create_directory(directory + "/.cupy");
        std::filesystem::create_directory(directory + "/.cupy/saves");

        return 0;

    }catch(std::filesystem::filesystem_error err){
        int errCode = err.code().value();

        if(errCode == 2){
            log("Directory not found, creating directory");
            
            std::filesystem::create_directory(directory);
            std::filesystem::create_directory(directory + "/.cupy");
            std::filesystem::create_directory(directory + "/.cupy/saves");
            
            return 0;

        }else if(errCode == 13){
            error("Permission denied");
            return -4;
            
        }else if(errCode == 20){
            error("Passed in file name rather than directory");
            return -5;

        }else{
            error("Unexpected error occurred.");
            return -6;
        }
    }catch(int err){
        if(err == -7){
            error("cupy already initialised");
        }

        return err;
    }
}

// Function for getting the ID of the last save
int getLastSaveID(){
    int maxID = -1;

    // Iterate over the saves directory
    for(auto file : std::filesystem::directory_iterator(std::filesystem::current_path().string() + "/.cupy/saves")){
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

// Function for getting all the current tracked files
std::vector<std::string> getTrackedFiles(){
    std::vector<std::string> trackedFiles;

    std::ifstream trackFile(".cupy/.track");
    
    std::string line;
    while (std::getline(trackFile, line)) {
        trackedFiles.push_back(line);
    }

    return trackedFiles;
}

// Function for getting all files in a directory
// directory -> directory to search in for all the files
std::vector<std::string> getAllFiles(std::string directory){
    std::vector<std::string> allFiles;

    // Iterate over the directory and add the path of the file to the return vector
    for(auto file : std::filesystem::recursive_directory_iterator(directory)){
        allFiles.push_back(file.path().string());
    }

    return allFiles;
}

// Function for checking if a certain file is being tracked
// filePath -> path to file to check
bool isFileTracked(std::string filePath){
    std::ifstream trackFile(".cupy/.track");

    std::string line;
    while(std::getline(trackFile, line)){
        if(line == '[' + filePath){
            return true;
        }
    }

    return false;
}

// Function to start tracking a file/directory
// fileToAdd -> the path of the file/directory to start tracking
int addFileToTrack(std::string filePathToAdd){
    if(filePathToAdd.find(".cupy") != std::string::npos){
        log(".cupy file detected, not adding to track");
        return 0;
    }

    if(isFileTracked(filePathToAdd)){
        log(filePathToAdd + " is already being tracked");
        return 0;
    }

    // If filePathToAdd is a directory, start tracking all of the files inside the directory
    if(std::filesystem::is_directory(filePathToAdd)){
        for(auto file : std::filesystem::recursive_directory_iterator(filePathToAdd)){
            addFileToTrack(file.path().string());
        }

        return 0;
    }

    if(doesFileExist(filePathToAdd)){
        std::ofstream trackFile(".cupy/.track", std::ios::app);
        trackFile << filePathToAdd << std::endl;

    }else{
        error(filePathToAdd + " does not exist");

        return -1;
    }

    log(filePathToAdd + " added to tracking");

    return 0;
}

// Function to ignore a certain file/directory from the tracking
// filePath -> path to file/directory to ignore
void ignoreFileFromTracking(std::string filePath){
    // If filePath is a directory, ignore all of the files inside of the directory
    if(std::filesystem::is_directory(filePath)){
        for(auto file : std::filesystem::recursive_directory_iterator(filePath)){
            ignoreFileFromTracking(file.path().string());
        }
    }

    std::ifstream trackFile(".cupy/.track");
    std::vector<std::string> trackedFiles;

    std::string line;
    while(std::getline(trackFile, line)){
        if(line != filePath){
            trackedFiles.push_back(line);
        }
    }

    trackFile.close();

    std::ofstream trackFileOut(".cupy/.track");
    
    for(std::string trackedFile : trackedFiles){
        trackFileOut << trackedFile << std::endl;
    }
    
    trackFileOut.close();

    log(filePath + " ignored from tracking");
}

// Function for rebuilding an old version of a file
// filePath -> path to the file to rebuild
// saveIDFinal -> the save ID to rebuild up to (and including)
std::string rebuildOldFile(std::string filePath, int saveIDFinal){
    if(saveIDFinal < 0){
        // Trying to rebuild when there isn't a previous save to rebuild from
        return "";
    }

    std::vector<std::string> oldFileSplit;
    std::vector<Change> changes;
    bool foundContent = false;

    // Sort so it applies changes in the correct order
    std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        directoryEntries.push_back(saveDir);
    }

    // Sort according to the filename of the path, which is the save ID (otherwise would be lexographically)
    std::sort(directoryEntries.begin(), directoryEntries.end(), [](std::filesystem::__cxx11::directory_entry a, std::filesystem::__cxx11::directory_entry b){
        return std::stoi(a.path().filename().string()) < std::stoi(b.path().filename().string());
    });

    // Iterate over the save directories in order
    for(auto saveDir : directoryEntries){
        // If we have gone past the saveIDFinal, break
        if(saveDir.path().string() == ".cupy/saves/" + std::to_string(saveIDFinal + 1)){
            break;
        }

        std::string changesFilePath = saveDir.path().string() + "/.changes";

        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            std::string line;
            while(std::getline(changesFile, line)){
                if(line == '[' + filePath){
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
                        while(std::getline(changesFile, changesLine) && changesLine != "--------------------" && changesLine != ""){
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

// Function for checking if a file has changed from previous save
// filePath -> path to file that is being checked
bool hasFileChanged(std::string filePath){
    std::string oldHash = "";
    std::string newHash = "";

    // Sort directories first to keep oldHash finding updated to the most recent hash
    std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        directoryEntries.push_back(saveDir);
    }

    // Sort according to the filename of the path, which is the save ID (otherwise would be lexographically)
    std::sort(directoryEntries.begin(), directoryEntries.end(), [](std::filesystem::__cxx11::directory_entry a, std::filesystem::__cxx11::directory_entry b){
        return std::stoi(a.path().filename().string()) < std::stoi(b.path().filename().string());
    });

    // Iterate over all change files in order
    for(auto saveDir : directoryEntries){
        std::string changesFilePath = saveDir.path().string() + "/.changes";

        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            // Get the most updated hash
            std::string line;
            while(std::getline(changesFile, line)){
                if(line == '[' + filePath){
                    std::getline(changesFile, oldHash);
                    break;                
                }
            }

            changesFile.close();
        }
    }

    // Read file
    std::ifstream file(filePath);

    std::string content;
    std::string line;
    while(std::getline(file, line)){
        content += line + "\n";
    }

    file.close();

    if(content != ""){
        content.pop_back();
    }

    if(content == ""){
        // Hash of an empty string
        newHash = "d41d8cd98f00b204e9800998ecf8427e";
    }else{
        // Calculate new hash
        newHash = md5(content);
    }

    if(oldHash != newHash && oldHash != ""){
        return true;
    }

    if(oldHash == "" && newHash != ""){
        return true;
    }

    return false;
}

// Function for checking if a certain file has a beginning save entry
// filePath -> path to file that's being checked
bool hasNoFullEntry(std::string filePath){
    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        std::string changesFilePath = saveDir.path().string() + "/.changes";

        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            std::string line;
            while(std::getline(changesFile, line)){
                if(line == '[' + filePath){
                    return false;
                }
            }
        }
    }

    return true;
}

// Function to rollback to previous save
// saveID -> the ID of the save to rollback to
void rollbackToSave(int saveID){
    std::ifstream changesFile(".cupy/saves/" + std::to_string(saveID) + "/.changes");

    // Get all files that have been made up to this point (at the time of the saveID save)
    std::set<std::string> filePaths;
    for(int currentSaveID=0;currentSaveID<=saveID;currentSaveID++){
        std::ifstream changeFile(".cupy/saves/" + std::to_string(currentSaveID) + "/.changes");
        
        std::string line;
        while(std::getline(changeFile, line)){
            if(line[0] != '['){
                continue;
            }

            filePaths.insert(line.substr(1));
        }
    }

    // Rebuild each file and write to the files their previous content (at the time of the saveID save)
    for(std::string path : filePaths){
        std::string newContent = rebuildOldFile(path, saveID);

        std::ofstream outFile(path);

        outFile << newContent;

        outFile.close();
    }
}

// Function for OBLITERATING a save
// saveID -> ID of the save to obliterate
void obliterateSave(int saveID){
    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        int currentSaveID = std::stoi(saveDir.path().filename().string());

        if(currentSaveID >= saveID){
            log("Obliterating save " + std::to_string(currentSaveID));
            // obliterate...
            std::filesystem::remove_all(saveDir.path());
        }
    }
}

// Function for viewing changes made in a certain save
// saveID -> ID of the save to view changes from
void viewChanges(int saveID){
    log("Viewing change " + std::to_string(saveID));
    std::ifstream changesFile(".cupy/saves/" + std::to_string(saveID) + "/.changes");

    if(changesFile){
        std::string line;
        int lineCounter = 0;
        while(std::getline(changesFile, line)){
            lineCounter++;

            if(line == "--------------------"){
                continue;
            }

            // Check if line is a change or content line
            if(line[0] != '['){
                continue;
            }

            std::string filePath = line.substr(1);

            // Get the content before the save and then after the save
            std::string oldFileContent = rebuildOldFile(filePath, saveID-1);
            std::string updatedFileContent = rebuildOldFile(filePath, saveID);

            std::vector<std::string> changes = getChanges(oldFileContent, updatedFileContent);

            if(changes.size() > 0){
                log("Changes for file: " + filePath);
            }

            for(std::string change : changes){
                int splitPos = change.find(':');

                int lineNum = std::stoi(change.substr(0, splitPos));
                std::string changeContent = change.substr(splitPos+1);

                log("Line " + std::to_string(lineNum) + " => " + changeContent);
            }
        }
    }

    changesFile.close();
}

// Function for checking if cvcs is initialised
// projectName -> name of current project (could be empty)
bool isInitialised(){
    for(auto file : std::filesystem::directory_iterator(std::filesystem::current_path())){
        if(file.path().filename() == ".cupy" && file.is_directory()){
            return true;
        }
    }

    return false;
}

// Function for uploading files to the server
int upload(int clientSocket, std::string projectName, std::vector<std::string> filePaths, std::string saveMessage){
    sendMessage(clientSocket, "upload");
    sendMessage(clientSocket, projectName);
    sendMessage(clientSocket, std::to_string(filePaths.size()));
    sendMessage(clientSocket, saveMessage);

    log("Uploading files from projectName");

    for(std::string filePath : filePaths){
        log("Uploading " + filePath);

        sendMessage(clientSocket, filePath);

        // Read file
        std::string content;
        std::ifstream file(filePath);
        
        std::string line;
        while(std::getline(file, line)){
            content += line + '\n';
        }
        content.pop_back();

        file.close();

        // Send over file contents
        sendMessage(clientSocket, content);
    }

    return 0;
}

// Function for getting all project names from the server
std::vector<std::string> getProjectNames(){
    // Ask server for list of project names

    initialiseSockets();

    SocketType clientSocket = connectToServer(SERVER_IP, SERVER_PORT);

    log("Requesting project names...");

    sendMessage(clientSocket, "list");
    
    int projectCount = std::stoi(receiveMessage(clientSocket));

    if(projectCount < 0){
        return {};
    }

    std::vector<std::string> projectNames;
    for(int i=0; i<projectCount; i++){
        std::string projectName = receiveMessage(clientSocket);
        projectNames.push_back(projectName);
    }

    closeSocket(clientSocket);

    return projectNames;
}

// Function for downloading a certain project
int download(std::string projectName){
    initialiseSockets();

    SocketType clientSocket = connectToServer(SERVER_IP, SERVER_PORT);

    log("Downloading project " + projectName);

    sendMessage(clientSocket, "download");
    sendMessage(clientSocket, projectName);

    closeSocket(clientSocket);
}

std::string getProjectName(){
    std::ifstream projectFile(".cupy/.project");

    std::string projectName;
    std::getline(projectFile, projectName);

    projectFile.close();

    return projectName;
}

int main(int argc, char* argv[]){
    std::string projectName = "";

    if(isInitialised()){
        std::ifstream projectFile(std::filesystem::current_path().string() + "/.cupy/project");

        std::getline(projectFile, projectName);
    
        projectFile.close();
    }

    if(argc <= 1){
        error("Not enough arguments passed. Usage:\ncvcs init <directory>\ncvcs save <message>?\ncvcs add <filename>\ncvcs ignore <filename>\ncvcs rollback <saveID>\ncvcs obliterate <saveID>\ncvcs history\ncvcs status\ncvcs upload <filenames>? @<message>@?\ncvcs download <projectname?>");
        return -1;

    }else if(std::string(argv[1]) == "help"){
        log("Usage:\ncvcs init <directory>\ncvcs save <message>?\ncvcs add <filename>\ncvcs ignore <filename>\ncvcs rollback <saveID>\ncvcs obliterate <saveID>\ncvcs history\ncvcs status\ncvcs upload <filenames>? @<message>@?\ncvcs download <projectname?>");

    }else if(std::string(argv[1]) == "history" && argc == 2){
        // View history

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        // Sort because who would want to see the history out of order
        std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

        for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
            directoryEntries.push_back(saveDir);
        }

        // Sort according to the filename of the path, which is the save ID (otherwise would be lexographically)
        std::sort(directoryEntries.begin(), directoryEntries.end(), [](std::filesystem::__cxx11::directory_entry a, std::filesystem::__cxx11::directory_entry b){
            return std::stoi(a.path().filename().string()) < std::stoi(b.path().filename().string());
        });

        log("Available commands: ");
        log("  q: Quit");
        log("  r: Rollback");
        log("  o: Obliterate");
        log("  v: View Changes");

        // Iterate over save directories in order
        std::string input;
        for(auto saveDir : directoryEntries){
            std::string saveFilePath = saveDir.path().string() + "/.save";
            std::ifstream saveFile(saveFilePath);
            std::string changesFilePath = saveDir.path().string() + "/.changes";
            std::ifstream changesFile(changesFilePath);

            std::string dateTime = "";
            std::string message = "";
            
            int saveID = 0;

            std::getline(saveFile, dateTime);
            std::getline(saveFile, message);

            saveID = std::stoi(saveDir.path().filename().string());

            saveFile.close();

            log("------------------------");
            log("Save ID: " + std::to_string(saveID));
            log("Date Time: " + dateTime);
            log("Message: " + message);
        
            while(true){
                std::cout << ">> ";
                std::getline(std::cin, input);

                if(input == "q"){
                    break;
                }else if(input == "r"){
                    rollbackToSave(saveID);
                }else if(input == "o"){
                    obliterateSave(saveID);
                }else if(input == "v"){
                    viewChanges(saveID);
                }else if(input == ""){
                    break;
                }
            }

            if(input == "q"){
                break;
            }
            
        }

        return 0;

    }else if(std::string(argv[1]) == "add" && argc == 3){
        // Add file to tracking

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }
    
        std::string fileToAdd = std::string(argv[2]);

        addFileToTrack(std::filesystem::current_path().string() + "/" + fileToAdd);

        return 0;

    }else if(std::string(argv[1]) == "add" && argc > 3){
        // Iterate over argv and pass each file to addFileToTrack

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        for(int i = 2; i < argc; i++){
            addFileToTrack(std::filesystem::current_path().string() + "/" + std::string(argv[i]));
        }

        return 0;

    }else if(std::string(argv[1]) == "ignore" && argc == 3){
        // Ignore file from tracking

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        std::string fileToIgnore = std::string(argv[2]);
        ignoreFileFromTracking(std::filesystem::current_path().string() + "/" + fileToIgnore);

        return 0;

    }else if(std::string(argv[1]) == "ignore" && argc > 3){
        // Iterate over argv and pass each file to ignoreFileFromTracking

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        for(int i=2; i<argc; i++){
            ignoreFileFromTracking(std::filesystem::current_path().string() + "/" + std::string(argv[i]));
        }

        return 0;

    }else if(std::string(argv[1]) == "rollback" && argc == 3){
        // Rollback to a specific save

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        int saveID = std::stoi(argv[2]);

        if(saveID > getLastSaveID()){
            error("Invalid save ID");
            return -9;
        }

        log("Rolling back to save " + std::to_string(saveID));

        rollbackToSave(saveID);

        return 0;
    
    }else if(std::string(argv[1]) == "upload" && argc == 2){
        // Upload all tracked files onto server (server does diff processing)

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        std::string projectName = getProjectName();

        initialiseSockets();

        SocketType clientSocket = connectToServer(SERVER_IP, SERVER_PORT);

        // Iterate over tracked files and pass each file to upload function
        
        std::vector<std::string> trackedFiles = getTrackedFiles();

        upload(clientSocket, projectName, trackedFiles, "");

        closeSocket(clientSocket);

    }else if(std::string(argv[1]) == "upload" && argc == 3){
        // Either "cvcs upload <filename>" or "cvcs upload <message>" has been ran

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        std::string argument = std::string(argv[2]);

        std::string projectName = getProjectName();

        initialiseSockets();

        SocketType clientSocket = connectToServer(SERVER_IP, SERVER_PORT);

        if(argument[0] == '@' && argument[argument.length()-1] == '@'){
            // It's a string, so must be the message

            argument = argument.substr(1).substr(0, argument.length()-2);

            std::vector<std::string> trackedFiles = getTrackedFiles();

            upload(clientSocket, projectName, trackedFiles, argument);

        }else{
            // Must be the file name
            std::vector<std::string> files = {argument};

            upload(clientSocket, projectName, files, "No message provided");
        }

        closeSocket(clientSocket);

    }else if(std::string(argv[1]) == "upload" && argc > 3){
        // Upload specified files onto server (server does diff processing)

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        std::string projectName = getProjectName();

        initialiseSockets();

        SocketType clientSocket = connectToServer(SERVER_IP, SERVER_PORT);

        // Iterate over argv and pass each file to upload function

        std::vector<std::string> files;

        for(int i=2; i<argc; i++){
            files.push_back(std::string(argv[i]));
        }

        if(files[argc-3][0] == '@' && files[argc-3][files[argc-3].length()-1] == '@'){
            // User included a message

            // Get the message
            std::string message = files[files.size()-1].substr(1).substr(0, files[files.size()-1].length()-2);

            // Remove the message from the files
            files.pop_back();

            upload(clientSocket, projectName, files, message);
        }else{
            // User did not include a message

            upload(clientSocket, projectName, files, "No message provided");
        }

        closeSocket(clientSocket);

    }else if(std::string(argv[1]) == "download" && argc == 2){
        // Get project names and let user choose which one to download

        // NO NEED TO INITIALISE IF DOWNLOADING PROJECT FILES!!!!!!

        std::vector<std::string> projectNames = getProjectNames();

        if(projectNames.size() > 0){
            for(std::string projectName : projectNames){
                log(projectName);
            }
        }

    }else if(std::string(argv[1]) == "download" && argc == 3){
        // Download stored files for specified project (argv[2])

        error("Not yet implemented!!");

    }else if(std::string(argv[1]) == "status"){
        // Show status of tracked files

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        std::vector<std::string> trackedFiles = getTrackedFiles();

        bool anyChanges = false;
        for(std::string trackedFile : trackedFiles){
            if(hasFileChanged(trackedFile)){
                anyChanges = true;

                log(trackedFile + " has been modified");
            
                // Get the content at last save
                std::string oldContent = rebuildOldFile(trackedFile, getLastSaveID());
                
                // Get the current content
                std::string newContent = "";
                std::ifstream newFileStream(trackedFile);
                
                std::string line;
                while(std::getline(newFileStream, line)){
                    newContent += line + "\n";
                }
                // Remove extra newline
                newContent.pop_back();

                newFileStream.close();

                std::vector<std::string> changes = getChanges(oldContent, newContent);

                for(std::string change : changes){
                    int counter = change.find_first_of(':');

                    int lineNum = std::stoi(change.substr(0, counter+1));

                    std::string newLine = change.substr(counter+1);

                    log("[" + trackedFile + "]" + std::to_string(lineNum) + " => " + newLine);
                }

                log(" ");
            }
        }

        if(!anyChanges){
            log("No changes detected");
        }

        return 0;

    }else if(std::string(argv[1]) == "obliterate" && argc == 3){
        // Obliterate save and every save ahead

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        int saveID = std::stoi(argv[2]);

        log("Obliterating save(s) " + std::to_string(saveID) + " onwards");

        obliterateSave(saveID);

    }else if((std::string(argv[1]) == "save") && (argc == 2 || argc == 3)){
        // Save the current state

        if(!isInitialised()){
            error("cvcs not initialised!");
            return -11;
        }

        if(getLastSaveID() > 0){
            // Check if any changes to files have been made
            std::vector<std::string> trackedFiles = getTrackedFiles();

            bool changesMade = false;
            for(std::string trackedFile : trackedFiles){
                if(hasFileChanged(trackedFile)){
                    changesMade = true;
                    break;
                }
            }

            // If they haven't, exit early
            if(!changesMade){
                error("No changes detected");
                return -10;
            }
        }

        // Save current state
        std::string cwd = std::filesystem::current_path().string();

        for(auto file : std::filesystem::directory_iterator(cwd)){
            std::filesystem::path path = file.path();
            std::string fileName = path.filename().string();

            std::string saveMessage = (argc == 2) ? "No message provided" : argv[2];
            int saveID = getLastSaveID() + 1;

            // Create new directory and files for the current save
            std::filesystem::create_directory(cwd + "/.cupy/saves/" + std::to_string(saveID));
            std::ofstream changesFile(cwd + "/.cupy/saves/" + std::to_string(saveID) + "/.changes");
            std::ofstream saveFile(cwd + "/.cupy/saves/" + std::to_string(saveID) + "/.save");

            saveFile << getDateTime();
            saveFile << saveMessage;
                
            saveFile.close();
                
            for(std::string trackedFile : getTrackedFiles()){
                std::ifstream trackedFileStream(trackedFile);
                std::vector<std::string> content;

                // Get the current content of the file
                std::string line;
                while(std::getline(trackedFileStream, line)){
                    content.push_back(line);
                }

                trackedFileStream.close();

                if(hasFileChanged(trackedFile)){
                    log("File has been changed: " + trackedFile);
                    // Store the path and hash of the file
                    changesFile << '[' << trackedFile << std::endl;

                    std::string reconstructedString = reconstructSplitString(content);
                    if(reconstructedString != ""){
                        changesFile << md5(reconstructedString) << std::endl;
                    }else{
                        // Hash of an empty string
                        changesFile << "d41d8cd98f00b204e9800998ecf8427e" << std::endl;
                    }

                    // Get the changes from previous save
                    std::string oldContent = rebuildOldFile(trackedFile, getLastSaveID()-1);

                    std::vector<std::string> changes = getChanges(oldContent, reconstructSplitString(content));

                    for(std::string change : changes){
                        changesFile << change << std::endl;

                        int splitPos = change.find_first_of(':');

                        int lineNum = std::stoi(change.substr(0, splitPos+1));

                        std::string newLine = change.substr(splitPos+1);

                        log("[" + trackedFile + "]" + std::to_string(lineNum) + " => " + newLine);
                    }

                    if(changes.size() >= 1){
                        log(" ");
                    }

                    changesFile << "--------------------" << std::endl;

                // If there's no changes, and the file has never been saved before, save it
                }else if(hasNoFullEntry(trackedFile)){
                    changesFile << '[' << trackedFile << std::endl;
                    changesFile << md5(reconstructSplitString(content)) << std::endl;
                    changesFile << reconstructSplitString(content) << std::endl;
                    changesFile << "--------------------" << std::endl;                    
                
                }
            }

            changesFile.close();

            log("Saved successfully");

            return 0;
        }
            
    }else if(std::string(argv[1]) == "init" && argc == 3){
        // Initialise
        
        std::string directoryPath = std::string(argv[2]);

        int retCode = initialise(directoryPath);

        if(retCode < 0){
            error("Errors occurred, exiting...");
            return retCode;
        }

        if(directoryPath == "."){
            directoryPath = std::filesystem::current_path().string();
        }

        #ifdef _WIN32
            int splitPos = directoryPath.find_last_of('\\');
        #else
            int splitPos = directoryPath.find_last_of('/');
        #endif
        
        projectName = directoryPath.substr(splitPos+1);

        std::ofstream projectFile(directoryPath + "/.cupy/.project");
        projectFile << projectName;
        projectFile.close();

        log("Initialised successfully with project name " + projectName);

        return 0;

    }else{
        error("Invalid arguments passed. Usage:\ncvcs init <directory>\ncvcs save <message>?\ncvcs add <filename>\ncvcs ignore <filename>\ncvcs rollback <saveID>\ncvcs obliterate <saveID>\ncvcs history\ncvcs status\ncvcs upload <filenames>? @<message>@?\ncvcs download <projectname?>");
        return -2;
    }

    return 0;
}