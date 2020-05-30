#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <random>
#include <cmath>
#include <array>
#include <fstream>

using namespace std;

class GeneratorEngine
{
public:
    GeneratorEngine(int maxNum = 1000, int minStrLen = 1, int maxStrLen = 10) :
        m_maxStrLen(maxStrLen),
        m_numDistributor(0, maxNum),
        m_strLenDistributor(minStrLen, maxStrLen),
        m_charDistributor(0, posibleChars.size() - 1)
    {
    }

    void write(ofstream &file)
    {
        int num = m_numDistributor(m_generator);
        char str[m_maxStrLen + 1];
        size_t len = m_strLenDistributor(m_generator);
        for(size_t i = 0; i < len; i++)
            str[i] = posibleChars[m_charDistributor(m_generator)];
        str[len] = 0x00;
        file << std::to_string(num) << ": " << str << endl;
    }
private:
    std::vector<char> stringBuf;
    size_t m_maxStrLen;
    std::array<char, 6> posibleChars = {'a', 'b', 'c', 'd', 'e', 'f'};
    std::mt19937 m_generator;
    std::uniform_int_distribution<> m_numDistributor;
    std::uniform_int_distribution<> m_strLenDistributor;
    std::uniform_int_distribution<> m_charDistributor;
};

int main()
{
    GeneratorEngine engine;
    std::ofstream file("/home/oosavu/example.bin", ios::binary);
    for(int i = 0; i < 100; i++)
        engine.write(file);
    file.close();
    return 0;
}
