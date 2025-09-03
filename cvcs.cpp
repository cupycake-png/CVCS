#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <ctime>

#include "md5.h"

// Potentially necessary for cross platform compatibility
#ifdef _WIN32
    #include <windows.h>
#endif

struct Change{
    int lineNum;
    std::string lineContent;
};

struct SaveData{
    std::string dateTime;
    std::string message;
    int id;
};

template <typename T>
void error(T errMessage){
    std::cout << "[-] " << errMessage << std::endl;
}

template <typename T>
void log(T message){
    std::cout << "[!] " << message << std::endl;
}

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

bool doesFileExist(std::string fileName){
    std::ifstream file(fileName);
    return file.good();
}

int getLastSaveID(){
    int lastID = -1;

    for(auto file : std::filesystem::directory_iterator(std::filesystem::current_path().string() + "/.cupy/saves")){
        std::string fileName = file.path().filename().string();

        try{
            int saveID = std::stoi(fileName);
            lastID = std::max(lastID, saveID);
        }catch(std::invalid_argument& e){
            // Ignore non-integer filenames
        }
    }

    return lastID;
}

std::string getDateTime(){
    time_t ts;
    time(&ts);

    return ctime(&ts);
}

std::vector<std::string> getTrackedFiles(){
    std::vector<std::string> trackedFiles;

    std::ifstream trackFile(".cupy/.track");
    
    std::string line;
    while (std::getline(trackFile, line)) {
        trackedFiles.push_back(line);
    }

    return trackedFiles;
}

std::vector<std::string> getAllFiles(std::string directory){
    std::vector<std::string> allFiles;

    for(auto file : std::filesystem::recursive_directory_iterator(directory)){
        allFiles.push_back(file.path().string());
    }

    return allFiles;
}

bool isFileTracked(std::string fileToAdd){
    std::ifstream trackFile(".cupy/.track");

    std::string line;
    while(std::getline(trackFile, line)){
        if(line == fileToAdd){
            return true;
        }
    }

    return false;
}

int addFileToTrack(std::string fileToAdd){
    if(fileToAdd.find(".cupy") != std::string::npos){
        log(".cupy file detected, not adding to track");
        return 0;
    }

    if(isFileTracked(fileToAdd)){
        log(fileToAdd + " is already being tracked");
        return 0;
    }

    if(std::filesystem::is_directory(fileToAdd)){
        for(auto file : std::filesystem::recursive_directory_iterator(fileToAdd)){
            addFileToTrack(file.path().string());
        }

        return 0;
    }

    if(doesFileExist(fileToAdd)){
        std::ofstream trackFile(".cupy/.track", std::ios::app);
        trackFile << fileToAdd << std::endl;

    }else{
        error(fileToAdd + " does not exist");

        return -1;
    }

    log(fileToAdd + " added to tracking");

    return 0;
}

void ignoreFileFromTracking(std::string fileName){
    if(std::filesystem::is_directory(fileName)){
        for(auto file : std::filesystem::recursive_directory_iterator(fileName)){
            ignoreFileFromTracking(file.path().string());
        }
    }

    std::ifstream trackFile(".cupy/.track");
    std::vector<std::string> trackedFiles;

    std::string line;
    while(std::getline(trackFile, line)){
        if(line != fileName){
            trackedFiles.push_back(line);
        }
    }

    trackFile.close();

    std::ofstream trackFileOut(".cupy/.track");
    for(auto trackedFile : trackedFiles){
        trackFileOut << trackedFile << std::endl;
    }

    log(fileName + " ignored from tracking");
}

