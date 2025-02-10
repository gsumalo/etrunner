#include "convenience.hpp"
#include <future>
#include <boost/process.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

namespace convenience {

std::string read_file(const boost::filesystem::path & path)
{
    std::string rv;

    if (boost::filesystem::exists(path)) {
        boost::filesystem::ifstream file;
        file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
        file.open(path, std::ios_base::binary);
        std::istreambuf_iterator<char> it(file), endIt;
        rv.assign(it, endIt);
    }

    return rv;
}

static std::mutex RUN_PROCESS_MUTEX;

int run_process(
        const boost::filesystem::path & executable,
        std::vector<std::string> args,
        std::string_view std_in,
        std::string & std_out,
        std::string & std_err)
{
    int rv(EXIT_FAILURE);

    try {
        boost::asio::io_service ios;
        std::future<std::string> standard_output, standard_error;

        std::unique_lock guard(RUN_PROCESS_MUTEX);
        boost::process::child process(executable.string(), args, boost::process::std_in < boost::asio::buffer(std_in),
                boost::process::std_out > standard_output, boost::process::std_err > standard_error, ios);
        guard.unlock();

        ios.run();
        process.wait();

        std_out = standard_output.get();
        std_err = standard_error.get();
        rv = process.exit_code();
    } catch (...) {
    } 

    return rv;
}

}   // namespace convenience
