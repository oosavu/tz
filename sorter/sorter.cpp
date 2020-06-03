#include "sorter.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <queue>
using namespace std;

void Sorter::process()
{
    ifstream file(m_inputFile, ios::binary);
    if(!file)
        throw string("can't open file:") + m_inputFile;

    vector<size_t> chunkBounds = findChunkBounds(file, m_averageChunkSize);
    m_averageChunkOfChunkSize = m_averageChunkSize / (chunkBounds.size() - 1);

    file.seekg (0, file.beg);
    for(size_t chunkIndex = 0; chunkIndex < chunkBounds.size() - 1; chunkIndex++)
    {
        size_t chunkSize = chunkBounds[chunkIndex + 1] - chunkBounds[chunkIndex];
        vector<char> data(chunkSize);
        file.read(data.data(), data.size());
        vector<LineInfo> lineData = collectLineInfo(data);
        vector<size_t> idx = sortIndexes(lineData, data);
        saveSortedChunk(idx, lineData, data, chunkIndex);
    }
    file.close();


    merge();
}

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

        if(file.tellg() == fileSize) // it was the final line, so just break
            break;
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

std::vector<Sorter::LineInfo> Sorter::collectLineInfo(std::vector<char> &data)
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

void Sorter::merge()
{
    for(size_t i = 0; i < m_chunks.size(); i++)
    {
        m_chunks[i].init();
        m_chunks[i].loadNextChunk();
    }

    std::vector<size_t> currLineNum(m_chunks.size(), 0);

    auto compartator = [&](size_t l1, size_t l2) {
        const IterativeFile &chunk1 = m_chunks[l1];
        const IterativeFile &chunk2 = m_chunks[l2];
        const char* data1 = chunk1.chunkData.data();
        const char* data2 = chunk2.chunkData.data();
        const LineInfo &lineInfol1 = chunk1.chunkIndexData[currLineNum[l1]];
        const LineInfo &lineInfol2 = chunk2.chunkIndexData[currLineNum[l2]];

        int cmp = customSTRCMP(&data1[lineInfol1.strStart - chunk1.byteGlobalOffset], &data2[lineInfol2.strStart - chunk2.byteGlobalOffset]);
        if (cmp < 0)
            return true;
        else if (cmp > 0)
            return false;
        else
            return lineInfol1.num > lineInfol2.num;
    };

    //std::vector<size_t> indexHeap(m_chunks.size());
    //iota(indexHeap.begin(), indexHeap.end(), 0);
    //make_heap(indexHeap.begin(), indexHeap.end(), compartator);
    priority_queue<size_t,  std::vector<size_t>, decltype(compartator)> q(compartator);

    for (int i = 0; i < m_chunks.size(); i++)
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
        const LineInfo &lineInfo = m_chunks[poppedIndex].chunkIndexData.at(currLineNum[poppedIndex]);
//        cout << poppedIndex << " " << currLineNum[poppedIndex] << " " << m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset]
//             <<  m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset + 1]
//              <<  m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset + 2] <<  endl;

        outputStream.write(&m_chunks[poppedIndex].chunkData.data()[lineInfo.start - m_chunks[poppedIndex].byteGlobalOffset], lineInfo.finis - lineInfo.start);
        outputStream.flush();

        currLineNum.at(poppedIndex) ++;
        if(currLineNum[poppedIndex] >= m_chunks[poppedIndex].chunkIndexData.size())
        {
            currLineNum[poppedIndex] = 0;
            if(!m_chunks[poppedIndex].loadNextChunk())
            {
                continue;
            }
                //indexHeap.pop_back();
        }
        q.push(poppedIndex);
      //  push_heap(indexHeap.begin(), indexHeap.end(), compartator);
    }

    outputStream.close();
}

