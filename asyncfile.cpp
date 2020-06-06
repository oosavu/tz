#include "asyncfile.h"

AsyncOStreamBuf::AsyncOStreamBuf(const std::string &name, size_t chunkSize) :
    m_maxChunkSize(chunkSize),
    m_isValid(false),
    m_done(false),
    m_name(name)
{
    m_chunk.reserve(m_maxChunkSize);
    m_currChunk.reserve(m_maxChunkSize);
    m_file = fopen(m_name.c_str(),"wb");
    if(!m_file)
    {
        std::cout << "can't open file " << name << std::endl;
        return;
    }
    m_isValid = true;

    m_thread = std::thread(&AsyncOStreamBuf::worker, this);
}

AsyncOStreamBuf::~AsyncOStreamBuf()
{
    swapChunks();
    {
        std::unique_lock<std::mutex>(this->m_mutex);
        this->m_done = true;
    }
    m_condition.notify_one();
    m_thread.join();
}

bool AsyncOStreamBuf::isValid()
{
    return m_isValid;
}

void AsyncOStreamBuf::worker() {
    //std::cout << "worker start" << std::endl;
    while (!m_done)
    {
        std::unique_lock<std::mutex> guard(this->m_mutex);
        this->m_condition.wait(guard,[this](){
            return !this->m_currChunk.empty() || this->m_done;
        });
        //  std::cout << "worker wakeup" << std::endl;
        fwrite(m_currChunk.data(), sizeof(char),m_currChunk.size(), m_file);
        m_currChunk.clear();
        fflush(m_file);
        m_condition.notify_one();
    }
    fwrite(m_currChunk.data(), sizeof(char),m_currChunk.size(), m_file);
    fflush(m_file);
    fclose(m_file);
    //std::cout << "worker finis" << std::endl;
}

std::streamsize AsyncOStreamBuf::xsputn(const char *s, std::streamsize n)
{
    if(m_chunk.size() + n > m_maxChunkSize)
    {
        //it's time to wakeup worker and flush the m_chunk
        swapChunks();
    }
    m_chunk.resize(m_chunk.size() + n);
    memcpy(m_chunk.data() + m_chunk.size() - n, s, n);
    return n;
}

void AsyncOStreamBuf::swapChunks()
{
    //std::cout << "start swap" << std::endl;
    {
        std::unique_lock<std::mutex> guard(this->m_mutex);
        //std::cout << "append wait..." << std::endl;
        m_condition.wait(guard,[this](){
            return this->m_currChunk.empty();
        });
        std::swap(m_currChunk, m_chunk);
    }
    m_condition.notify_one();
}

int AsyncOStreamBuf::sync() {
    //        std::cout << "sync" << std::endl;
    swapChunks();
    return 0;
}

AsyncOstream::AsyncOstream(const std::string &name, size_t chunkSize) :
    m_streamBuf(name, chunkSize),
    std::ostream(&m_streamBuf)
{
}

bool AsyncOstream::isValid()
{
    return m_streamBuf.isValid();
}
