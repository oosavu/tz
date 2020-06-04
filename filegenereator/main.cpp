#include <iostream>
#include <string>
#include <random>
#include <array>
#include <fstream>
#include <stdio.h>
#include <chrono>
#include "../cmdopts.h"
using namespace std;

struct Options
{
    int64_t filesize{50000};
    std::string filepath{"asd.txt"};
    int stringsCount{2};
    int numsCount{100000};
    int maxStrLen{50};
    int maxNum{200000};
    bool fullRandom{false};
};

class GeneratorEngine
{
public:
    GeneratorEngine(const Options &opts) :
        m_generator(time(0)),
        m_numDistributor(0, opts.maxNum),
        m_strLenDistributor(1, opts.maxStrLen),
        m_charDistributor(0, posibleChars.size() - 1)
    {
    }

    inline std::string genNum()
    {
        return std::to_string(m_numDistributor(m_generator)) + ". ";
    }

    inline std::string genString()
    {
        std::string str;
        size_t len = m_strLenDistributor(m_generator);
        str.resize(len + 1);
        for(size_t i = 0; i < len; i++)
            str[i] = posibleChars[m_charDistributor(m_generator)];
        str[len] = '\n';
        return str;
    }
private:
    std::array<char, 26> posibleChars ={'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
    std::mt19937 m_generator;
    std::uniform_int_distribution<> m_numDistributor;
    std::uniform_int_distribution<> m_strLenDistributor;
    std::uniform_int_distribution<> m_charDistributor;
};

int main(int argc, const char* argv[])
{
    auto parser = CmdOpts<Options>::Create({{"--filesize", &Options::filesize },
                                            {"--filepath", &Options::filepath },
                                            {"--stringsCount", &Options::stringsCount},
                                            {"--numsCount", &Options::numsCount},
                                            {"--maxStrLen", &Options::maxStrLen},
                                            {"--maxNum", &Options::maxNum},
                                            {"--fullRandom", &Options::fullRandom}});

    auto opts = parser->parse(argc, argv);

    std::ofstream file(opts.filepath, ios::binary);
    if(!file)
    {
        cerr << "ERROR: can't open file:" << opts.filepath;
        return -1;
    }
    GeneratorEngine engine(opts);

    std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();

    //slow variant, no big memory cache usage
    if (opts.fullRandom)
    {
        int64_t count = 0;
        while (count < opts.filesize)
        {
            std::string num = engine.genNum();
            file.write(num.data(), num.size());
            std::string str = engine.genString();
            file.write(str.data(), str.size());
            count += num.size() + str.size();

        }
    }
    //fast variant. generate cache then combine it;
    else
    {
        std::vector<std::string> numsCache(opts.numsCount);
        for(int i = 0; i < opts.numsCount; i++)
            numsCache[i] = engine.genNum();
        std::vector<std::string> stringsCache(opts.stringsCount);
        for(int i = 0; i < opts.stringsCount; i++)
            stringsCache[i] = engine.genString();

        //we need separate random generator for cache indexes
        std::mt19937 indexGenerator(time(0));
        std::uniform_int_distribution<> numIndexDistributor(0, opts.numsCount - 1);
        std::uniform_int_distribution<> stringIndexDistributor(0, opts.stringsCount - 1);

        int64_t count = 0; // file.getp wery slow!
        while (count < opts.filesize)
        {
            int numIndex = numIndexDistributor(indexGenerator);
            file.write(numsCache[numIndex].data(), numsCache[numIndex].size());
            int stringIndex = stringIndexDistributor(indexGenerator);
            file.write(stringsCache[stringIndex].data(), stringsCache[stringIndex].size());
            count += numsCache[numIndex].size() + stringsCache[stringIndex].size();
        }
    }

    file.close();
    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    int time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
    float bytesPerSecond = float(opts.filesize) / (float(time) / 1000.0);
    cout << "FINISH. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
    return 0;
}