bool hasFileChanged(std::string fileName){
    std::string oldHash = "";
    std::string newHash = "";

    // Check if file has been created before
    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        std::string changesFilePath = saveDir.path().string() + "/.changes";

        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            // Get the most updated hash
            std::string line;
            while(std::getline(changesFile, line)){
                if(line == fileName){
                    std::getline(changesFile, oldHash);
                
                    std::ifstream file(fileName);

                    std::string content;
                    std::string line;
                    while(std::getline(file, line)){
                        content += line + "\n";
                    }
                    content.pop_back();

                    newHash = md5(content);
                
                    file.close();
                }
            }
        }
    }

    if(oldHash != newHash && oldHash != ""){
        return true;
    }

    if(oldHash == "" && newHash != ""){
        return true;
    }

    return false;
}

std::string reconstructSplitString(std::vector<std::string> splitString){
    std::string reconstructed = "";

    for(int i=0; i<splitString.size(); i++){
        reconstructed += splitString[i];

        if(i < splitString.size()-1){
            reconstructed += "\n";
        }
    }

    return reconstructed;
}

std::string rebuildOldFile(std::string filePath, int saveIDStop=getLastSaveID()){
    std::vector<std::string> oldFileSplit;
    std::vector<Change> changes;
    bool foundContent = false;

    std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        directoryEntries.push_back(saveDir);
    }

    std::sort(directoryEntries.begin(), directoryEntries.end());

    // Remove save directories that aren't needed

    for(auto saveDir : directoryEntries){
        if(saveDir.path().string() == ".cupy/saves/" + std::to_string(saveIDStop + 1)){
            break;
        }

        std::string changesFilePath = saveDir.path().string() + "/.changes";

        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            std::string line;
            while(std::getline(changesFile, line)){
                if(line == filePath){
                    if(!foundContent){
                        // Ignore hash
                        std::getline(changesFile, line);

                        // TODO: Iterate over all change files until content is found
                        std::string changesLine = "";
                        while(std::getline(changesFile, changesLine) && changesLine != "--------------------"){
                            oldFileSplit.push_back(changesLine);
                        }

                        foundContent = true;

                        break;

                    }else{
                        // Ignore hash
                        std::getline(changesFile, line);

                        std::string changesLine = "";
                        while(std::getline(changesFile, changesLine) && changesLine != "--------------------" && changesLine != ""){
                            int splitPos = changesLine.find_first_of(':');
                            
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

std::vector<std::string> getChanges(std::string file1, std::string file2){
    std::vector<std::string> changes;

    std::vector<std::string> oldLines;
    std::vector<std::string> newLines;

    std::istringstream oldStream(file1);
    std::string line;

    while(std::getline(oldStream, line)){
        oldLines.push_back(line);
    }

    std::istringstream newStream(file2);
    while(std::getline(newStream, line)){
        newLines.push_back(line);
    }

    size_t maxLines = std::max(oldLines.size(), newLines.size());

    for(size_t i=0; i<maxLines; i++){
        std::string oldLine = (i < oldLines.size() ? oldLines[i] : "");
        std::string newLine = (i < newLines.size() ? newLines[i] : "");

        if(oldLine != newLine){
            changes.push_back(std::to_string(i+1) + ":" + newLine);
        }
    }

    return changes;
}

bool hasNoFullEntry(std::string filePath){
    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        std::string changesFilePath = saveDir.path().string() + "/.changes";

        if(doesFileExist(changesFilePath)){
            std::ifstream changesFile(changesFilePath);

            std::string line;
            while(std::getline(changesFile, line)){
                if(line == filePath){
                    return false;
                }
            }
        }
    }

    return true;
}

void rollbackToSave(int saveID){
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
            if(line[0] != '/'){
                continue;
            }

            std::string filePath = line;
            std::string oldHash = "";
            std::getline(changesFile, oldHash);
            std::string newContent = rebuildOldFile(filePath, saveID);

            std::ofstream outFile(filePath);
            
            outFile << newContent;
            
            outFile.close();
        }
    }
}

void obliterateSave(int saveID){
    for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
        int currentSaveID = std::stoi(saveDir.path().filename().string());

        if(currentSaveID >= saveID){
            log("Obliterating save " + std::to_string(currentSaveID));
            std::filesystem::remove_all(saveDir.path());
        }
    }
}

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
            if(line[0] != '/'){
                continue;
            }

            std::string filePath = line;
            std::string oldHash = "";
            std::getline(changesFile, oldHash);

            std::ifstream file(filePath);

            std::string currentContent = "";
            std::string line;
            while(std::getline(file, line)){
                currentContent += line + "\n";
            }

            file.close();

            std::string oldFile = rebuildOldFile(filePath, saveID-1);

            std::vector<std::string> changes = getChanges(oldFile, currentContent);

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

// TODO: STOP ANYTHING WORKING IF .cupy ISN'T FOUND
// TODO: deletions aren't saved in the changes file
// TODO: history command "v" gives most recent changes rather than changes in that save
// TODO: get rid of md5 dependency (maybe try easier hashing algorithm?)
// TODO: make status show actual changes
// TODO: upload command to upload to a server like github fr fr
// TODO: rollback do NAWT work

int main(int argc, char* argv[]){
    if(argc <= 1){
        error("Not enough arguments passed. Usage:\ncvcs <directory>\ncvcs save <message?>\ncvcs add <filename>\ncvcs ignore <filename>\ncvcs rollback <saveID>\ncvcs history");
        return -1;

    }else if(std::string(argv[1]) == "history" && argc == 2){
        // View history

        std::vector<std::filesystem::__cxx11::directory_entry> directoryEntries;

        for(auto saveDir : std::filesystem::directory_iterator(".cupy/saves/")){
            directoryEntries.push_back(saveDir);
        }

        std::sort(directoryEntries.begin(), directoryEntries.end());

        log("Available commands: ");
        log("  q: Quit");
        log("  r: Rollback");
        log("  o: Obliterate");
        log("  v: View Changes");

        std::string input;
        for(auto saveDir : directoryEntries){
            std::string saveFilePath = saveDir.path().string() + "/.save";
            std::ifstream saveFile(saveFilePath);
            std::string changesFilePath = saveDir.path().string() + "/.changes";
            std::ifstream changesFile(changesFilePath);
            SaveData saveData;

            std::getline(saveFile, saveData.dateTime);
            std::getline(saveFile, saveData.message);
            saveData.id = std::stoi(saveDir.path().filename().string());

            saveFile.close();

            log("------------------------");
            log("Save ID: " + std::to_string(saveData.id));
            log("Date Time: " + saveData.dateTime);
            log("Message: " + saveData.message);
        
            while(true){
                std::cout << ">> ";
                std::getline(std::cin, input);

                if(input == "q"){
                    break;
                }else if(input == "r"){
                    rollbackToSave(saveData.id);
                }else if(input == "o"){
                    obliterateSave(saveData.id);
                }else if(input == "v"){
                    viewChanges(saveData.id);
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
        std::string fileToAdd = std::string(argv[2]);

        addFileToTrack(std::filesystem::current_path().string() + "/" + fileToAdd);

        return 0;

    }else if(std::string(argv[1]) == "add" && argc > 3){
        // Iterate over argv and pass each file to addFileToTrack
        for(int i = 2; i < argc; i++){
            addFileToTrack(std::filesystem::current_path().string() + "/" + std::string(argv[i]));
        }

        return 0;

    }else if(std::string(argv[1]) == "ignore" && argc == 3){
        // Ignore file from tracking
        std::string fileToIgnore = std::string(argv[2]);
        ignoreFileFromTracking(std::filesystem::current_path().string() + "/" + fileToIgnore);

        return 0;

    }else if(std::string(argv[1]) == "ignore" && argc > 3){
        // Iterate over argv and pass each file to ignoreFileFromTracking
        for(int i = 2; i < argc; i++){
            ignoreFileFromTracking(std::filesystem::current_path().string() + "/" + std::string(argv[i]));
        }

        return 0;

    }else if(std::string(argv[1]) == "rollback" && argc == 3){
        // Rollback to a specific save
        int saveID = std::stoi(argv[2]);

        if(saveID > getLastSaveID()){
            error("Invalid save ID");
            return -9;
        }

        log("Rolling back to save " + std::to_string(saveID));

        rollbackToSave(saveID);

        return 0;
    
    }else if(std::string(argv[1]) == "status"){
        // Show status of tracked files
        std::vector<std::string> trackedFiles = getTrackedFiles();

        for(std::string trackedFile : trackedFiles){
            if(hasFileChanged(trackedFile)){
                log(trackedFile + " has been modified");
            }
        }

        return 0;

    }else if(std::string(argv[1]) == "obliterate" && argc == 3){
        // Nuke save and every save ahead
        int saveID = std::stoi(argv[2]);

        log("Obliterating save(s) " + std::to_string(saveID) + " onwards");

        obliterateSave(saveID);

    }else if((std::string(argv[1]) == "save") && (argc == 2 || argc == 3)){

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

            if(fileName == ".cupy"){
                std::string saveMessage = (argc == 2) ? "No message provided" : argv[2];
                int saveID = getLastSaveID() + 1;

                std::filesystem::create_directory(cwd + "/.cupy/saves/" + std::to_string(saveID));

                std::ofstream saveFile(cwd + "/.cupy/saves/" + std::to_string(saveID) + "/.save");
                
                saveFile << getDateTime();
                saveFile << saveMessage;
                saveFile.close();

                std::ofstream changesFile(cwd + "/.cupy/saves/" + std::to_string(saveID) + "/.changes");
                
                for(std::string trackedFile : getTrackedFiles()){
                    std::ifstream trackedFileStream(trackedFile);
                    std::vector<std::string> content;

                    std::string line;
                    while(std::getline(trackedFileStream, line)){
                        content.push_back(line);
                    }

                    trackedFileStream.close();

                    if(hasFileChanged(trackedFile)){
                        changesFile << trackedFile << std::endl;
                        changesFile << md5(reconstructSplitString(content)) << std::endl;

                        std::string oldContent = rebuildOldFile(trackedFile, getLastSaveID());

                        std::vector<std::string> changes = getChanges(oldContent, reconstructSplitString(content));

                        for(std::string change : changes){
                            changesFile << change << std::endl;

                            int counter = change.find_first_of(':');

                            int lineNum = std::stoi(change.substr(0, counter+1));

                            std::string newLine = change.substr(counter+1);

                            log("[" + trackedFile + "]" + std::to_string(lineNum) + " => " + newLine);

                            if(lineNum > content.size()){
                                content.resize(lineNum);
                            }

                            content[lineNum-1] = newLine;
                        }

                        if(changes.size() >= 1){
                            log(" ");
                        }

                        changesFile << "--------------------" << std::endl;

                    }else if(hasNoFullEntry(trackedFile)){
                        changesFile << trackedFile << std::endl;
                        changesFile << md5(reconstructSplitString(content)) << std::endl;
                        changesFile << reconstructSplitString(content) << std::endl;
                        changesFile << "--------------------" << std::endl;                    
                    }
                }

                changesFile.close();

                log("Saved successfully");

                return 0;
            }
        }

        error("No .cupy directory found");

        return -8;
    
    }else if(argc == 2){
        // Initialise
        
        std::string directoryPath = std::string(argv[1]);

        int retCode = initialise(directoryPath);

        if(retCode < 0){
            error("Errors occurred, exiting...");
            return retCode;
        }

        log("Initialised successfully");

        return 0;

    }else{
        error("Invalid arguments passed. Usage:\ncvcs <directory>\ncvcs save <message?>\ncvcs add <filename>\ncvcs ignore <filename>\ncvcs rollback <saveID>\ncvcs history");
        return -2;
    }

    return 0;
}