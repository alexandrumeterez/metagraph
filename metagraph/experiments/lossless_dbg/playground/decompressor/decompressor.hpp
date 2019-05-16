//
// Created by Jan Studený on 2019-03-11.
//
#include <utility>
#include <iostream>
#include <map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tclap/CmdLine.h>
#include <filesystem>

using TCLAP::ValueArg;
using TCLAP::MultiArg;
using TCLAP::UnlabeledValueArg;
using TCLAP::UnlabeledMultiArg;
using TCLAP::ValuesConstraint;

using json = nlohmann::json;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#pragma clang diagnostic ignored "-Wcomma"

#include "path_database_baseline_wavelet_deprecated.hpp"
#include "samplers.hpp"
#include "utilities.hpp"

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"



using namespace std;
using namespace std::string_literals;


using node_index = SequenceGraph::node_index;


int main_decompressor(int argc, char *argv[]) {
    TCLAP::CmdLine cmd("Decompress reads",' ', "0.1");
    TCLAP::ValueArg<std::string> inputArg("i",
                                         "input",
                                         "Folder where to store the compressed files.",
                                         true,
                                         "",
                                         "string");
    TCLAP::ValueArg<std::string> outputArg("o",
                                          "output",
                                          "FASTA/Q file that should be compressed",
                                          true,
                                          "",
                                          "string");
    cmd.add(inputArg);
    cmd.add(outputArg);
    cmd.parse(argc, argv);
    auto input_folder = inputArg.getValue();
    auto output_filename = outputArg.getValue();
    auto db = PathDatabaseBaselineWaveletDeprecated::deserialize(input_folder);
    auto reads = db.decode_all_reads();
    write_reads_to_fasta(reads,output_filename);

    return 0;
}