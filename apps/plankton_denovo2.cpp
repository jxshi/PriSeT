#include <iostream>

#include "../src/argument_parser.hpp"
#include "../src/solver.hpp"

// g++ ../PriSeT/apps/barcode_miner_bf2.cpp -Wno-write-strings -std=c++17 -Wall -Wextra -lstdc++fs -Wno-unknown-pragmas -lstdc++fs -DNDEBUG -O3 -I~/include -L~/lib -ldivsufsort -o barcode_miner_bf2
// barcode_miner_bf2 -l <dir_library> -w <dir_work> [-m <max_num_primers>] [-s]

using namespace priset;
using namespace std;

int main(int argc, char ** argv)
{
    // set path prefixes for library files
    io_cfg_type io_cfg{};

    // Default configuration for primer settings.
    primer_cfg_type primer_cfg{};

    // parse options and init io and primer configurators
    options opt(argc, argv, primer_cfg, io_cfg);

    // primer_cfg.set_dTm(4);
    primer_cfg.set_dTm(2);

    // init solver
    solver_brute_force solver{io_cfg, primer_cfg};

    // solve
    solver.solve();

    // sort
    solver.sort_results_by_frequency();

    // output solutions
    cout << solver.get_header() << endl;
    std::vector<TResult> results;
    auto [has_next, results] solver.get_next_result():
    size_t ctr{0};
    while (has_next)
    {
        cout << "Result #" << ++ctr << ":\n";
        for (TResult result : results)
            cout << "\t" << result.to_string() << endl;
        auto [has_next, results] = solver.get_next_result();
    }
    return 0;
}


// lib_dir=<lib_dir>
// taxid=<taxid>
// work_dir=<work_dir>
// g++ ../PriSeT/apps/plankton_denovo.cpp -Wno-write-strings -std=c++17 -Wall -Wextra -lstdc++fs -Wno-unknown-pragmas -lstdc++fs -DNDEBUG -O3 -I~/include -L~/lib -ldivsufsort -o denovo
// ./denovo $lib_dir $work_dir


