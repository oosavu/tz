#include "asyncfile.h"

AsyncOStreamBuf::AsyncOStreamBuf(const std::string &name, size_t chunkSize) :
    m_maxChunkSize(chunkSize),
    m_isValid(false),
    m_done(false),
    m_name(name)
{
    m_chunk.reserve(m_maxChunkSize);
    m_currChunk.reserve(m_maxChunkSize);
    m_file.open(m_name, std::ios::binary);
    if(!m_file)
        return;

    m_isValid = true;
    m_thread = std::thread(&AsyncOStreamBuf::worker, this);
}

AsyncOStreamBuf::~AsyncOStreamBuf()
{
    swapChunks();
    {
        std::lock_guard<std::mutex>(this->m_mutex);
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
    while (!m_done)
    {
        std::unique_lock<std::mutex> guard(this->m_mutex);
        this->m_condition.wait(guard,[this](){
            return !this->m_currChunk.empty() || this->m_done;
        });
        ElementaryFileOperations::write(m_file, m_currChunk);
        m_currChunk.clear();
        m_condition.notify_one();
    }
    ElementaryFileOperations::write(m_file, m_currChunk);
    m_currChunk.clear();
    m_file.close();
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
    {
        std::unique_lock<std::mutex> guard(this->m_mutex);
        m_condition.wait(guard,[this](){
            return this->m_currChunk.empty();
        });
        std::swap(m_currChunk, m_chunk);
    }
    m_condition.notify_one();
}

int AsyncOStreamBuf::sync() {
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

bool ElementaryFileOperations::read(std::ifstream &stream, int64_t startByte, int64_t endByte, std::vector<char> &data)
{
    std::lock_guard<std::mutex> locker(m_fileSystemGlobalMutex);
    int64_t size = endByte - startByte;
    data.resize(size);
    stream.seekg(startByte);
    stream.read(data.data(), size);
    return bool(stream);
}

bool ElementaryFileOperations::write(std::ofstream &stream, std::vector<char> &data)
{
    std::lock_guard<std::mutex> locker(m_fileSystemGlobalMutex);
    //data.resize(endByte - startByte);
    stream.write(data.data(), data.size());
    stream.flush();
    return bool(stream);
    //fwrite(data.data(), sizeof(char), data.size(), file) == data.size();
}
