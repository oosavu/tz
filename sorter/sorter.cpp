#include "sorter.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <queue>

using namespace std;

namespace sorter{

void process(const std::string &m_cacheFolder, const std::string &m_inputFile, const std::string &m_outputFile, const int64_t m_averageChunkSize)
{
    ChunksVector chunkBounds = findChunkBounds(m_inputFile, m_averageChunkSize);

    cout << "input file size:" << chunkBounds.back().second - chunkBounds.front().first << endl;

    int64_t m_averageChunkOfChunkSize = m_averageChunkSize / (chunkBounds.size());

    IterativeFile file(m_inputFile, chunkBounds);
    if (!file.init())
        throw string("can't open file:") + m_inputFile;

    std::vector<std::pair<IterativeFile, IterativeFile>> m_chunks;
    size_t chunkIndex = 0;
    while(file.loadNextChunk())
    {
        vector<LineInfo> lineData = collectLineInfo(file.data);
        vector<size_t> idx = sortIndexes(lineData, file.data);
        m_chunks.emplace_back(saveSortedChunk(idx, lineData, file.data, chunkIndex, m_cacheFolder, m_averageChunkOfChunkSize));
        chunkIndex++;
    }
    file.close();
    merge(m_chunks, m_outputFile);
}

std::vector<std::pair<int64_t, int64_t>> findChunkBounds(const std::string &filePath, int64_t averageChunkSize)
{
    ifstream file(filePath, ios::binary);
    if(!file)
        throw string("can't open file:") + filePath;

    file.seekg(0, file.end);
    int64_t fileSize = file.tellg();

    vector<size_t> keyPoints;
    keyPoints.push_back(0);

    int64_t averageGlobalPos = 0;
    while(true)
    {
        averageGlobalPos += averageChunkSize;
        if(averageGlobalPos >= fileSize)
        {
            keyPoints.push_back(fileSize);
            break;
        }
        file.seekg(averageGlobalPos, file.beg);
        string line;
        getline(file, line); // move to the first '\n' charecter

        keyPoints.push_back(file.tellg());

        if(file.tellg() == fileSize) // it was the final line, so just break
            break;
    }
    file.close();

    cout << "chunkBounds:";
    for (size_t i : keyPoints)
        cout << i << " ";
    cout << endl;

    std::vector<std::pair<int64_t, int64_t>> res;
    for (size_t i = 0; i < keyPoints.size() - 1; i++)
        res.push_back({keyPoints[i],keyPoints[i+1]});
    return res;

}

std::vector<size_t> sortIndexes(const std::vector<LineInfo> &lineData, const std::vector<char> &data)
{
    vector<size_t> idx(lineData.size());
    iota(idx.begin(), idx.end(), 0);
    const char* rawData = data.data();
    sort(idx.begin(), idx.end(),
         [&](size_t l1, size_t l2) {
        int cmp = customSTRCMP(&rawData[lineData[l1].strStart], &rawData[lineData[l2].strStart]);
        if (cmp < 0)
            return true;
        else if (cmp > 0)
            return false;
        else
            return lineData[l1].num < lineData[l2].num;
    });
    return idx;
}

std::vector<LineInfo> collectLineInfo(std::vector<char> &data)
{
    vector<LineInfo> lines;
    auto i = data.begin(), end = data.end();
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
        lines.emplace_back(lineInfo);
    }
    return lines;
}

