#ifndef SORTER_H
#define SORTER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <chrono>
#include <mutex>
#include <iostream>
#include <thread>
#include <string>
#include <sstream>


namespace sorter
{

using ChunksVector = std::vector<std::pair<int64_t, int64_t>>;

void sortBigFile(const std::string &cacheFolder, const std::string &inputFile, const std::string &outputFile, const int64_t chunkSize);

struct LineInfo{
    size_t num; // cached Number
    size_t start; // start byte in chunk
    size_t strStart; // start str byte
    size_t finis; // finis byte in chunk
};

class Semaphore
{
public:
    Semaphore(int count);
    void notify();
    void wait();
private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    unsigned long m_count;
};

class IterativeFile{
public:
    IterativeFile(const std::string &m_filePath, const ChunksVector &m_chunksInfo);
    bool init();
    bool loadNextChunk();
    void close();
    std::vector<char> data; // current loaded chunk

    const std::string filePath();
private:
    std::ifstream m_file;
    std::string m_filePath;
    ChunksVector m_chunksInfo; //info about file parts
    size_t m_indexOfChunk;    //current index of chunksInfo
};

class TimeTracker
{
public:
    void start();
    int elapsed();
private:
    std::chrono::system_clock::time_point startTime;
};

// just chunk file into shunks with appropriate size
ChunksVector findChunkBounds(const std::string &filePath, int64_t averageChunkSize);

// sort by indexes
std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data);

//collect cache about raw data
std::vector<LineInfo> collectLineInfo(std::vector<char> &data);

//sort asynchoniasly parts of file and save this parts to cache folder
std::vector<IterativeFile> asyncChunkSort(const std::string &filePath, const ChunksVector &chunks, const std::string &cacheFolder, const int64_t averageChunkSize);

//k-way merge sort with heap speedup
void merge(std::vector<IterativeFile> &iterativeChunks, const std::string & outputFile);

//utilite for saving data due to sorted cache. return "microchunks" of saved chunk-file
ChunksVector saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> &linesInfo, std::vector<char> &rawData, const std::string &chunkFilePath, size_t averageChunkOfChunkSize);

//waiting for full c++17 support...
std::string genFilePath(const std::string &folder, const std::string &name, int index);

//we need to stop strcmp after the \n (not \00)
//copypaste from glibc/string/strcmp.c
inline char customSTRCMP(const char *p1, const char *p2);

};

#endif // SORTER_H
