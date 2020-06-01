#include "sorter.h"
#include <algorithm>
#include <iostream>
#include <numeric>

using namespace std;

std::vector<size_t> Sorter::findChunkBounds(ifstream &file, size_t averageChunkSize)
{
    file.seekg(0, file.end);
    size_t fileSize = file.tellg();
    cout << "input file size:" << fileSize << endl;

    vector<size_t> res;
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
        string line;
        getline(file, line); // move to the first '\n' charecter

        res.push_back(file.tellg());

        if(file.tellg() == fileSize) // it was the final line, so just continue
        {
            break;
        }

    }

    cout << "chunkBounds:";
    for (size_t i : res)
        cout << i << " ";
    cout << endl;

    return res;

}

std::vector<size_t> Sorter::sortIndexes(const std::vector<Sorter::LineInfo> &lineData, const std::vector<char> &data)
{
    vector<size_t> idx(lineData.size());
    iota(idx.begin(), idx.end(), 0);
    const char* rawData = data.data();
    sort(idx.begin(), idx.end(),
         [&](size_t l1, size_t l2) {
        int cmp = strcmp(&rawData[lineData[l1].strStart], &rawData[lineData[l2].strStart]);
        if (cmp < 0)
            return true;
        else if (cmp > 0)
            return false;
        else
            return lineData[l1].num > lineData[l2].num;
    });

    return idx;
}

std::vector<Sorter::LineInfo> Sorter::collectChunkInfo(std::vector<char> &data)
{
    vector<LineInfo> lines;
    auto i = data.begin(), end = data.end();
    //end--; //drop the last '\n'
    char* rawData = data.data();
    while(i != end)
    {
        LineInfo lineInfo;
        lineInfo.start = distance(data.begin(), i);
        lineInfo.num =  static_cast<unsigned int>(atoi(&rawData[lineInfo.start]));
        auto dotPosition = find(i, end, '.');
        dotPosition ++; // skip dot
        dotPosition ++; // skip space
        lineInfo.strStart = distance(data.begin(), dotPosition);
        i = find(dotPosition, end, '\n');
        i++;
        lineInfo.finis = distance(data.begin(), i);
        //cout << rawData[lineInfo.strStart] << endl;
        lines.emplace_back(lineInfo);
    }
    return lines;
}

void Sorter::saveSortedChunk(const std::vector<size_t> &idx, const std::vector<Sorter::LineInfo> lineInfo, char *rawData, const string &filePath)
{
    ofstream file(filePath, ios::binary);
    for(size_t i : idx)
    {
        file.write(&rawData[lineInfo[i].start], lineInfo[i].finis - lineInfo[i].start);
    }
    file.close();
}

void Sorter::createSortedChunks()
{
    ifstream file(m_inputFile, ios::binary);
    if(!file)
        throw string("can't open file:") + m_inputFile;

    vector<size_t> chunkBounds = findChunkBounds(file, m_chunkSize);


    file.seekg (0, file.beg);
    for(size_t chunkIndex = 0; chunkIndex < chunkBounds.size() - 1; chunkIndex++)
    {
        size_t chunkSize = chunkBounds[chunkIndex + 1] - chunkBounds[chunkIndex];
        vector<char> data(chunkSize);
        file.read(data.data(), data.size());
        vector<LineInfo> lineData = collectChunkInfo(data);
        vector<size_t> idx = sortIndexes(lineData, data);
        cout <<  lineData[0].num << data[lineData[0].strStart]<< endl;
        //for(size_t i : idx)
        //    cout << i  <<  ": "<< lineData[i].num << data[lineData[i].strStart]<< endl;
        saveSortedChunk(idx, lineData, data.data(), genFilePath(m_cacheFolder, "chunk", chunkIndex));
    }
}

string Sorter::genFilePath(const string &folder, const string &name, int index)
{
    if(folder.empty())
        return name + to_string(index);
    return folder + "/" + name + to_string(index); //todo win support.
}