void merge(std::vector<std::pair<IterativeFile, IterativeFile> > &m_chunks, const string & m_outputFile)
{
    //some cache variables for saving CPU time on vector indexing operations;
    std::vector<char*> currDatas(m_chunks.size());
    std::vector<LineInfo*> currLineInfos(m_chunks.size());
    std::vector<size_t> currLineInfoSizes(m_chunks.size());


    for(size_t i = 0; i < m_chunks.size(); i++)
    {
        if(!m_chunks[i].first.init())
            throw string("can't open file: ") + m_chunks[i].first.filePath;
        m_chunks[i].first.loadNextChunk();
        currDatas[i] = (m_chunks[i].first.data.data());

        if(!m_chunks[i].second.init())
            throw string("can't open file: ") + m_chunks[i].second.filePath;
        m_chunks[i].second.loadNextChunk();
        currLineInfos[i] = reinterpret_cast<LineInfo *>(m_chunks[i].second.data.data());
        currLineInfoSizes[i] = m_chunks[i].second.data.size() / sizeof(LineInfo);
    }

    std::vector<size_t> currLineNum(m_chunks.size(), 0);

    auto compartator = [&](size_t l1, size_t l2) {
        const char* data1 = m_chunks[l1].first.data.data();
        const char* data2 = m_chunks[l2].first.data.data();

        int cmp = customSTRCMP(&data1[currLineInfos[l1]->strStart - m_chunks[l1].first.globalOffset], &data2[currLineInfos[l2]->strStart - m_chunks[l2].first.globalOffset]);
        if (cmp < 0)
            return true;
        else if (cmp > 0)
            return false;
        else
            return currLineInfos[l1]->num > currLineInfos[l2]->num;
    };

    //std::vector<size_t> indexHeap(m_chunks.size());
    //iota(indexHeap.begin(), indexHeap.end(), 0);
    //make_heap(indexHeap.begin(), indexHeap.end(), compartator);

    priority_queue<size_t,  std::vector<size_t>, decltype(compartator)> q(compartator);

    for (size_t i = 0; i < m_chunks.size(); i++)
        q.push(i);

    ofstream outputStream(m_outputFile, ios::binary);
    if(!outputStream)
        throw string("can't open file: ") + m_outputFile;

    while(!q.empty())
    {
        //push_heap(indexHeap.begin(), indexHeap.end());

        //auto maxEl = max_element(indexHeap.begin(), indexHeap.end(), compartator);
        //int poppedIndex = std::distance(indexHeap.begin(), maxEl);

        size_t poppedIndex = q.top();
        q.pop();
        //const LineInfo &lineInfo = m_chunks[poppedIndex].chunkIndexData.at(currLineNum[poppedIndex]);
//        cout << poppedIndex << " " << currLineNum[poppedIndex] << " " << m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset]
//             <<  m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset + 1]
//              <<  m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset + 2] <<  endl;

        outputStream.write(&m_chunks[poppedIndex].first.data.data()[currLineInfos[poppedIndex]->start - m_chunks[poppedIndex].first.globalOffset],
                currLineInfos[poppedIndex]->finis - currLineInfos[poppedIndex]->start);
        outputStream.flush();

        currLineNum.at(poppedIndex) ++;
        currLineInfos.at(poppedIndex) ++;
        if(currLineNum[poppedIndex] * sizeof(LineInfo) >= m_chunks[poppedIndex].second.data.size())
        {
            bool isNext = m_chunks[poppedIndex].first.loadNextChunk() && m_chunks[poppedIndex].second.loadNextChunk();
            currLineNum[poppedIndex] = 0;
            currLineInfos[poppedIndex] = reinterpret_cast<LineInfo *>(m_chunks[poppedIndex].second.data.data());
            if(!isNext)
                continue;
                //indexHeap.pop_back();
        }
        q.push(poppedIndex);
      //  push_heap(indexHeap.begin(), indexHeap.end(), compartator);
    }

    outputStream.close();
}

