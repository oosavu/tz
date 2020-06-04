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

struct AsyncFileWriter
{
    std::ofstream                 m_file;
    std::mutex                    m_mutex;
    std::condition_variable       m_condition;
    std::queue<std::vector<char>> m_chunksQueue;
    std::vector<char>             m_currChunk;
    bool                          m_done;
    std::thread                   m_thread;

    void worker() {
        std::cout << "worker start";
        bool local_done(false);
        std::vector<char> buf;
        while (!local_done) {
            {
                std::unique_lock<std::mutex> guard(this->m_mutex);
                std::cout << "worker wait...";
                this->m_condition.wait(guard,[this](){
                    return !this->m_chunksQueue.empty() || this->m_done;
                });
                if (!this->m_chunksQueue.empty())
                {
                    m_currChunk.swap(m_chunksQueue.front());
                    m_chunksQueue.pop();
                }
                local_done = this->m_chunksQueue.empty() && this->m_done;
            }
            if (!buf.empty())
            {
                std::cout << "worker write...";
                m_file.write(buf.data(), std::streamsize(buf.size()));
                m_file.flush();
                buf.clear();
            }
        }
        m_file.flush();
        std::cout << "worker finis...";
    }

public:
    AsyncFileWriter(const std::string & name)
        : m_file(name)
        , m_done(false)
        , m_thread(&AsyncFileWriter::worker, this)
    {

    }
    ~AsyncFileWriter()
    {
        {
            std::unique_lock<std::mutex>(this->m_mutex);
            this->m_done = true;
        }
        this->m_condition.notify_one();
        this->m_thread.join();
    }
    void append(const std::vector<char> && chunk)
    {
        {
            std::unique_lock<std::mutex>(this->m_mutex);
            m_chunksQueue.emplace(std::move(chunk));
        }
        this->m_condition.notify_one();
    }
};

//int main()
//{
//    async_buf    sbuf("async.out");
//    std::ostream astream(&sbuf);
//    std::ifstream in("async_stream.cpp");
//    for (std::string line; std::getline(in, line); ) {
//        astream << line << '\n' << std::flush;
//    }
//}

#endif // ASYNCFILE_H
