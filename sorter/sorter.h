#ifndef SORTER_H
#define SORTER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstring>

class Sorter
{
public:
    Sorter(const std::string &cacheFolder, const std::string &inputFile, const std::string &outputFile, const uint64_t chunkSize) :
        m_cacheFolder(cacheFolder),
        m_inputFile(inputFile),
        m_outputFile(outputFile),
        m_averageChunkSize(chunkSize)
    {}

    void process();

private:
    std::string m_cacheFolder;
    std::string m_inputFile;
    std::string m_outputFile;
    size_t m_averageChunkSize;

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

    struct IterativeChunk{
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

        //currenc index in chunksInfo
        int indexOfChunk;

        //current position in current loaded chunk in terms of whole file
        size_t indexGlobalOffset;
        size_t byteGlobalOffset;
        size_t currChunkSize;
        size_t currChunkIndexSize;

        IterativeChunk(const std::string &dataFilePath, const std::string &indexFilePath, const std::vector<ChunkOfChunkInfo> chunksInfo);
        void init();
        bool loadNextChunk();
    };
    std::vector<IterativeChunk> m_chunks;

    size_t m_averageChunkOfChunkSize;

    std::vector<size_t> findChunkBounds(std::ifstream &file, size_t averageChunkSize);
    std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data);
    std::vector<LineInfo> collectChunkInfo(std::vector<char> &data);

    void merge();

    // returns lineInfo for saved data
    void saveSortedChunk(const std::vector<size_t> &idx, const std::vector<Sorter::LineInfo> linesInfo, std::vector<char> &rawData, size_t chunkIndex);

    static std::string genFilePath(const std::string &folder, const std::string &name, int index);
    static inline char customSTRCMP(const char *p1, const char *p2);
};

#endif // SORTER_H