std::pair<IterativeFile, IterativeFile> saveSortedChunk(const std::vector<size_t> &idx, const std::vector<LineInfo> linesInfo, std::vector<char> &rawData, size_t chunkIndex, const string &m_cacheFolder, size_t m_averageChunkOfChunkSize)
{
    string chunkFilePath = genFilePath(m_cacheFolder, "chunk", chunkIndex);
    ofstream chunkFile(chunkFilePath, ios::binary);
    if(!chunkFile)
        throw string("can't open file:") + chunkFilePath;

    string chunkIndexFilePath = genFilePath(m_cacheFolder, "chunkIndex", chunkIndex);
    ofstream indexFile(chunkIndexFilePath, ios::binary);
    if(!indexFile)
        throw string("can't open file:") + chunkIndexFilePath;


    vector<int64_t> dataKeypoints{0};
    vector<int64_t> indexKeypoints{0};

    size_t averageChunkOfChunkEnd = m_averageChunkOfChunkSize;
    std::vector<LineInfo> sortedLinesInfo;
    size_t globalPosition = 0;
    for(size_t i = 0; i < idx.size(); i++)
    {
        size_t idx_i = idx[i];
        const LineInfo& lineInfo = linesInfo.at(idx_i);
        size_t lineSize = lineInfo.finis - lineInfo.start;
        chunkFile.write(&rawData.data()[lineInfo.start], lineSize);
        sortedLinesInfo.emplace_back(LineInfo{lineInfo.num, globalPosition, globalPosition + lineInfo.strStart - lineInfo.start, globalPosition + lineSize});
        globalPosition += lineSize;
        if(globalPosition > averageChunkOfChunkEnd && i != idx.size() - 1)
        {
            indexFile.write(reinterpret_cast<char*>(sortedLinesInfo.data()), sortedLinesInfo.size() * sizeof(LineInfo));
            sortedLinesInfo.clear();
            averageChunkOfChunkEnd += m_averageChunkOfChunkSize;
            dataKeypoints.push_back(globalPosition);
            indexKeypoints.push_back((i+1) * sizeof(LineInfo));
        }
    }
    dataKeypoints.push_back(globalPosition);
    indexKeypoints.push_back(idx.size() * sizeof(LineInfo));

    ChunksVector dataChunks, indexChunks;
    for(size_t i = 0; i < dataKeypoints.size() - 1; i++)
    {
        dataChunks.push_back({dataKeypoints[i], dataKeypoints[i+1]});
        indexChunks.push_back({indexKeypoints[i], indexKeypoints[i+1]});
    }
    indexFile.write(reinterpret_cast<char*>(sortedLinesInfo.data()), sortedLinesInfo.size() * sizeof(LineInfo));
    chunkFile.close();
    indexFile.close();

    return {IterativeFile(chunkFilePath, dataChunks), IterativeFile(chunkIndexFilePath,indexChunks)};
}


std::string genFilePath(const string &folder, const string &name, int index)
{
    if(folder.empty())
        return name + to_string(index);
    return folder + "/" + name + to_string(index); //todo win support.
}


//we need to stop strcmp after the \n (not \00)
//copypaste from glibc/string/strcmp.c
char customSTRCMP(const char *p1, const char *p2)
{
    const unsigned char *s1 = (const unsigned char *) p1;
    const unsigned char *s2 = (const unsigned char *) p2;
    unsigned char c1, c2;
    do
    {
        c1 = (unsigned char) *s1++;
        c2 = (unsigned char) *s2++;
        if (c1 == '\n') // '\0'
            return c1 - c2;
    }
    while (c1 == c2);
    return c1 - c2;
}


IterativeFile::IterativeFile(const string &filePath, const ChunksVector &chunksInfo) :
    filePath(filePath),
    chunksInfo(chunksInfo)
{
}

bool IterativeFile::init()
{
    file.open(filePath, ios::binary);
    if(!file)
        return false;
    indexOfChunk = -1;
    return true;
}

bool IterativeFile::loadNextChunk()
{
    indexOfChunk ++;
    if(indexOfChunk >= chunksInfo.size())
        return false;

    globalOffset = chunksInfo[indexOfChunk].first;
    currSize = chunksInfo[indexOfChunk].second - chunksInfo[indexOfChunk].first;
    data.resize(currSize);
    file.read(data.data(), data.size());
    return true;
}

void IterativeFile::close()
{
    data.clear();
    file.close();
}
}
