#ifndef SORTER_H
#define SORTER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <chrono>

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

struct IterativeFile{
    // input streams
    std::ifstream file;
    std::string filePath;
    //info about file parts
    ChunksVector chunksInfo;

    //current data
    std::vector<char> data;

    //current index of chunksInfo
    size_t indexOfChunk;

    //current position in current loaded chunk in terms of whole file
    size_t globalOffset;
    size_t currSize;

    IterativeFile(const std::string &filePath, const ChunksVector &chunksInfo);
    bool init();
    bool loadNextChunk();
    void close();
};

class TimeTracker
{
public:
    void start();
    int elapsed();
private:
    std::chrono::system_clock::time_point startTime;
};

std::vector<std::pair<int64_t, int64_t>> findChunkBounds(const std::string &filePath, int64_t averageChunkSize);
std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data);
std::vector<LineInfo> collectLineInfo(std::vector<char> &data);

void merge(std::vector<std::pair<IterativeFile, IterativeFile>> &m_chunks, const std::string &m_outputFile);

// returns lineInfo for saved data
std::pair<IterativeFile, IterativeFile> saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> linesInfo, std::vector<char> &rawData,
                                                        const std::string &chunkFilePath, const std::string &chunkIndexFilePath, size_t m_averageChunkOfChunkSize);

std::string genFilePath(const std::string &folder, const std::string &name, int index);
inline char customSTRCMP(const char *p1, const char *p2);

};

#endif // SORTER_H