void Sorter::saveSortedChunk(const std::vector<size_t> &idx, const std::vector<Sorter::LineInfo> linesInfo, std::vector<char> &rawData, size_t chunkIndex)
{
    string chunkFilePath = genFilePath(m_cacheFolder, "chunk", chunkIndex);
    ofstream chunkFile(chunkFilePath, ios::binary);
    if(!chunkFile)
        throw string("can't open file:") + chunkFilePath;

    string chunkIndexFilePath = genFilePath(m_cacheFolder, "chunkIndex", chunkIndex);
    ofstream indexFile(chunkIndexFilePath, ios::binary);
    if(!indexFile)
        throw string("can't open file:") + chunkIndexFilePath;

    vector<ChunkOfChunkInfo> chunkOfChunksBound;
    chunkOfChunksBound.push_back(ChunkOfChunkInfo{0, 0, 0, 0});

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
            chunkOfChunksBound.push_back(ChunkOfChunkInfo{globalPosition, i + 1, 0, 0});
        }
    }
    for(size_t i = 0; i < chunkOfChunksBound.size() - 1; i++)
    {
        chunkOfChunksBound[i].finisByte = chunkOfChunksBound[i + 1].startByte;
        chunkOfChunksBound[i].finisLineInfoIndex = chunkOfChunksBound[i + 1].startLineInfoIndex;
    }
    chunkOfChunksBound.back().finisByte = globalPosition;
    chunkOfChunksBound.back().finisLineInfoIndex = idx.size();
    chunkFile.close();
    indexFile.write(reinterpret_cast<char*>(sortedLinesInfo.data()), sortedLinesInfo.size() * sizeof(LineInfo));
    ofstream indexTextFile(genFilePath(m_cacheFolder, "textchunkIndex", chunkIndex) +  to_string(chunkOfChunksBound.size() - 1));

    indexFile.close();

//    cout << "chunkOfChunksBounds " << chunkIndex << ": ";
//    for (ChunkOfChunkInfo &coc: chunkOfChunksBound)
//        cout << "[" << coc.startByte << "-" << coc.finisByte << "] ";
//    cout << endl;

    m_chunks.push_back(IterativeFile(chunkFilePath, chunkIndexFilePath, chunkOfChunksBound));
}


std::string Sorter::genFilePath(const string &folder, const string &name, int index)
{
    if(folder.empty())
        return name + to_string(index);
    return folder + "/" + name + to_string(index); //todo win support.
}


//we need to stop strcmp after the \n (not \00)
//copypaste from glibc/string/strcmp.c
char Sorter::customSTRCMP(const char *p1, const char *p2)
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


Sorter::IterativeFile::IterativeFile(const string &dataFilePath, const string &indexFilePath, const std::vector<Sorter::ChunkOfChunkInfo> chunksInfo) :
    dataFilePath(dataFilePath),
    indexFilePath(indexFilePath),
    chunksInfo(chunksInfo)
{}

void Sorter::IterativeFile::init()
{
    dataFile.open(dataFilePath, ios::binary);
    if(!dataFile)
        throw string("can't open file: ") + dataFilePath;

    indexFile.open(indexFilePath, ios::binary);
    if(!indexFile)
        throw string("can't open file: ") + indexFilePath;
    indexOfChunk = -1;
}

bool Sorter::IterativeFile::loadNextChunk()
{
    indexOfChunk ++;
    if(indexOfChunk >= chunksInfo.size())
        return false;

    indexGlobalOffset = chunksInfo[indexOfChunk].startLineInfoIndex;
    byteGlobalOffset = chunksInfo[indexOfChunk].startByte;

    currChunkIndexSize = chunksInfo[indexOfChunk].finisLineInfoIndex - chunksInfo[indexOfChunk].startLineInfoIndex;
    currChunkSize = chunksInfo[indexOfChunk].finisByte - chunksInfo[indexOfChunk].startByte;

    chunkData.resize(currChunkSize);
    dataFile.read(chunkData.data(), chunkData.size());

    chunkIndexData.resize(currChunkIndexSize );
    indexFile.read(reinterpret_cast<char*>(chunkIndexData.data()), chunkIndexData.size() * sizeof(LineInfo));
    return true;
}

