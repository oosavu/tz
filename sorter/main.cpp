#include <iostream>
#include "../cmdopts.h"
#include "sorter.h"

struct Options
{
    int64_t chunkSize{100000};
    std::string inputFile{"/home/oosavu/asd.txt"};
    std::string outputFile{"/home/oosavu/qwe.txt"};
    std::string cacheDir{"/home/oosavu"};
};

int main(int argc, const char* argv[])
{
    auto parser = CmdOpts<Options>::Create({{"--chunkSize", &Options::chunkSize },
                                            {"--inputFile", &Options::inputFile },
                                            {"--outputFile", &Options::outputFile},
                                            {"--cacheDir", &Options::cacheDir}});
    auto opts = parser->parse(argc, argv);

    try {
        Sorter sorter(opts.cacheDir, opts.inputFile, opts.outputFile, opts.chunkSize);
        sorter.process();
    } catch (std::string s) {
        std::cerr << "ERROR:" << s << std::endl;
        return -1;
    }
    return 0;
}
