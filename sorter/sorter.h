#ifndef SORTER_H
#define SORTER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstring>

namespace sorter
{

using ChunksVector = std::vector<std::pair<int64_t, int64_t>>;

void process(const std::string &cacheFolder, const std::string &inputFile, const std::string &outputFile, const int64_t chunkSize);

struct LineInfo{
    size_t num; // cached Number
    size_t start; // start byte in chunk
    size_t strStart; // start str byte
    size_t finis; // finis byte in chunk
};

struct ChunkOfChunkInfo
{
    size_t startByte;
    size_t startLineInfoIndex;
    size_t finisByte;
    size_t finisLineInfoIndex;
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
    int indexOfChunk;

    //current position in current loaded chunk in terms of whole file
    size_t globalOffset;
    size_t currSize;

    IterativeFile(const std::string &filePath, const ChunksVector &chunksInfo);
    bool init();
    bool loadNextChunk();
};


struct IterativeFileDeprecated{
    // input streams
    std::ifstream dataFile;
    std::ifstream indexFile;
    std::string dataFilePath;
    std::string indexFilePath;

    //info about file parts
    std::vector<ChunkOfChunkInfo> chunksInfo;

    //stored data and index
    std::vector<char> chunkData;
    std::vector<LineInfo> chunkIndexData;

    //current index of chunksInfo
    int indexOfChunk;

    //current position in current loaded chunk in terms of whole file
    size_t indexGlobalOffset;
    size_t byteGlobalOffset;
    size_t currChunkSize;
    size_t currChunkIndexSize;

    IterativeFileDeprecated(const std::string &dataFilePath, const std::string &indexFilePath, const std::vector<ChunkOfChunkInfo> chunksInfo);
    void init();
    bool loadNextChunk();
};

std::vector<std::pair<int64_t, int64_t>> findChunkBounds(const std::string &filePath, int64_t averageChunkSize);
std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data);
std::vector<LineInfo> collectLineInfo(std::vector<char> &data);

void merge(std::vector<IterativeFileDeprecated> &m_chunks, const std::string &m_outputFile);

// returns lineInfo for saved data
IterativeFileDeprecated saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> linesInfo, std::vector<char> &rawData, size_t chunkIndex, const std::string &m_cacheFolder, size_t m_averageChunkOfChunkSize);

std::string genFilePath(const std::string &folder, const std::string &name, int index);
inline char customSTRCMP(const char *p1, const char *p2);

};

#endif // SORTER_H
