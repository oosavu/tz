#include <iostream>
#include <string>
#include <random>
#include <array>
#include <fstream>
#include "cmdopts.h"
#include <stdio.h>
#include <chrono>

using namespace std;

struct Options
{
    int filesize{1000000000};
    std::string filepath{"/home/oosavu/asd.txt"};
    int stringRepeatSeries{1};
    int stringRepeatSeriesLength{10};
    int maxStrLen{100};
    int maxNum{1000};
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
        //return "sssssssssssssdfsdfffffffffffffffff";
        std::string str;
        size_t len = m_strLenDistributor(m_generator);
        str.resize(len + 1);
        for(size_t i = 0; i < len; i++)
            str[i] = posibleChars[m_charDistributor(m_generator)];
        str[len] = '\n';
        return str;
    }

//    void writeRandom(ofstream &file)
//    {
//        int num = genNum();
//        std::string str = genString();
//        //file << ". " << str;
//        file << std::to_string(num) << ". " << str;
//    }
private:
    std::array<char, 27> posibleChars ={'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',' '};
    std::mt19937 m_generator;
    std::uniform_int_distribution<> m_numDistributor;
    std::uniform_int_distribution<> m_strLenDistributor;
    std::uniform_int_distribution<> m_charDistributor;
};

int main(int argc, const char* argv[])
{
    auto parser = CmdOpts<Options>::Create({{"--filesize", &Options::filesize },
                                            {"--filepath", &Options::filepath },
                                            {"--stringRepeatSeries", &Options::stringRepeatSeries},
                                            {"--stringRepeatSeriesLength", &Options::stringRepeatSeriesLength},
                                            {"--maxStrLen", &Options::maxStrLen},
                                            {"--maxNum", &Options::maxNum}});

    auto opts = parser->parse(argc, argv);

    //GeneratorEngine engine(opts);
    //std::ofstream file(opts.filepath, ios::binary);
    //std::vector<char> vec(23);
    //file.rdbuf()->pubsetbuf(&vec.front(), vec.size());

    //    if(!file)
    //    {
    //        cerr << "ERROR: can't open file:" << opts.filepath;
    //        return 0;
    //    }

    int qwe = 100;
    {
        FILE* pFile;
        pFile = fopen("file.binary", "wb");
        setvbuf ( pFile , NULL , _IOFBF , 1024 );
        std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
        string repeat;
        int qwe = 10;
        repeat.resize(qwe, 'a');
        int coun = 0;
        while(coun < opts.filesize)
        {
            coun += qwe;
            fwrite(repeat.data(), sizeof(char), repeat.size() , pFile);
        }
        fflush(pFile);
        fclose (pFile);
        std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
        int time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        float bytesPerSecond = float(opts.filesize) / (float(time) / 1000.0);
        cout << "FINISH C. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
    }

    {
        std::ofstream file(opts.filepath, ios::binary);
        std::vector<char> vec(23);
        std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
        string repeat;

        repeat.resize(qwe, 'a');
        int coun = 0;
        while(coun < opts.filesize)
        {
            coun += qwe;
            file.write(repeat.data(), repeat.size());
            //fwrite(repeat.data(), sizeof(char), repeat.size() , pFile);
        }
        file.close();
        std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
        int time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        float bytesPerSecond = float(opts.filesize) / (float(time) / 1000.0);
        cout << "FINISH. time:" << time << " msec. speed: " << int(bytesPerSecond)  << " bytes/sec" << endl;
    }

    //just put repeats at the begining of file (TODO ability to distribute it)
    //    for(int i = 0; i < opts.stringRepeatSeries && file.tellp() < opts.filesize; i++)
    //    {
    //        string repeat = engine.genString();
    //        for(int j = 0; j < opts.stringRepeatSeriesLength && file.tellp() < opts.filesize; j++)
    //        {
    //            int num = engine.genNum();
    //            file << std::to_string(num) << ". " << repeat;
    //        }
    //    }

    //string repeat = engine.genString();



    //while(file.tellp() < opts.filesize)
    //file << repeat;
    //    while(coun < opts.filesize)
    //    {
    //        coun += qwe;
    //        file.write(repeat.data(), repeat.size());
    //    }
    //        //engine.writeRandom(file);
    //    file.close();




    return 0;
}

