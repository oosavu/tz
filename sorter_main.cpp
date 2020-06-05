#include <iostream>
#include "cmdopts.h"
#include "sorter.h"
#include <future>
struct Options
{
    int64_t chunkSize{1073741824};
    std::string inputFile{"in.txt"};
    std::string outputFile{"out.txt"};
    std::string cacheDir{""};
};

int main(int argc, const char* argv[])
{
    auto parser = CmdOpts<Options>::Create({{"--chunkSize", &Options::chunkSize },
                                            {"--inputFile", &Options::inputFile },
                                            {"--outputFile", &Options::outputFile},
                                            {"--cacheDir", &Options::cacheDir}});
    auto opts = parser->parse(argc, argv);

    try {
        sorter::sortBigFile(opts.cacheDir, opts.inputFile, opts.outputFile, opts.chunkSize);
    } catch (std::string s) {
        std::cerr << "ERROR:" << s << std::endl;
        return -1;
    }
    return 0;
}
