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

struct AsyncFileWriter
{
    //std::ofstream                 m_file;
    FILE*    m_file;
    std::mutex                    m_mutex;
    std::condition_variable       m_condition;
    std::vector<char>             m_currChunk;
    bool                          m_done;
    std::thread                   m_thread;
    std::string m_name;

    void worker() {
        std::cout << "worker start" << std::endl;
        m_file = fopen(m_name.c_str(),"wb");
        while (!m_done)
        {
            std::unique_lock<std::mutex> guard(this->m_mutex);
            std::cout << "worker wait..." << std::endl;
            this->m_condition.wait(guard,[this](){
                return !this->m_currChunk.empty() || this->m_done;
            });
            fwrite(m_currChunk.data(), sizeof(char),m_currChunk.size(), m_file);
            m_currChunk.clear();
            fflush(m_file);
            m_condition.notify_one();
        }
        fwrite(m_currChunk.data(), sizeof(char),m_currChunk.size(), m_file);
        m_currChunk.clear();
        fflush(m_file);
        fclose(m_file);
        std::cout << "worker finis..." << std::endl;
    }


public:
    AsyncFileWriter(const std::string & name) :
         m_done(false) ,
         m_thread(&AsyncFileWriter::worker, this),
         m_name(name)
    {
    }

    ~AsyncFileWriter()
    {
        {
            std::unique_lock<std::mutex>(this->m_mutex);
            this->m_done = true;
        }
        m_condition.notify_one();
        m_thread.join();
    }
    void appendChunk(std::vector<char> && chunk)
    {
        {
            std::unique_lock<std::mutex> guard(this->m_mutex);
            std::cout << "append wait..." << std::endl;
            m_condition.wait(guard,[this](){
                return this->m_currChunk.empty();
            });
            m_currChunk = std::move(chunk);
        }
        m_condition.notify_one();
    }
};

class AsyncStreamBuf : public std::streambuf
{
    std::vector<char> chunk;
    AsyncFileWriter m_writer;

    virtual  std::streamsize xsputn(const char* s, std::streamsize n)
    {
        std::cout << "!!!" << std::endl;
        chunk.resize(n);
        memcpy(chunk.data(), s, n);
        m_writer.appendChunk(std::move(chunk));
        return n;
    }
public:
    AsyncStreamBuf(const std::string & name) :
        m_writer(name),
        std::streambuf()
    {
        //setp(chunk.data(), chunk.data() + chunk.size());
    }

    virtual  int overflow(int c) {
        std::cout << "over" << std::endl;
        return c;

        if (c != std::char_traits<char>::eof()) {
            *this->pptr() = std::char_traits<char>::to_char_type(c);
            this->pbump(1);
        }
        return this->sync() != -1
            ? std::char_traits<char>::not_eof(c): std::char_traits<char>::eof();
    }
    virtual  int sync() {
        std::cout << "sync" << std::endl;
        return 0;
    }
};


//class StringBuffer: public std::stringstream
//{

//public:
//    StringBuffer() : std::stringstream() {}

//    long length()
//    {
//        long length = tellp();

//        if(length < 0)
//            length = 0;
//        return length;
//    }
//};


#endif // ASYNCFILE_H
