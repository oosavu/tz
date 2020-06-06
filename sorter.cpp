#include "sorter.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <queue>
#include <chrono>
#include <execution>
#include <future>
#include "asyncfile.h"
#include <mutex>
#include <condition_variable>
using namespace std;
namespace sorter{

vector<IterativeFile> asyncChunkSort(const string &inputFile, const ChunksVector &chunkBounds, const string &cacheFolder, const int64_t averageChunkSize)
{
    int64_t averageChunkOfChunkSize = averageChunkSize / (chunkBounds.size());
    Semaphore taskCounter(3); // memory limit
    mutex fileSystemMutex;

    auto sortFunctor = [&](int indexOfChunk) -> pair<string, ChunksVector>{
        cout << "start sort " << indexOfChunk << endl;
        taskCounter.wait();
        int64_t globalOffset = chunkBounds[indexOfChunk].first;
        int64_t currSize = chunkBounds[indexOfChunk].second - chunkBounds[indexOfChunk].first;

        vector<char> data(currSize);
        vector<LineInfo> lineData;
        {
            unique_lock<mutex> locker(fileSystemMutex);
            cout << "start read part " << indexOfChunk << endl;
            ifstream file(inputFile, ios::binary);
            if (!file)
                throw string("can't open file:") + inputFile;
            file.seekg(globalOffset);
            data.resize(currSize);
            if(!file.read(data.data(), data.size()))
                throw string("read error:") + inputFile;
            file.close();
        }
        cout << "start sort " << indexOfChunk << endl;
        lineData = collectLineInfo(data);
        vector<size_t> idx = sortIndexes(lineData, data);
        ChunksVector bounds;
        string chunkFilePath = genFilePath(cacheFolder, "chunk", indexOfChunk);
        {
            unique_lock<mutex> locker(fileSystemMutex);
            cout << "start save " << indexOfChunk << endl;
            bounds = saveSortedChunk(idx, lineData, data, chunkFilePath, averageChunkOfChunkSize);
        }
        taskCounter.notify();
        cout << "finis " << indexOfChunk << endl;
        return {chunkFilePath, bounds};
    };
    vector<future<pair<string, ChunksVector>>> results;
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

    ChunksVector chunkBounds = findChunkBounds(inputFile, averageChunkSize);
    int64_t fileSize = chunkBounds.back().second - chunkBounds.front().first;
    cout << "input file size:" << fileSize << " chunks:" << chunkBounds.size() << endl;

    if(chunkBounds.size() == 1)
    {
        int time = tracker.elapsed();
        float bytesPerSecond = float(fileSize) / float(float(max(time, 1)) / 1000.0f);
        cout << "FINISH. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
        return;
    }

    vector<IterativeFile> chunkFiles = asyncChunkSort(inputFile, chunkBounds, cacheFolder, averageChunkSize);
    merge(chunkFiles, outputFile);

    cout << "create chunks time:" << tracker.elapsed() << endl;

    //merge(m_chunks, outputFile);

    for(auto &filePair : chunkFiles)
        remove(filePair.filePath().c_str());

    int time = tracker.elapsed();
    float bytesPerSecond = float(fileSize) / float(float(max(time, 1)) / 1000.0f);
    cout << "FINISH. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
    return;


}

vector<pair<int64_t, int64_t>> findChunkBounds(const string &filePath, int64_t averageChunkSize)
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

    //    cout << "chunkBounds:";
    //    for (size_t i : keyPoints)
    //        cout << i << " ";
    //    cout << endl;

