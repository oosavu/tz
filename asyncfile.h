#ifndef ASYNCFILE_H
#define ASYNCFILE_H

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <utility>

class ElementaryFileOperations
{
    inline static std::mutex m_fileSystemGlobalMutex;
public:
    static bool read(std::ifstream &stream, int64_t startByte, int64_t endByte, std::vector<char> &data);

    static bool write(std::ofstream &stream, std::vector<char> &data);
};

class AsyncOStreamBuf : public std::streambuf
{
public:
    AsyncOStreamBuf(const std::string & name, size_t chunkSize);
    ~AsyncOStreamBuf();
    bool isValid();

private:
    size_t m_maxChunkSize;
    bool m_isValid;
    std::ofstream m_file;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::vector<char> m_currChunk;
    bool m_done;
    std::thread m_thread;
    std::string m_name;
    std::vector<char> m_chunk;

    void worker();
    virtual std::streamsize xsputn(const char* s, std::streamsize n);
    void swapChunks();
    virtual int sync();
};

class AsyncOstream : public std::ostream
{
    AsyncOStreamBuf m_streamBuf;
public:
    AsyncOstream(const std::string & name, size_t chunkSize = 100000000); //100mb cache
    bool isValid();
};


#endif // ASYNCFILE_H
