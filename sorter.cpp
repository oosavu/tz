#include "sorter.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <queue>
#include <chrono>
#include <execution>
#include <future>
#include <memory>
#include <stdio.h>
#include "asyncfile.h"
#include "spdlog/spdlog.h"

using namespace std;

namespace sorter{

vector<IterativeFile> asyncChunkSort(const string &inputFile, const ChunkBoundsVector &chunkBounds, const string &cacheFolder, const int64_t averageChunkSize)
{
    int64_t averageChunkOfChunkSize = averageChunkSize / (chunkBounds.size());
    Semaphore taskCounter(4); // memory limit

    auto sortFunctor = [&](int indexOfChunk) -> pair<string, ChunkBoundsVector>{
        std::string debugID = to_string(indexOfChunk) + "[" + to_string(chunkBounds[indexOfChunk].first) + "-" + to_string(chunkBounds[indexOfChunk].second) + "]";

        taskCounter.wait();
        spdlog::info("wake up thread for " + debugID);
        int64_t currSize = chunkBounds[indexOfChunk].second - chunkBounds[indexOfChunk].first;

        vector<char> data;
        vector<LineInfo> lineData;

        spdlog::info("start read part for " + debugID);

        ifstream file(inputFile, ios::binary);
        if (!file)
        {
            spdlog::error("{}: can't open file: {}", debugID, inputFile);
            return {"", ChunkBoundsVector()};
        }
        data.resize(currSize);
        if(!ElementaryFileOperations::read(file, chunkBounds[indexOfChunk].first, chunkBounds[indexOfChunk].second, data))
        {
            spdlog::error("{}: can't read file: {}", debugID, inputFile);
            return {"", ChunkBoundsVector()};
        }
        file.close();

        spdlog::info("start collectLineInfo " + debugID);
        lineData = collectLineInfo(data);
        spdlog::info("start sort for " + debugID);
        vector<size_t> idx = sortIndexes(lineData, data);
        ChunkBoundsVector bounds;
        string chunkFilePath = genFilePath(cacheFolder, "chunk", indexOfChunk);
        spdlog::info("start save " + debugID);
        bounds = saveSortedChunk(idx, lineData, data, chunkFilePath, averageChunkOfChunkSize);

        taskCounter.notify();
        spdlog::info("finis " + debugID);
        return {chunkFilePath, bounds};
    };
    vector<future<pair<string, ChunkBoundsVector>>> results;
    for(size_t i = 0; i < chunkBounds.size(); i++)
        results.emplace_back(async(launch::async, sortFunctor, i));

    vector<IterativeFile> res;
    for(auto &futureRes : results)
    {
        auto params = futureRes.get();
        res.emplace_back(IterativeFile(params.first, params.second));
    }

    return res;
}


void sortBigFile(const string &cacheFolder, const string &inputFile, const string &outputFile, const int64_t averageChunkSize)
{
    TimeTracker tracker;
    tracker.start();

    ChunkBoundsVector chunkBounds = findChunkBounds(inputFile, averageChunkSize);
    int64_t fileSize = chunkBounds.back().second - chunkBounds.front().first;
    spdlog::info("input file size:{}. chunks:{}", fileSize, chunkBounds.size());

    //    if(chunkBounds.size() == 1)
    //    {
    //          TODO in real program (not in a test case for a job :) ) we must avoid double sorting for small files;
    //    }

    vector<IterativeFile> chunkFiles = asyncChunkSort(inputFile, chunkBounds, cacheFolder, averageChunkSize);
    spdlog::info("create chunks time: {}ms", tracker.elapsed());

    merge(chunkFiles, outputFile);


    for(auto &filePair : chunkFiles)
        remove(filePair.filePath().c_str());

    int time = tracker.elapsed();
    float bytesPerSecond = float(fileSize) / float(float(max(time, 1)) / 1000.0f);
    spdlog::info("FINISH. time: {}ms. speed:{}bytes/sec", time, bytesPerSecond);
}

ChunkBoundsVector findChunkBounds(const string &filePath, int64_t averageChunkSize)
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

    ChunkBoundsVector res;
    for (size_t i = 0; i < keyPoints.size() - 1; i++)
        res.push_back({keyPoints[i],keyPoints[i+1]});
    return res;

}