    vector<pair<int64_t, int64_t>> res;
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


void merge(vector<IterativeFile> &iterativeChunks, const string & outputFile)
{
    vector<char*> currDatas(iterativeChunks.size());
    vector<vector<LineInfo>> currLineInfos(iterativeChunks.size());
    vector<vector<LineInfo>::iterator> iterators(iterativeChunks.size());

    for(size_t i = 0; i < iterativeChunks.size(); i++)
    {
        if(!iterativeChunks[i].init())
            throw string("can't open file: ") + iterativeChunks[i].filePath();
        iterativeChunks[i].loadNextChunk();
        currDatas[i] = iterativeChunks[i].data.data();
        currLineInfos[i] = collectLineInfo(iterativeChunks[i].data);
        iterators[i] = currLineInfos[i].begin();
    }

    auto compartator = [&](size_t l1, size_t l2) {
        const char* data1 = currDatas[l1] + iterators[l1]->strStart - iterators[l1]->start;
        const char* data2 = currDatas[l2] + iterators[l2]->strStart - iterators[l2]->start;

        int cmp = customSTRCMP(data1 , data2);
        if (cmp > 0)
            return true;
        else if (cmp < 0)
            return false;
        else
            return iterators[l1]->num > iterators[l2]->num;
    };

    priority_queue<size_t,  vector<size_t>, decltype(compartator)> q(compartator);

    for (size_t i = 0; i < iterativeChunks.size(); i++)
        q.push(i);

    AsyncOstream outputStream(outputFile);
    if(!outputStream.isValid())
        throw string("can't open file: ") + outputFile;

    while(!q.empty())
    {
        size_t poppedIndex = q.top();
        q.pop();

        //        string pstr = to_string(poppedIndex) + " " + to_string(currLineInfos[poppedIndex]->finis - currLineInfos[poppedIndex]->strStart) + " ";
        //        outputStream.write(pstr.data(), pstr.size());

        outputStream.write(currDatas[poppedIndex],
                           iterators[poppedIndex]->finis - iterators[poppedIndex]->start);

        currDatas.at(poppedIndex) += iterators[poppedIndex]->finis - iterators[poppedIndex]->start; //currLineInfos[poppedIndex]->start - m_chunks[poppedIndex].first.globalOffset;
        iterators.at(poppedIndex) ++;

        if(iterators.at(poppedIndex) == currLineInfos.at(poppedIndex).end())
        {
            bool isNext = iterativeChunks[poppedIndex].loadNextChunk();
            currDatas[poppedIndex] = iterativeChunks[poppedIndex].data.data();
            currLineInfos[poppedIndex] = collectLineInfo(iterativeChunks[poppedIndex].data);
            iterators[poppedIndex] = currLineInfos[poppedIndex].begin();
            if(!isNext)
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
        if (c1 == '\n') // '\0'
            return c1 - c2;
    }
    while (c1 == c2);
    return c1 - c2;
}


IterativeFile::IterativeFile(const string &filePath, const ChunksVector &chunksInfo) :
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

bool IterativeFile::loadNextChunk()
{
    m_indexOfChunk ++;
    if(m_indexOfChunk >= m_chunksInfo.size())
        return false;

    //globalOffset = chunksInfo[indexOfChunk].first;
    //currSize = ;
    data.resize(m_chunksInfo[m_indexOfChunk].second - m_chunksInfo[m_indexOfChunk].first);
    if(!m_file.read(data.data(), data.size()))
        throw string("read error:") + m_filePath;
    return true;
}

void IterativeFile::close()
{
    data.clear();
    m_file.close();
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

ChunksVector saveSortedChunk(const vector<size_t> &idx, const vector<LineInfo> &linesInfo, vector<char> &rawData, const string &chunkFilePath, size_t m_averageChunkOfChunkSize)
{
    AsyncOstream stream(chunkFilePath);
    if(!stream.isValid())
        throw string("ERROR: can't open file:") + chunkFilePath;

    vector<int64_t> dataKeypoints{0};

    size_t averageChunkOfChunkEnd = m_averageChunkOfChunkSize;
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
            averageChunkOfChunkEnd += m_averageChunkOfChunkSize;
            dataKeypoints.push_back(globalPosition);
        }
    }
    dataKeypoints.push_back(globalPosition);

    ChunksVector dataChunks;
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



//pair<IterativeFile, IterativeFile> saveSortedChunk(const vector<size_t> &idx, const vector<LineInfo> linesInfo, vector<char> &rawData, const string &chunkFilePath, const string &chunkIndexFilePath, size_t m_averageChunkOfChunkSize)
//{
//    ofstream chunkFile(chunkFilePath, ios::binary);
//    if(!chunkFile)
//        throw string("can't open file:") + chunkFilePath;

//    ofstream indexFile(chunkIndexFilePath, ios::binary);
//    if(!indexFile)
//        throw string("can't open file:") + chunkIndexFilePath;


//    vector<int64_t> dataKeypoints{0};
//    vector<int64_t> indexKeypoints{0};

//    size_t averageChunkOfChunkEnd = m_averageChunkOfChunkSize;
//    vector<LineInfo> sortedLinesInfo;
//    size_t globalPosition = 0;
//    for(size_t i = 0; i < idx.size(); i++)
//    {
//        size_t idx_i = idx[i];
//        const LineInfo& lineInfo = linesInfo.at(idx_i);
//        size_t lineSize = lineInfo.finis - lineInfo.start;
//        chunkFile.write(&rawData.data()[lineInfo.start], lineSize);
//        sortedLinesInfo.emplace_back(LineInfo{lineInfo.num, globalPosition, globalPosition + lineInfo.strStart - lineInfo.start, globalPosition + lineSize});
//        globalPosition += lineSize;
//        if(globalPosition > averageChunkOfChunkEnd && i != idx.size() - 1)
//        {
//            indexFile.write(reinterpret_cast<char*>(sortedLinesInfo.data()), sortedLinesInfo.size() * sizeof(LineInfo));
//            sortedLinesInfo.clear();
//            averageChunkOfChunkEnd += m_averageChunkOfChunkSize;
//            dataKeypoints.push_back(globalPosition);
//            indexKeypoints.push_back((i+1) * sizeof(LineInfo));
//        }
//    }
//    dataKeypoints.push_back(globalPosition);
//    indexKeypoints.push_back(idx.size() * sizeof(LineInfo));

//    ChunksVector dataChunks, indexChunks;
//    for(size_t i = 0; i < dataKeypoints.size() - 1; i++)
//    {
//        dataChunks.push_back({dataKeypoints[i], dataKeypoints[i+1]});
//        indexChunks.push_back({indexKeypoints[i], indexKeypoints[i+1]});
//    }
//    indexFile.write(reinterpret_cast<char*>(sortedLinesInfo.data()), sortedLinesInfo.size() * sizeof(LineInfo));
//    chunkFile.close();
//    indexFile.close();

//    return {IterativeFile(chunkFilePath, dataChunks), IterativeFile(chunkIndexFilePath,indexChunks)};
//}


//void merge(vector<pair<IterativeFile, IterativeFile> > &m_chunks, const string & m_outputFile)
//{
//    //some cache variables for saving CPU time on vector indexing operations;
//    vector<char*> currDatas(m_chunks.size());
//    vector<LineInfo*> currLineInfos(m_chunks.size());
//    vector<size_t> currLineInfoSizes(m_chunks.size());
//    vector<size_t> currLineNum(m_chunks.size(), 0);

//    for(size_t i = 0; i < m_chunks.size(); i++)
//    {
//        if(!m_chunks[i].first.init())
//            throw string("can't open file: ") + m_chunks[i].first.filePath;
//        m_chunks[i].first.loadNextChunk();
//        currDatas[i] = m_chunks[i].first.data.data();

//        if(!m_chunks[i].second.init())
//            throw string("can't open file: ") + m_chunks[i].second.filePath;
//        m_chunks[i].second.loadNextChunk();
//        currLineInfos[i] = reinterpret_cast<LineInfo *>(m_chunks[i].second.data.data());
//        currLineInfoSizes[i] = m_chunks[i].second.data.size() / sizeof(LineInfo);
//    }

//    auto compartator = [&](size_t l1, size_t l2) {
//        const char* data1 = currDatas[l1] + currLineInfos[l1]->strStart - currLineInfos[l1]->start;
//        const char* data2 = currDatas[l2] + currLineInfos[l2]->strStart - currLineInfos[l2]->start;

//        int cmp = customSTRCMP(data1, data2);
//        if (cmp > 0)
//            return true;
//        else if (cmp < 0)
//            return false;
//        else
//            return currLineInfos[l1]->num > currLineInfos[l2]->num;
//    };

//    priority_queue<size_t,  vector<size_t>, decltype(compartator)> q(compartator);

//    for (size_t i = 0; i < m_chunks.size(); i++)
//        q.push(i);

//    ofstream outputStream(m_outputFile, ios::binary);
//    if(!outputStream)
//        throw string("can't open file: ") + m_outputFile;

//    while(!q.empty())
//    {

//        size_t poppedIndex = q.top();
//        q.pop();

//        //        string pstr = to_string(poppedIndex) + " " + to_string(currLineInfos[poppedIndex]->finis - currLineInfos[poppedIndex]->strStart) + " ";
//        //        outputStream.write(pstr.data(), pstr.size());

//        outputStream.write(currDatas[poppedIndex],
//                           currLineInfos[poppedIndex]->finis - currLineInfos[poppedIndex]->start);

//        currDatas.at(poppedIndex) += currLineInfos[poppedIndex]->finis - currLineInfos[poppedIndex]->start; //currLineInfos[poppedIndex]->start - m_chunks[poppedIndex].first.globalOffset;
//        currLineNum.at(poppedIndex) ++;
//        currLineInfos.at(poppedIndex) ++;

//        if(currLineNum[poppedIndex] >= currLineInfoSizes[poppedIndex])
//        {
//            bool isNext = m_chunks[poppedIndex].first.loadNextChunk() && m_chunks[poppedIndex].second.loadNextChunk();
//            currLineNum.at(poppedIndex) = 0;
//            currLineInfos.at(poppedIndex) = reinterpret_cast<LineInfo *>(m_chunks[poppedIndex].second.data.data());
//            currDatas.at(poppedIndex) = m_chunks[poppedIndex].first.data.data();
//            currLineInfoSizes.at(poppedIndex) = m_chunks[poppedIndex].second.data.size() / sizeof(LineInfo);
//            if(!isNext)
//                continue;
//        }
//        q.push(poppedIndex);
//    }

//    outputStream.close();
//}


//    // in case of little file just run index sort; TODO not create index file
//    if(chunkBounds.size() == 1)
//    {
//        cout << "file is small. use sort without chunks.." << endl;
//        IterativeFile file(inputFile, chunkBounds);
//        if (!file.init())
//            throw string("can't open file:") + inputFile;
//        file.loadNextChunk();

//        vector<LineInfo> lineData = collectLineInfo(file.data);
//        vector<size_t> idx = sortIndexes(lineData, file.data);
//        string chunkIndexFilePath = genFilePath(cacheFolder, "chunkIndex", 0);
//        saveSortedChunk(idx, lineData, file.data, outputFile, chunkIndexFilePath, fileSize);

//        int time = tracker.elapsed();
//        float bytesPerSecond = float(fileSize) / float(float(max(time, 1)) / 1000.0f);
//        cout << "FINISH. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
//        return;
//    }

//    int64_t averageChunkOfChunkSize = averageChunkSize / (chunkBounds.size());

//    IterativeFile file(inputFile, chunkBounds);
//    if (!file.init())
//        throw string("can't open file:") + inputFile;

//    vector<pair<IterativeFile, IterativeFile>> m_chunks;
//    size_t chunkIndex = 0;
//    while(file.loadNextChunk())
//    {
//        vector<LineInfo> lineData = collectLineInfo(file.data);
//        vector<size_t> idx = sortIndexes(lineData, file.data);
//        string chunkFilePath = genFilePath(cacheFolder, "chunk", chunkIndex);
//        string chunkIndexFilePath = genFilePath(cacheFolder, "chunkIndex", chunkIndex);
//        m_chunks.emplace_back(saveSortedChunk(idx, lineData, file.data, chunkFilePath, chunkIndexFilePath, averageChunkOfChunkSize));
//        chunkIndex++;
//    }
//    file.close();

//    cout << "create chunks time:" << tracker.elapsed() << endl;

//    //merge(m_chunks, outputFile);

//    for(auto &filePair : m_chunks)
//    {
//        remove(filePair.first.filePath.c_str());
//        remove(filePair.second.filePath.c_str());
//    }

//    int time = tracker.elapsed();
//    float bytesPerSecond = float(fileSize) / float(float(max(time, 1)) / 1000.0f);
//    cout << "FINISH. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
}