int main(int argc, char ** argv)
{
    if (argc != 4)
    {
        std::cout << "Give taxid, and paths to lib and work dirs.\n";
        exit(-1);
    }
    unsigned const priset_argc = 6;
    char * const priset_argv[priset_argc] = {"priset", "-l", argv[2], "-w", argv[3], "-s"};
    for (unsigned i = 0; i < priset_argc; ++i) std::cout << priset_argv[i] << " ";
    std::cout << std::endl;

    // timing
    std::chrono::time_point<std::chrono::system_clock> start, finish;
    std::array<size_t, TIMEIT::SIZE> runtimes;

    // collect number of kmers or kmer pairs left after relevant processing steps
    TKmerCounts kmerCounts{0, 0, 0, 0};

    // set path prefixes for library files
    io_cfg_type io_cfg{};

    // get instance to primer sequence settings
    primer_cfg_type primer_cfg{};

    // parse options and init io and primer configurators
    options opt(priset_argc, priset_argv, primer_cfg, io_cfg);

    TKLocations locations;
    TDirectoryInformation directoryInformation;
    TSequenceNames sequenceNames;
    TSequenceLengths sequenceLengths;

    // compute k-mer mappings
    start = std::chrono::high_resolution_clock::now();
    fm_map(io_cfg, primer_cfg, locations);
    finish = std::chrono::high_resolution_clock::now();
    runtimes.at(TIMEIT::MAP) += std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

    std::cout << "INFO: kmers init = " << std::accumulate(locations.begin(), locations.end(), 0, [](unsigned ctr, auto & location){return ctr + location.second.first.size();}) << std::endl;

    TReferences references;
    TKmerIDs kmerIDs;
    TSeqNoMap seqNoMap;
    start = std::chrono::high_resolution_clock::now();
    transform_and_filter(io_cfg, locations, references, seqNoMap, kmerIDs, &kmerCounts);
    finish = std::chrono::high_resolution_clock::now();
    runtimes.at(TIMEIT::FILTER1_TRANSFORM) += std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
    std::cout << "INFO: kmers after filter1 & transform = " << get_num_kmers(kmerIDs) << std::endl;

    // TODO: delete locations
    using TPairList = TPairList<TPair<TCombinePattern<TKmerID, TKmerLength>>>;
    TPairList pairs;
    // dictionary collecting (unique) pair frequencies

    start = std::chrono::high_resolution_clock::now();
    // template<typename TPairList>
    // void combine(TReferences const & references, TKmerIDs const & kmerIDs, TPairList & pairs, TKmerCounts * kmerCounts = nullptr)
    combine<TPairList>(references, kmerIDs, pairs, &kmerCounts);
    finish = std::chrono::high_resolution_clock::now();
    runtimes.at(TIMEIT::COMBINE_FILTER2) += std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

    std::cout << "INFO: pairs after combiner = " << kmerCounts[KMER_COUNTS::COMBINER_CNT] << std::endl;

    //  <freq, <kmer_fwd, mask_fwd, kmer_rev, mask_rev> >
    // using TPairFreq = std::pair<uint32_t, std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>>;
    std::vector<TPairFreq> pair_freqs;
    start = std::chrono::high_resolution_clock::now();
    // template<typename TPairList, typename TPairFreqList>
    // void filter_pairs(TReferences & references, TKmerIDs const & kmerIDs, TPairList & pairs, TPairFreqList & pair_freqs, TKmerCounts * kmerCounts = nullptr)
    filter_pairs<TPairList, TPairFreqList>(references, kmerIDs, pairs, pair_freqs, &kmerCounts);
    std::cout << "INFO: pairs after pair_freq filter = " << kmerCounts[KMER_COUNTS::FILTER2_CNT] << std::endl;
    finish = std::chrono::high_resolution_clock::now();
    runtimes.at(TIMEIT::PAIR_FREQ) += std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

    std::cout << "K\tMAP\t\tFILTER1_TRANSFORM\tCOMBINE_FILTER2\tPAIR_FREQ\t|\tSUM [μs]\n" << std::string(100, '_') << "\n";
    std::cout << "[" << 16 << ":" << 25 << "]\t" << runtimes[priset::TIMEIT::MAP] << "\t" <<
            '\t' << runtimes[priset::TIMEIT::FILTER1_TRANSFORM] <<
            '\t' << runtimes[priset::TIMEIT::COMBINE_FILTER2] << '\t' << runtimes[priset::TIMEIT::PAIR_FREQ] <<
            "\t|\t" << std::accumulate(std::cbegin(runtimes), std::cend(runtimes), 0) << '\n';

    // Sort pairs by frequency of
    std::sort(pair_freqs.begin(), pair_freqs.end(), [&](TPairFreq & p1, TPairFreq & p2){return p1.first > p2.first;});
    std::stringstream sstream;
    std::cout << "ID\tFrequency\tForward\tReverse\tTm\tCG\n";

    for (size_t k = 0; k < std::min(pair_freqs.size(), size_t(15)); ++k)
    {
        TPairFreq pf = pair_freqs.at(k);
        const auto & [code_fwd, mask_fwd, code_rev, mask_rev] = pf.second;
        std::string fwd = dna_decoder(code_fwd, mask_fwd);
        std::string rev = dna_decoder(code_rev, mask_rev);
        sstream << std::hex << std::hash<std::string>()(fwd + rev) << ",";
        sstream << pf.first << "," << fwd << "," << rev << ",";
        sstream << "\"[" << float(Tm(code_fwd, mask_fwd)) << "," << float(Tm(code_rev, mask_rev)) << "]\",";
        sstream << "\"[" << CG(code_fwd, mask_fwd) << "," << CG(code_rev, mask_rev) << "]\"" << std::endl;

    }
    // std::cout << sstream.str().substr(0,8) << "," << pf.first << "," << fwd << ",";
    // std::cout << rev << ",\"[" << float(Tm(code_fwd, mask_fwd)) << ",";
    // std::cout << float(Tm(code_rev, mask_rev)) << "]\",\"[" << CG(code_fwd, mask_fwd) << "," << CG(code_rev, mask_rev) << "]\"" << std::endl;

    std::cout << sstream.rdbuf();

    // get timestamp and output primers in csv format #primerID,fwd,rev
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    fs::path primer_file = io_cfg.get_work_dir() / ("primers_" + std::string(timestamp, 10));
    std::ofstream ofs;
    ofs.open(primer_file);
    ofs << "#PrimerID, fwd (3' to 5'), rev (3' to 5')\n";
    ofs << sstream.rdbuf();
    ofs.close();
    std::cout << "primer sequences written to " << primer_file.string() << std::endl;

    return 0;
}