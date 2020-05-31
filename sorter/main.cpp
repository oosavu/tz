#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <random>
#include <cmath>
#include <array>
#include <fstream>
#include <tuple>
#include <algorithm>
#include <string_view>

std::vector<size_t> findChunkBounds(std::ifstream &file, size_t averageChunkSize)
{
    file.seekg (0, file.end);
    size_t fileSize = file.tellg();

    std::vector<size_t> res;
    res.push_back(0);

    size_t averageGlobalPos = 0;
    while(true)
    {
        averageGlobalPos += averageChunkSize;
        if(averageGlobalPos >= fileSize)
        {
            res.push_back(fileSize);
            break;
        }
        file.seekg(averageGlobalPos, file.beg);
        std::string line;
        std::getline(file, line);
        res.push_back(file.tellg());
        if(file.eof())
            break;
    }
    return res;
}

struct ChunkIndex{
    size_t byteSize;

};

void sortChunk(std::vector<char> &data)
{
    struct LineInfo{
        unsigned int num;
        size_t strStart;
        size_t strEnd;
    };

    std::vector<LineInfo> lines;
    auto it = std::find(data.begin(), data.end(), '\n');
    size_t lineStart = 0;
    char* rawData = data.data();
    for (; it != data.end(); ++it)
    {
        size_t lineEnd = distance(data.begin(), it);
        std::string_view numStr(&rawData[lineStart], lineEnd);
        std::stoi("sdf");
    }
}

void sortChunks(std::ifstream &file, std::vector<size_t> chunkBounds)
{
    for(size_t chunkIndex = 0; chunkIndex < chunkBounds.size() - 1; chunkIndex++)
    {
        size_t chunkSize = chunkBounds[chunkIndex + 1] - chunkBounds[chunkIndex];
        std::vector<char> data(chunkSize);
        file.read(data.data(), data.size());
        sortChunk(data);
    }
}



int createSortedChunk(std::ifstream &file, size_t startPos, size_t maxChunkSize)
{
    file.seekg (0, file.end);
    size_t fileLength = file.tellg();

    file.seekg(startPos);
    std::string startLine;
    std::getline(file, startLine);

    size_t chunkStart = file.tellg();
    size_t maxSize = std::min(fileLength - chunkStart, maxChunkSize);
    std::vector<char> data(maxSize);
    file.read(data.data(), data.size());

    auto lastStringEndPos = std::find_if(data.rbegin(), data.rend(), [](char i) { return i == '\n'; });
    int chunkLen = std::distance(lastStringEndPos, data.rend());
    if (lastStringEndPos.)


    size_t pos = startPos;

}


int main()
{
    cout << "Hello World!" << endl;
    return 0;
}
