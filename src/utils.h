#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

// Struct for storing the changes made to a file
struct Change{
    int lineNum;
    std::string lineContent;
};

// Function for getting the current date and time as a string
std::string getDateTime();

// Function for reconstructing strings that have been split by newlines
// splitString -> the vector of strings to be reconstructed
std::string reconstructSplitString(std::vector<std::string> splitString);

// Function for checking if a file exists
// filePath -> path to the file to check
bool doesFileExist(std::string filePath);

// Function for getting changes between two files (line-by-line, update to Myer's diff pls and thanks)
// file1 -> the first file's contents
// file2 -> the second file's contents
std::vector<std::string> getChanges(std::string file1Contents, std::string file2Contents);

#endif