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
        .default_value(1u);

    program.add_argument("--query_parallel")
        .help("search parallelism")
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
    uint32_t query_parallel = program.get<uint32_t>("--query_parallel");

    // insert
    uint32_t entries_per_thread = total_entries / parallel;
    uint32_t remainder = total_entries % parallel;

    SkipListV1<Node<10/*MaxLevel*/>, 50/*NextLevelP*/> skiplist(parallel, sizeof(Node<10>) * entries_per_thread + 1);
    std::vector<RequestGenerator *> req_gens;
    std::vector<std::string_view> keys(total_entries);

    for (uint32_t i = 0; i < parallel; i++)
    {
        uint32_t n_entries = entries_per_thread + (i < remainder ? 1 : 0);
        req_gens.emplace_back(new RequestGenerator(n_entries, i));
    }

    std::vector<std::thread> threads;

    std::cout << "start insert entries.\n";
    auto insert_start = std::chrono::steady_clock::now();

    uint32_t entries_offset = 0;
    for (uint32_t i = 0; i < parallel; i++)
    {
        uint32_t n_entries = entries_per_thread + (i < remainder ? 1 : 0);
        threads.emplace_back([&, i, entries_offset, n_entries]()
        {
            for (uint32_t j = 0; j < n_entries; j++)
            {
                req_gens[i]->generateRequest();
                // req_gens[i]->generateRequest2();
                skiplist.insert(req_gens[i]->mKey, req_gens[i]->mValue, i);
                keys[entries_offset + j] = req_gens[i]->mKey;
            }
        });
        entries_offset += n_entries;
    }

    for (uint32_t i = 0; i < parallel; i++)
    {
        threads[i].join();
    }

    auto insert_end = std::chrono::steady_clock::now();
    std::cout << "insert " << total_entries << " entries with " << parallel << " threads cost " << (insert_end - insert_start).count() / 1000000. << "ms.\n";

    // skiplist.checkBottom();

    // query
    threads.clear();

    uint32_t queries_per_thread = total_queries / query_parallel;
    remainder = total_queries % query_parallel;
    auto query_start = std::chrono::steady_clock::now();

    for (uint32_t i = 0; i < query_parallel; i++)
    {
        uint32_t n_queries = queries_per_thread + (i < remainder ? 1 : 0);
        threads.emplace_back([&, n_queries]()
        {
            RNG random;
            volatile uint64_t value;
            for (uint32_t j = 0; j < n_queries; j++)
            {
                uint32_t idx = random.rand() % total_queries;
                value = skiplist.find(keys[idx]);
                // if (j % 11 == 10) {
                //     std::cout << "finished 10 queries\n";
                // }
            }
        });
    }

    for (uint32_t i = 0; i < query_parallel; i++)
    {
        threads[i].join();
    }

    auto query_end = std::chrono::steady_clock::now();
    std::cout << "query " << total_queries << " keys cost " << (query_end - query_start).count() / 1000000. << "ms.\n";

    return 0;
}

