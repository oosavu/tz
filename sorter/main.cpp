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
#include <cstring>

// it's bad practice to use namespace std, however for test job it's ok
using namespace std;

std::vector<size_t> findChunkBounds(std::ifstream &file, size_t averageChunkSize)
{
    file.seekg (0, file.end);
    size_t fileSize = file.tellg();
    cout << "fileSize:" << fileSize << endl;

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

struct LineInfo{
    unsigned int num;
    size_t start;
    size_t strStart;
    size_t end;
};

std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data)
{
    std::vector<size_t> idx(lineData.size());
    iota(idx.begin(), idx.end(), 0);
    const char* rawData = data.data();
    sort(idx.begin(), idx.end(),
         [&](size_t l1, size_t l2) {
        if (std::strcmp(&rawData[lineData[l1].strStart], &rawData[lineData[l2].strStart]) > 0)
            return true;
        return lineData[l1].num > lineData[l2].num;
    });

    return idx;
}

std::vector<LineInfo> collectChunkInfo(std::vector<char> &data)
{
    std::vector<LineInfo> lines;
    auto it = std::find(data.begin(), data.end(), '\n');
    char* rawData = data.data();
    for (; it != data.end(); ++it)
    {
        LineInfo lineInfo;
        lineInfo.start = std::distance(data.begin(), it);
        cout << lineInfo.start <<endl;
        //size_t lineEnd = distance(data.begin(), it);
        // std::from_chars() does not supported in gcc75
        // std::string_view does not supported by stoi
        // stoi does not support char*
        lineInfo.num =  static_cast<unsigned int>(std::atoi(&rawData[lineInfo.start]));
        lineInfo.strStart = lineInfo.start + 1; // minimum one charecter for digit
        while(rawData[lineInfo.strStart] != '.')
            lineInfo.strStart++;
        lineInfo.strStart++;
        if(lines.size() != 0)
            lines[lines.size() - 1].end = lineInfo.start;
        lines.emplace_back(lineInfo);
    }
    lines.back().end = data.size();
    return lines;
}

void saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> lineInfo, char* rawData, const std::string &filePath)
{
    std::ofstream file(filePath, std::ios::binary);
    for(size_t i : idx)
    {
        file.write(&rawData[lineInfo[i].start], lineInfo[i].end - lineInfo[i].start);
    }
    file.close();
}

void sortChunks(std::ifstream &file, std::vector<size_t> chunkBounds)
{
    file.seekg (0, file.beg);
    for(size_t chunkIndex = 0; chunkIndex < chunkBounds.size() - 1; chunkIndex++)
    {
        size_t chunkSize = chunkBounds[chunkIndex + 1] - chunkBounds[chunkIndex];
        std::vector<char> data(chunkSize);
        file.read(data.data(), data.size());
        std::vector<LineInfo> lineData = collectChunkInfo(data);
        std::vector<size_t> idx = sortIndexes(lineData, data);
        saveSortedChunk(idx, lineData, data.data(), "/home/oosavu/qwe.txt");
    }
}

void createSortedChunks(const std::string &filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if(!file)
        throw std::string("can't open file:") + filePath.c_str();
    std::vector<size_t> chunkBounds = findChunkBounds(file, 1000000);
    sortChunks(file, chunkBounds);
}


int main()
{
    try {
        createSortedChunks("/home/oosavu/example.bin");
    } catch (std::string s) {
        std::cerr << "ERROR:" << s << endl;;
        return -1;
    }
    return 0;
}
