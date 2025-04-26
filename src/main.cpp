#include "RequestGenerator.h"
#include "SkipListV1.h"

#include "argparse/argparse.hpp"

#include <chrono>
#include <cstdint>
#include <thread>

using namespace dm;

uint64_t total_entries = 2'000'000;
uint64_t total_queries = 100'000;

int main(int argc, char *argv[])
{
    argparse::ArgumentParser program("dmtb2025q1", "1.0", argparse::default_arguments::none);

    program.add_argument("--parallel")
        .help("insert parallelism")
        .scan<'i', uint32_t>()
        .default_value(16u);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    uint32_t parallel = program.get<uint32_t>("--parallel");

    // insert
    uint32_t entries_per_thread = total_entries / parallel;
    uint32_t remainder = total_entries % parallel;

    SkipListV1<Node<10/*MaxLevel*/>, 50/*NextLevelP*/> skiplist(parallel, sizeof(Node<10>) * entries_per_thread + 1);
    std::vector<RequestGenerator *> req_gens;
    std::vector<const char *> keys(total_entries);

    for (uint32_t i = 0; i < parallel; i++)
    {
        uint32_t n_entries = entries_per_thread + i < remainder ? 1 : 0;
        req_gens.emplace_back(new RequestGenerator(n_entries, i));
    }

    std::vector<std::thread> threads;

    auto insert_start = std::chrono::steady_clock::now();

    uint32_t entries_offset = 0;
    for (uint32_t i = 0; i < parallel; i++)
    {
        uint32_t n_entries = entries_per_thread + i < remainder ? 1 : 0;
        threads.emplace_back([&, i, entries_offset]()
        {
            req_gens[i]->generateRequest();
            skiplist.insert(req_gens[i]->mKey, req_gens[i]->mValue, i);
            keys[n_entries + i] = req_gens[i]->mKey.data();
        });
        entries_offset += n_entries;
    }

    for (uint32_t i = 0; i < parallel; i++)
    {
        threads[i].join();
    }

    auto insert_end = std::chrono::steady_clock::now();
    std::cout << "insert " << total_entries << " entries with " << parallel << " threads cost " << (insert_end - insert_start).count() / 1000000. << "ms.\n";

    // query
    threads.clear();
    RNG random;

    auto query_start = std::chrono::steady_clock::now();

    for (uint32_t i = 0; i < total_queries; i++)
    {
        uint32_t idx = random.rand() % total_entries;
        uint64_t value = skiplist.find(keys[idx]);
    }

    auto query_end = std::chrono::steady_clock::now();
    std::cout << "query " << total_queries << " keys cost " << (query_end - query_start).count() / 1000000. << "ms.\n";

    return 0;
}

