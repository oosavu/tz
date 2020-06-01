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
        m_chunkSize(chunkSize)
    {}

    void process()
    {
    }

private:
    std::string m_cacheFolder;
    std::string m_inputFile;
    std::string m_outputFile;
    uint64_t m_chunkSize;

    std::vector<std::string> m_chunkPaths;

    struct LineInfo{
        size_t num;
        size_t start;
        size_t strStart;
        size_t finis;
    };

    std::vector<size_t> findChunkBounds(std::ifstream &file, size_t averageChunkSize);
    std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data);
    std::vector<LineInfo> collectChunkInfo(std::vector<char> &data);
    void saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> lineInfo, char* rawData, const std::string &filePath);
    void createSortedChunks();


    static std::string genFilePath(const std::string &folder, const std::string &name, int index);

};

#endif // SORTER_H
