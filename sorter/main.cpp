#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <random>
#include <cmath>
#include <array>
#include <fstream>
#include <tuple>
#include <algorithm>
#include <string_view>
#include <cstring>
#include "../cmdopts.h"

// it's bad practice to use namespace std, however for test job it's ok :)
using namespace std;


struct Options
{
    uint64_t chunkSize{100000};
    std::string inputFile{"/home/oosavu/asd.txt"};
    std::string outputFile{"/home/oosavu/qwe.txt"};
    std::string cacheDir{"/home/oosavu"};
};

vector<size_t> findChunkBounds(ifstream &file, size_t averageChunkSize)
{
    file.seekg(0, file.end);
    size_t fileSize = file.tellg();
    cout << "input file size:" << fileSize << endl;

    vector<size_t> res;
    res.push_back(0);

    size_t averageGlobalPos = 0;
    while(true)
    {
        averageGlobalPos += averageChunkSize;
        if(averageGlobalPos >= fileSize)
        {
            res.push_back(fileSize);
            break;
        }
        file.seekg(averageGlobalPos, file.beg);
        string line;
        getline(file, line); // move to the first '\n' charecter

        res.push_back(file.tellg());

        if(file.tellg() == fileSize) // it was the final line, so just continue
        {
            break;
        }

    }

    cout << "chunkBounds:";
    for (size_t i : res)
        cout << i << " ";
    cout << endl;

    return res;

}

struct LineInfo{
    size_t num;
    size_t start;
    size_t strStart;
    size_t finis;
};

vector<size_t> sortIndexes(const vector<LineInfo> &lineData, const vector<char> &data)
{
    vector<size_t> idx(lineData.size());
    iota(idx.begin(), idx.end(), 0);
    const char* rawData = data.data();
    sort(idx.begin(), idx.end(),
         [&](size_t l1, size_t l2) {
        int cmp = strcmp(&rawData[lineData[l1].strStart], &rawData[lineData[l2].strStart]);
        if (cmp < 0)
            return true;
        else if (cmp > 0)
            return false;
        else
            return lineData[l1].num > lineData[l2].num;
    });

    return idx;
}

vector<LineInfo> collectChunkInfo(vector<char> &data)
{
    vector<LineInfo> lines;
    auto i = data.begin(), end = data.end();
    //end--; //drop the last '\n'
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
        //cout << rawData[lineInfo.strStart] << endl;
        lines.emplace_back(lineInfo);
    }
    return lines;
}

void saveSortedChunk(const vector<size_t> &idx, const vector<LineInfo> lineInfo, char* rawData, const string &filePath)
{
    ofstream file(filePath, ios::binary);
    for(size_t i : idx)
    {
        file.write(&rawData[lineInfo[i].start], lineInfo[i].finis - lineInfo[i].start);
    }
    file.close();
}

std::string genFilePath(const std::string &folder, const std::string &name, int index)
{
    return folder + "/" + name + to_string(index); //todo win support.
}

void createSortedChunks(const Options &opts)
{
    ifstream file(opts.inputFile, ios::binary);
    if(!file)
        throw string("can't open file:") + opts.inputFile;

    vector<size_t> chunkBounds = findChunkBounds(file, opts.chunkSize);


    file.seekg (0, file.beg);
    for(size_t chunkIndex = 0; chunkIndex < chunkBounds.size() - 1; chunkIndex++)
    {
        size_t chunkSize = chunkBounds[chunkIndex + 1] - chunkBounds[chunkIndex];
        vector<char> data(chunkSize);
        file.read(data.data(), data.size());
        vector<LineInfo> lineData = collectChunkInfo(data);
        vector<size_t> idx = sortIndexes(lineData, data);
        cout <<  lineData[0].num << data[lineData[0].strStart]<< endl;
        //for(size_t i : idx)
        //    cout << i  <<  ": "<< lineData[i].num << data[lineData[i].strStart]<< endl;
        saveSortedChunk(idx, lineData, data.data(), genFilePath(opts.cacheDir, "chunk", chunkIndex));
    }
}


int main(int argc, const char* argv[])
{
    auto parser = CmdOpts<Options>::Create({{"--chunkSize", &Options::chunkSize },
                                            {"--inputFile", &Options::inputFile },
                                            {"--outputFile", &Options::outputFile},
                                            {"--cacheDir", &Options::cacheDir}});
    auto opts = parser->parse(argc, argv);

    try {
        createSortedChunks(opts);
    } catch (string s) {
        cerr << "ERROR:" << s << endl;;
        return -1;
    }
    return 0;
}
