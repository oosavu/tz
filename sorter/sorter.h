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

    std::vector<std::string> m_chunkFilesPaths;

    struct LineInfo{
        size_t num; // cached Number
        size_t start; // start byte in chunk
        size_t strStart; // start str byte
        size_t finis; // finis byte in chunk
    };
    std::vector<std::vector<LineInfo>> m_sortedChunksInfo;

    std::vector<size_t> findChunkBounds(std::ifstream &file);
    std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data);
    std::vector<LineInfo> collectChunkInfo(std::vector<char> &data);

    void merge();

    // returns lineInfo for saved data
    void saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> lineInfo, char* rawData, const std::string &filePath);

    static std::string genFilePath(const std::string &folder, const std::string &name, int index);
    static inline char customSTRCMP(const char *p1, const char *p2);
};

#endif // SORTER_H