vector<size_t> sortIndexes(const vector<LineInfo> &lineData, const vector<char> &data)
{
    vector<size_t> idx(lineData.size());
    iota(idx.begin(), idx.end(), 0);
    const char* rawData = data.data();
    sort(execution::par,idx.begin(), idx.end(),
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

// collect info about lines in format: "NUMBER: STRING\n"
vector<LineInfo> collectLineInfo(vector<char> &data)
{
    vector<LineInfo> lines;
    auto i = data.begin(), end = data.end();
    char* rawData = data.data();
    while(i != end)
    {
        LineInfo lineInfo;
        lineInfo.start = distance(data.begin(), i);//i - data.begin();  //distance(data.begin(), i);////
        lineInfo.num =  static_cast<unsigned int>(atoi(&rawData[lineInfo.start]));
        auto dotPosition = find(i, end, '.');
        dotPosition ++; // skip dot
        dotPosition ++; // skip space
        lineInfo.strStart = distance(data.begin(), dotPosition);//dotPosition - data.begin();
        i = find(dotPosition, end, '\n');
        i++;
        lineInfo.finis = i - data.begin(); //distance(data.begin(), i);// i - data.begin(); //
        lines.emplace_back(lineInfo);
    }
    return lines;
}


void merge(vector<IterativeFile> &iterativeChunks, const string & outputFile)
{
    spdlog::info("start merge ...");

    vector<PreparedChunk> curredChunks(iterativeChunks.size());
    vector<future<PreparedChunk>> futureChunks(iterativeChunks.size());

    int totalChunks = 0;
    for(IterativeFile &file : iterativeChunks)
        totalChunks += file.chunksCount();

    atomic<int> loadedChunksCount = 0;
    auto prepareChunkFunctor = [&](int i) -> PreparedChunk{
        PreparedChunk res;
        if(!iterativeChunks[i].loadNextChunk(res.data))
        {
            res.isValid = false;
            return res;
        }
        res.isValid = true;
        res.currDataPointer = res.data.data();
        res.lineInfo = collectLineInfo(res.data);
        res.lineIterator = res.lineInfo.begin();
        loadedChunksCount++;
        spdlog::info("loaded {}/{}", loadedChunksCount, totalChunks);
        return res;
    };

    auto compartator = [&](size_t l1, size_t l2) {
        PreparedChunk &p1 = curredChunks[l1];
        PreparedChunk &p2 = curredChunks[l2];
        const char* data1 = p1.currDataPointer + p1.lineIterator->strStart - p1.lineIterator->start;
        const char* data2 = p2.currDataPointer + p2.lineIterator->strStart - p2.lineIterator->start;

        int cmp = customSTRCMP(data1 , data2);
        if (cmp > 0)
            return true;
        else if (cmp < 0)
            return false;
        else
            return p1.lineIterator->num > p1.lineIterator->num;
    };

    //load first
    for(size_t i = 0; i < iterativeChunks.size(); i++)
    {
        if(!iterativeChunks[i].init())
            throw string("can't open file: ") + iterativeChunks[i].filePath();
        futureChunks[i] = async(prepareChunkFunctor, i);
    }

    //wait first and load next
    for(size_t i = 0; i < iterativeChunks.size(); i++)
    {
        curredChunks[i] = futureChunks[i].get();
        if(!curredChunks[i].isValid)
            throw string("can't load first chunk: ") + iterativeChunks[i].filePath();
        futureChunks[i] = async(prepareChunkFunctor, i);
    }

    // init queue
    priority_queue<size_t,  vector<size_t>, decltype(compartator)> q(compartator);
    for (size_t i = 0; i < iterativeChunks.size(); i++)
        q.push(i);

    //init output file
    AsyncOstream outputStream(outputFile);
    if(!outputStream.isValid())
        throw string("can't open file: ") + outputFile;

    while(!q.empty())
    {
        size_t poppedIndex = q.top();
        q.pop();

        // uncomment for debug..
        //        string pstr = to_string(poppedIndex) + " " + to_string(currLineInfos[poppedIndex]->finis - currLineInfos[poppedIndex]->strStart) + " ";
        //        outputStream.write(pstr.data(), pstr.size());

        PreparedChunk &chunk = curredChunks[poppedIndex];
        outputStream.write(chunk.currDataPointer,
                           chunk.lineIterator->finis - chunk.lineIterator->start);

        chunk.currDataPointer += chunk.lineIterator->finis - chunk.lineIterator->start;
        chunk.lineIterator ++;

        if(chunk.lineIterator == chunk.lineInfo.end())
        {
            curredChunks[poppedIndex] = futureChunks[poppedIndex].get();
            if(curredChunks[poppedIndex].isValid)
            {
                futureChunks[poppedIndex] = async(prepareChunkFunctor, poppedIndex);
            }
            else
            {
                iterativeChunks[poppedIndex].close();
                continue;
            }

        }
        q.push(poppedIndex);
    }
}



string genFilePath(const string &folder, const string &name, int index)
{
    if(folder.empty())
        return name + to_string(index);
    return folder + "\\" + name + to_string(index);
}


char customSTRCMP(const char *p1, const char *p2)
{
    const unsigned char *s1 = (const unsigned char *) p1;
    const unsigned char *s2 = (const unsigned char *) p2;
    unsigned char c1, c2;
    do
    {
        c1 = (unsigned char) *s1++;
        c2 = (unsigned char) *s2++;
        if (c1 == '\n') // '\n' insted of '\0'!!!
            return c1 - c2;
    }
    while (c1 == c2);
    return c1 - c2;
}


IterativeFile::IterativeFile(const string &filePath, const ChunkBoundsVector &chunksInfo) :
    m_filePath(filePath),
    m_chunksInfo(chunksInfo)
{
}

bool IterativeFile::init()
{
    m_file.open(m_filePath, ios::binary);
    if(!m_file)
        return false;
    m_indexOfChunk = -1;
    return true;
}

bool IterativeFile::loadNextChunk(std::vector<char> &data)
{
    m_indexOfChunk ++;
    if(m_indexOfChunk >= m_chunksInfo.size())
        return false;

    if(!ElementaryFileOperations::read(m_file, m_chunksInfo[m_indexOfChunk].first, m_chunksInfo[m_indexOfChunk].second, data))
        throw string("read error:") + m_filePath;
    return true;
}

int IterativeFile::currentChunk()
{
    return m_indexOfChunk;
}

int IterativeFile::chunksCount()
{
    return m_chunksInfo.size();
}

void IterativeFile::close()
{
    m_file.close();
}

const std::string IterativeFile::filePath()
{
    return m_filePath;
}

void TimeTracker::start()
{
    startTime = chrono::system_clock::now();
}

int TimeTracker::elapsed()
{
    chrono::system_clock::time_point currentTime = chrono::system_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();
}

ChunkBoundsVector saveSortedChunk(const vector<size_t> &idx, const vector<LineInfo> &linesInfo, vector<char> &rawData, const string &chunkFilePath, size_t averageChunkOfChunkSize)
{
    AsyncOstream stream(chunkFilePath);
    if(!stream.isValid())
        throw string("ERROR: can't open file:") + chunkFilePath;

    vector<int64_t> dataKeypoints{0};

    size_t averageChunkOfChunkEnd = averageChunkOfChunkSize;
    size_t globalPosition = 0;
    for(size_t i = 0; i < idx.size(); i++)
    {
        size_t idx_i = idx[i];
        const LineInfo& lineInfo = linesInfo.at(idx_i);
        size_t lineSize = lineInfo.finis - lineInfo.start;
        stream.write(&rawData.data()[lineInfo.start], lineSize);
        globalPosition += lineSize;
        if(globalPosition > averageChunkOfChunkEnd && i != idx.size() - 1)
        {
            averageChunkOfChunkEnd += averageChunkOfChunkSize;
            dataKeypoints.push_back(globalPosition);
        }
    }
    dataKeypoints.push_back(globalPosition);

    ChunkBoundsVector dataChunks;
    for(size_t i = 0; i < dataKeypoints.size() - 1; i++)
        dataChunks.push_back({dataKeypoints[i], dataKeypoints[i+1]});
    return dataChunks;
}

Semaphore::Semaphore(int count) :
    m_count(count){}

void Semaphore::notify() {
    lock_guard<decltype(m_mutex)> lock(m_mutex);
    ++m_count;
    m_condition.notify_one();
}

void Semaphore::wait() {
    unique_lock<decltype(m_mutex)> lock(m_mutex);
    while(!m_count) // Handle spurious wake-ups.
        m_condition.wait(lock);
    --m_count;
}


}
