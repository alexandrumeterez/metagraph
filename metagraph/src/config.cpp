#include "config.hpp"

#include <cstring>
#include <iostream>
#include <unordered_set>
#include <filesystem>

#include "utils.hpp"
#include "threading.hpp"


Config::Config(int argc, const char *argv[]) {
    // provide help overview if no identity was given
    if (argc == 1) {
        print_usage(argv[0]);
        exit(-1);
    }

    // parse identity from first command line argument
    if (!strcmp(argv[1], "build")) {
        identity = BUILD;
    } else if (!strcmp(argv[1], "clean")) {
        identity = CLEAN;
    } else if (!strcmp(argv[1], "merge")) {
        identity = MERGE;
    } else if (!strcmp(argv[1], "extend")) {
        identity = EXTEND;
    } else if (!strcmp(argv[1], "concatenate")) {
        identity = CONCATENATE;
    } else if (!strcmp(argv[1], "compare")) {
        identity = COMPARE;
    } else if (!strcmp(argv[1], "align")) {
        identity = ALIGN;
    } else if (!strcmp(argv[1], "experiment")) {
        identity = EXPERIMENT;
    } else if (!strcmp(argv[1], "stats")) {
        identity = STATS;
    } else if (!strcmp(argv[1], "annotate")) {
        identity = ANNOTATE;
    } else if (!strcmp(argv[1], "coordinate")) {
        identity = ANNOTATE_COORDINATES;
    } else if (!strcmp(argv[1], "merge_anno")) {
        identity = MERGE_ANNOTATIONS;
    } else if (!strcmp(argv[1], "query")) {
        identity = QUERY;
    } else if (!strcmp(argv[1], "server_query")) {
        identity = SERVER_QUERY;
    } else if (!strcmp(argv[1], "transform")) {
        identity = TRANSFORM;
    } else if (!strcmp(argv[1], "transform_anno")) {
        identity = TRANSFORM_ANNOTATION;
    } else if (!strcmp(argv[1], "assemble")) {
        identity = ASSEMBLE;
    } else if (!strcmp(argv[1], "relax_brwt")) {
        identity = RELAX_BRWT;
    } else if (!strcmp(argv[1], "call_variants")) {
        identity = CALL_VARIANTS;
    } else if (!strcmp(argv[1], "parse_taxonomy")) {
        identity = PARSE_TAXONOMY;
    } else {
        print_usage(argv[0]);
        exit(-1);
    }

    // provide help screen for chosen identity
    if (argc == 2) {
        print_usage(argv[0], identity);
        exit(-1);
    }

    const auto get_value = [&](int i) {
        assert(i > 0);
        assert(i < argc);

        if (i + 1 == argc) {
            std::cerr << "Error: no value provided for option "
                      << argv[i] << std::endl;
            print_usage(argv[0], identity);
            exit(-1);
        }
        return argv[i + 1];
    };

    // parse remaining command line items
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            verbose = true;
        } else if (!strcmp(argv[i], "--print")) {
            print_graph = true;
        } else if (!strcmp(argv[i], "--print-col-names")) {
            print_column_names = true;
        } else if (!strcmp(argv[i], "--print-internal")) {
            print_graph_internal_repr = true;
        } else if (!strcmp(argv[i], "--count-kmers")) {
            count_kmers = true;
        } else if (!strcmp(argv[i], "--fwd-and-reverse")) {
            forward_and_reverse = true;
        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--canonical")) {
            canonical = true;
        } else if (!strcmp(argv[i], "--complete")) {
            complete = true;
        } else if (!strcmp(argv[i], "--dynamic")) {
            dynamic = true;
        } else if (!strcmp(argv[i], "--no-shrink")) {
            mark_dummy_kmers = false;
        } else if (!strcmp(argv[i], "--anno-filename")) {
            filename_anno = true;
        } else if (!strcmp(argv[i], "--anno-header")) {
            fasta_anno = true;
        } else if (!strcmp(argv[i], "--header-comment-delim")) {
            fasta_anno_comment_delim = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--anno-label")) {
            anno_labels.emplace_back(get_value(i++));
        } else if (!strcmp(argv[i], "--coord-binsize")) {
            genome_binsize_anno = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--suppress-unlabeled")) {
            suppress_unlabeled = true;
        } else if (!strcmp(argv[i], "--sparse")) {
            sparse = true;
        } else if (!strcmp(argv[i], "--fast")) {
            fast = true;
        } else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--parallel")) {
            parallel = atoi(get_value(i++));
            set_num_threads(parallel);
        } else if (!strcmp(argv[i], "--parts-total")) {
            parts_total = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--part-idx")) {
            part_idx = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--bins-per-thread")) {
            num_bins_per_thread = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--kmer-length")) {
            k = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--min-count")) {
            min_count = std::max(atoi(get_value(i++)), 1);
        } else if (!strcmp(argv[i], "--max-count")) {
            max_count = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--min-count-q")) {
            min_count_quantile = std::max(std::stod(get_value(i++)), 0.);
        } else if (!strcmp(argv[i], "--max-count-q")) {
            max_count_quantile = std::min(std::stod(get_value(i++)), 1.);
        } else if (!strcmp(argv[i], "--count-bins-q")) {
            for (const auto &border : utils::split_string(get_value(i++), " ")) {
                count_slice_quantiles.push_back(std::stod(border));
            }
        } else if (!strcmp(argv[i], "--mem-cap-gb")) {
            memory_available = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--dump-raw-anno")) {
            dump_raw_anno = true;
        } else if (!strcmp(argv[i], "--dump-text-anno")) {
            dump_text_anno = true;
        } else if (!strcmp(argv[i], "--discovery-fraction")) {
            discovery_fraction = std::stof(get_value(i++));
        } else if (!strcmp(argv[i], "--query-presence")) {
            query_presence = true;
        } else if (!strcmp(argv[i], "--filter-present")) {
            filter_present = true;
        } else if (!strcmp(argv[i], "--count-labels")) {
            count_labels = true;
        } else if (!strcmp(argv[i], "--map")) {
            map_sequences = true;
        } else if (!strcmp(argv[i], "--align-seed-unimems")) {
            alignment_seed_unimems = true;
        } else if (!strcmp(argv[i], "--align-edit-distance")) {
            alignment_edit_distance = true;
        } else if (!strcmp(argv[i], "--align-length")) {
            alignment_length = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-queue-size")) {
            alignment_queue_size = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-vertical-bandwidth")) {
            alignment_vertical_bandwidth = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-match-score")) {
            alignment_match_score = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-mm-transition-penalty")) {
            alignment_mm_transition = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-mm-transversion-penalty")) {
            alignment_mm_transversion = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-gap-open-penalty")) {
            alignment_gap_opening_penalty = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-gap-extension-penalty")) {
            alignment_gap_extension_penalty = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-alternative-alignments")) {
            alignment_num_alternative_paths = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-min-cell-score")) {
            alignment_min_cell_score = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-min-path-score")) {
            alignment_min_path_score = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-min-seed-length")) {
            alignment_min_seed_length = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-max-seed-length")) {
            alignment_max_seed_length = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--align-max-num-seeds-per-locus")) {
            alignment_max_num_seeds_per_locus = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--frequency")) {
            frequency = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--distance")) {
            distance = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--outfile-base")) {
            outfbase = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--reference")) {
            refpath = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--header-delimiter")) {
            fasta_header_delimiter = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--labels-delimiter")) {
            anno_labels_delimiter = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--separately")) {
            separately = true;
        } else if (!strcmp(argv[i], "--kmer-mapping-mode")) {
            kmer_mapping_mode = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--num-top-labels")) {
            num_top_labels = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--port")) {
            port = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--suffix")) {
            suffix = get_value(i++);
        } else if (!strcmp(argv[i], "--state")) {
            state = string_to_state(get_value(i++));

        } else if (!strcmp(argv[i], "--anno-type")) {
            anno_type = string_to_annotype(get_value(i++));
        } else if (!strcmp(argv[i], "--graph")) {
            graph_type = string_to_graphtype(get_value(i++));
        } else if (!strcmp(argv[i], "--rename-cols")) {
            rename_instructions_file = std::string(get_value(i++));
        //} else if (!strcmp(argv[i], "--db-path")) {
        //    dbpath = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--annotator")) {
            infbase_annotators.emplace_back(get_value(i++));
        } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--infile-base")) {
            infbase = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--to-adj-list")) {
            to_adj_list = true;
        } else if (!strcmp(argv[i], "--to-fasta")) {
            to_fasta = true;
        } else if (!strcmp(argv[i], "--to-gfa")) {
            to_gfa = true;
        } else if (!strcmp(argv[i], "--unitigs")) {
            to_fasta = true;
            unitigs = true;
        } else if (!strcmp(argv[i], "--primary-kmers")) {
            kmers_in_single_form = true;
        } else if (!strcmp(argv[i], "--header")) {
            header = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--prune-tips")) {
            min_tip_size = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--prune-unitigs")) {
            min_unitig_median_kmer_abundance = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--fallback")) {
            fallback_abundance_cutoff = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--count-dummy")) {
            count_dummy = true;
        } else if (!strcmp(argv[i], "--clear-dummy")) {
            clear_dummy = true;
        } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--len-suffix")) {
            suffix_len = atoi(get_value(i++));
        //} else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--threads")) {
        //    num_threads = atoi(get_value(i++));
        //} else if (!strcmp(argv[i], "--debug")) {
        //    debug = true;
        } else if (!strcmp(argv[i], "--greedy")) {
            greedy_brwt = true;
        } else if (!strcmp(argv[i], "--arity")) {
            arity_brwt = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--relax-arity")) {
            relax_arity_brwt = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "--cache-size")) {
            row_cache_size = atoi(get_value(i++));
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0], identity);
            exit(0);
        } else if (!strcmp(argv[i], "--label-mask-in")) {
            label_mask_in.emplace_back(get_value(i++));
        } else if (!strcmp(argv[i], "--label-mask-out")) {
            label_mask_out.emplace_back(get_value(i++));
        } else if (!strcmp(argv[i], "--label-mask-in-fraction")) {
            label_mask_in_fraction = std::stof(get_value(i++));
        } else if (!strcmp(argv[i], "--label-mask-out-fraction")) {
            label_mask_out_fraction = std::stof(get_value(i++));
        } else if (!strcmp(argv[i], "--label-other-fraction")) {
            label_other_fraction = std::stof(get_value(i++));
        } else if (!strcmp(argv[i], "--label-filter")) {
            label_filter.emplace_back(get_value(i++));
        } else if (!strcmp(argv[i], "--filter-by-kmer")) {
            filter_by_kmer = true;
        } else if (!strcmp(argv[i], "--call-bubbles")) {
            call_bubbles = true;
        } else if (!strcmp(argv[i], "--call-breakpoints")) {
            call_breakpoints = true;
        } else if (!strcmp(argv[i], "--accession")) {
            accession2taxid = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--taxonomy")) {
            taxonomy_nodes = std::string(get_value(i++));
        } else if (!strcmp(argv[i], "--taxonomy-map")) {
            taxonomy_map = std::string(get_value(i++));
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "\nERROR: Unknown option %s\n\n", argv[i]);
            print_usage(argv[0], identity);
            exit(-1);
        } else {
            fname.push_back(argv[i]);
        }
    }

    if (identity == TRANSFORM && to_fasta)
        identity = ASSEMBLE;

    // given kmc_pre and kmc_suf pair, only include one
    // this still allows for the same file to be included multiple times
    std::unordered_set<std::string> kmc_file_set;

    for (auto it = fname.begin(); it != fname.end(); ++it) {
        if (utils::get_filetype(*it) == "KMC"
                && !kmc_file_set.insert(utils::remove_suffix(*it, ".kmc_pre", ".kmc_suf")).second)
            fname.erase(it--);
    }

    utils::set_verbose(verbose);

    if (!fname.size() && identity != STATS
                      && identity != SERVER_QUERY
                      && !(identity == BUILD && complete)
                      && !(identity == CALL_VARIANTS)
                      && !(identity == PARSE_TAXONOMY)
                      && !(identity == CONCATENATE && !infbase.empty())) {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.size())
                fname.push_back(line);
        }
    }

    bool print_usage_and_exit = false;

    if (!count_slice_quantiles.size()) {
        count_slice_quantiles.push_back(0);
        count_slice_quantiles.push_back(1);
    }

    for (size_t i = 1; i < count_slice_quantiles.size(); ++i) {
        if (count_slice_quantiles[i - 1] >= count_slice_quantiles[i]) {
            std::cerr << "Error: bin count quantiles must be provided in strictly increasing order"
                      << std::endl;
            print_usage_and_exit = true;
        }
    }
    if (count_slice_quantiles.front() < 0 || count_slice_quantiles.back() > 1) {
        std::cerr << "Error: bin count quantiles must be in range [0, 1]"
                  << std::endl;
        print_usage_and_exit = true;
    }
    if (count_slice_quantiles.size() == 1) {
        std::cerr << "Error: provide at least two bin count borders"
                  << std::endl;
        print_usage_and_exit = true;
    }

    if (identity != CONCATENATE
            && identity != STATS
            && identity != SERVER_QUERY
            && !(identity == BUILD && complete)
            && !(identity == CALL_VARIANTS)
            && !(identity == PARSE_TAXONOMY)
            && !fname.size())
        print_usage_and_exit = true;

    if (identity == CONCATENATE && !(fname.empty() ^ infbase.empty())) {
        std::cerr << "Error: Either set all chunk filenames"
                  << " or use the -i and -l options" << std::endl;
        print_usage_and_exit = true;
    }

    if (identity == ALIGN && infbase.empty())
        print_usage_and_exit = true;

    if (identity == ALIGN &&
            (alignment_mm_transition < 0
            || alignment_mm_transversion < 0
            || alignment_gap_opening_penalty < 0
            || alignment_gap_extension_penalty < 0)) {
        std::cerr << "Error: alignment penalties should be given as positive integers"
                  << std::endl;
        print_usage_and_exit = true;
    }

    if (count_kmers || query_presence)
        map_sequences = true;

    if ((identity == QUERY || identity == SERVER_QUERY) && infbase.empty())
        print_usage_and_exit = true;

    if (identity == ANNOTATE && infbase.empty())
        print_usage_and_exit = true;

    if (identity == ANNOTATE_COORDINATES && infbase.empty())
        print_usage_and_exit = true;

    if ((identity == ANNOTATE_COORDINATES
            || identity == ANNOTATE
            || identity == EXTEND) && infbase_annotators.size() > 1) {
        std::cerr << "Error: one annotator at most is allowed for extension." << std::endl;
        print_usage_and_exit = true;
    }

    if (identity == ANNOTATE
            && !filename_anno && !fasta_anno && !anno_labels.size()) {
        std::cerr << "Error: No annotation to add" << std::endl;
        print_usage_and_exit = true;
    }

    if ((identity == ANNOTATE || identity == ANNOTATE_COORDINATES)
            && outfbase.empty())
        outfbase = utils::remove_suffix(infbase, ".dbg",
                                                 ".orhashdbg",
                                                 ".hashstrdbg",
                                                 ".bitmapdbg");
    if (identity == EXTEND && infbase.empty())
        print_usage_and_exit = true;

    if ((identity == QUERY || identity == SERVER_QUERY) && infbase_annotators.size() != 1)
        print_usage_and_exit = true;

    if ((identity == TRANSFORM
            || identity == CLEAN
            || identity == ASSEMBLE
            || identity == RELAX_BRWT)
                    && fname.size() != 1)
        print_usage_and_exit = true;

    if ((identity == TRANSFORM
            || identity == CONCATENATE
            || identity == EXTEND
            || identity == MERGE
            || identity == CLEAN
            || identity == TRANSFORM_ANNOTATION
            || identity == MERGE_ANNOTATIONS
            || identity == ASSEMBLE
            || identity == RELAX_BRWT)
                    && outfbase.empty())
        print_usage_and_exit = true;

    if (identity == PARSE_TAXONOMY &&
            ((accession2taxid == "" && taxonomy_nodes == "") || outfbase == ""))
        print_usage_and_exit = true;

    if (identity == MERGE && fname.size() < 2)
        print_usage_and_exit = true;

    if (identity == COMPARE && fname.size() != 2)
        print_usage_and_exit = true;

    if (discovery_fraction < 0 || discovery_fraction > 1)
        print_usage_and_exit = true;

    if (min_count >= max_count) {
        std::cerr << "Error: max-count must be greater than min-count" << std::endl;
        print_usage(argv[0], identity);
    }

    if (alignment_max_seed_length < alignment_min_seed_length) {
        std::cerr << "Error: align-max-seed-length has to be at least align-min-seed-length" << std::endl;
        print_usage_and_exit = true;
    }

    if (outfbase.size()
            && !(utils::check_if_writable(outfbase)
                    || (separately
                        && std::filesystem::is_directory(std::filesystem::status(outfbase))))) {
        std::cerr << "Error: Can't write to " << outfbase << std::endl
                  << "Check if the path is correct" << std::endl;
        exit(1);
    }

    // if misused, provide help screen for chosen identity and exit
    if (print_usage_and_exit) {
        print_usage(argv[0], identity);
        exit(-1);
    }
}

std::string Config::state_to_string(StateType state) {
    switch (state) {
        case STAT:
            return "fast";
        case DYN:
            return "dynamic";
        case SMALL:
            return "small";
        case FAST:
            return "faster";
        default:
            assert(false);
            return "Never happens";
    }
}

Config::StateType Config::string_to_state(const std::string &string) {
    if (string == "fast") {
        return StateType::STAT;
    } else if (string == "dynamic") {
        return StateType::DYN;
    } else if (string == "small") {
        return StateType::SMALL;
    } else if (string == "faster") {
        return StateType::FAST;
    } else {
        throw std::runtime_error("Error: unknown graph state");
    }
}

std::string Config::annotype_to_string(AnnotationType state) {
    switch (state) {
        case ColumnCompressed:
            return "column";
        case RowCompressed:
            return "row";
        case BRWT:
            return "brwt";
        case BinRelWT_sdsl:
            return "bin_rel_wt_sdsl";
        case BinRelWT:
            return "bin_rel_wt";
        case RowFlat:
            return "flat";
        case RBFish:
            return "rbfish";
        default:
            assert(false);
            return "Never happens";
    }
}

Config::AnnotationType Config::string_to_annotype(const std::string &string) {
    if (string == "column") {
        return AnnotationType::ColumnCompressed;
    } else if (string == "row") {
        return AnnotationType::RowCompressed;
    } else if (string == "brwt") {
        return AnnotationType::BRWT;
    } else if (string == "bin_rel_wt_sdsl") {
        return AnnotationType::BinRelWT_sdsl;
    } else if (string == "bin_rel_wt") {
        return AnnotationType::BinRelWT;
    } else if (string == "flat") {
        return AnnotationType::RowFlat;
    } else if (string == "rbfish") {
        return AnnotationType::RBFish;
    } else {
        std::cerr << "Error: unknown annotation representation" << std::endl;
        exit(1);
    }
}

Config::GraphType Config::string_to_graphtype(const std::string &string) {
    if (string == "succinct") {
        return GraphType::SUCCINCT;

    } else if (string == "hash") {
        return GraphType::HASH;

    } else if (string == "hashpacked") {
        return GraphType::HASH_PACKED;

    } else if (string == "hashstr") {
        return GraphType::HASH_STR;

    } else if (string == "bitmap") {
        return GraphType::BITMAP;

    } else {
        std::cerr << "Error: unknown graph representation" << std::endl;
        exit(1);
    }
}

void Config::print_usage(const std::string &prog_name, IdentityType identity) {
    fprintf(stderr, "Metagraph: comprehensive metagenome graph representation -- Version 0.1\n\n");

    const char annotation_list[] = "('column', 'row', 'bin_rel_wt_sdsl', 'bin_rel_wt', 'flat', 'rbfish', 'brwt')";

    switch (identity) {
        case NO_IDENTITY: {
            fprintf(stderr, "Usage: %s <command> [command specific options]\n\n", prog_name.c_str());

            fprintf(stderr, "Available commands:\n");

            fprintf(stderr, "\tbuild\t\tconstruct a graph object from input sequence\n");
            fprintf(stderr, "\t\t\tfiles in fast[a|q] formats into a given graph\n\n");

            fprintf(stderr, "\tclean\t\tclean an existing graph and extract sequences from it\n");
            fprintf(stderr, "\t\t\tin fast[a|q] formats\n\n");

            fprintf(stderr, "\textend\t\textend an existing graph with new sequences from\n");
            fprintf(stderr, "\t\t\tfiles in fast[a|q] formats\n\n");

            fprintf(stderr, "\tmerge\t\tintegrate a given set of graph structures\n");
            fprintf(stderr, "\t\t\tand output a new graph structure\n\n");

            fprintf(stderr, "\tconcatenate\tcombine the results of the external merge or\n");
            fprintf(stderr, "\t\t\tconstruction and output the resulting graph structure\n\n");

            fprintf(stderr, "\tcompare\t\tcheck whether two given graphs are identical\n\n");

            fprintf(stderr, "\talign\t\talign sequences provided in fast[a|q] files to graph\n\n");

            fprintf(stderr, "\tstats\t\tprint graph statistics for given graph(s)\n\n");

            fprintf(stderr, "\tannotate\tgiven a graph and a fast[a|q] file, annotate\n");
            fprintf(stderr, "\t\t\tthe respective kmers\n\n");

            fprintf(stderr, "\tcoordinate\tgiven a graph and a fast[a|q] file, annotate\n");
            fprintf(stderr, "\t\t\tkmers with their respective coordinates in genomes\n\n");

            fprintf(stderr, "\tmerge_anno\tmerge annotation columns\n\n");

            fprintf(stderr, "\ttransform\tgiven a graph, transform it to other formats\n\n");

            fprintf(stderr, "\ttransform_anno\tchange representation of the graph annotation\n\n");

            fprintf(stderr, "\tassemble\tgiven a graph, extract sequences from it\n\n");

            fprintf(stderr, "\trelax_brwt\toptimize the tree structure in brwt annotator\n\n");

            fprintf(stderr, "\tquery\t\tannotate sequences from fast[a|q] files\n\n");
            fprintf(stderr, "\tserver_query\tannotate received sequences and send annotations back\n\n");

            fprintf(stderr, "\tcall_variants\tgenerate a masked annotated graph and call variants\n");
            fprintf(stderr, "\t\t\trelative to unmasked graph\n\n");

            fprintf(stderr, "\tparse_taxonomy\tgenerate NCBI Accession ID to Taxonomy ID mapper\n\n");

            return;
        }
        case EXPERIMENT: {
            fprintf(stderr, "Usage: %s experiment ???\n\n", prog_name.c_str());
        } break;
        case BUILD: {
            fprintf(stderr, "Usage: %s build [options] FILE1 [[FILE2] ...]\n"
                            "\tEach input file is given in FASTA, FASTQ, VCF, or KMC format.\n"
                            "\tNote that VCF files must be in plain text or bgzip format.\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for build:\n");
            fprintf(stderr, "\t   --min-count [INT] \tmin k-mer abundance, including [1]\n");
            fprintf(stderr, "\t   --max-count [INT] \tmax k-mer abundance, excluding [inf]\n");
            fprintf(stderr, "\t   --min-count-q [INT] \tmin k-mer abundance quantile (min-count is used by default) [0.0]\n");
            fprintf(stderr, "\t   --max-count-q [INT] \tmax k-mer abundance quantile (max-count is used by default) [1.0]\n");
            fprintf(stderr, "\t   --reference [STR] \tbasename of reference sequence (for parsing VCF files) []\n");
            fprintf(stderr, "\t   --fwd-and-reverse \tadd both forward and reverse complement sequences [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --graph [STR] \tgraph representation: succinct / bitmap / hash / hashpacked / hashstr [succinct]\n");
            fprintf(stderr, "\t   --count-kmers \tcount k-mers and build weighted graph [off]\n");
            fprintf(stderr, "\t-k --kmer-length [INT] \tlength of the k-mer to use [3]\n");
            fprintf(stderr, "\t-c --canonical \t\tindex only canonical k-mers (e.g. for read sets) [off]\n");
            fprintf(stderr, "\t   --complete \t\tconstruct a complete graph (only for Bitmap graph) [off]\n");
            fprintf(stderr, "\t   --mem-cap-gb [INT] \tpreallocated buffer size in Gb [0]\n");
            fprintf(stderr, "\t   --dynamic \t\tuse dynamic build method [off]\n");
            fprintf(stderr, "\t-l --len-suffix [INT] \tk-mer suffix length for building graph from chunks [0]\n");
            fprintf(stderr, "\t   --suffix \t\tbuild graph chunk only for k-mers with the suffix given [off]\n");
            fprintf(stderr, "\t-o --outfile-base [STR]\tbasename of output file []\n");
            fprintf(stderr, "\t   --no-shrink \t\tdo not build mask for dummy k-mers (only for Succinct graph) [off]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case CLEAN: {
            fprintf(stderr, "Usage: %s clean -o <outfile-base> [options] GRAPH\n\n", prog_name.c_str());
            fprintf(stderr, "Available options for clean:\n");
            fprintf(stderr, "\t   --min-count [INT] \t\tmin k-mer abundance, including [1]\n");
            fprintf(stderr, "\t   --max-count [INT] \t\tmax k-mer abundance, excluding [inf]\n");
            fprintf(stderr, "\t   --prune-tips [INT] \t\tprune all dead ends shorter than this value [1]\n");
            fprintf(stderr, "\t   --prune-unitigs [INT] \tprune all unitigs with median k-mer counts smaller\n"
                            "\t                         \t\tthan this value (0: auto) [1]\n");
            fprintf(stderr, "\t   --fallback [INT] \t\tfallback threshold if the automatic one cannot be determined [1]\n");
            fprintf(stderr, "\t   --count-bins-q [FLOAT ...] \tbinning quantiles for partitioning k-mers with\n"
                            "\t                              \t\tdifferent abundance levels ['0 1']\n"
                            "\t                              \t\tExample: --count-bins-q '0 0.33 0.66 1'\n");
            // fprintf(stderr, "\n");
            // fprintf(stderr, "\t-o --outfile-base [STR]\tbasename of output file []\n");
            fprintf(stderr, "\t   --unitigs \t\t\textract unitigs instead of contigs [off]\n");
            fprintf(stderr, "\t   --to-fasta \t\t\tdump clean sequences to compressed FASTA file [off]\n");
            // fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case EXTEND: {
            fprintf(stderr, "Usage: %s extend -i <GRAPH> -o <extended_graph_basename> [options] FILE1 [[FILE2] ...]\n"
                            "\tEach input file is given in FASTA, FASTQ, VCF, or KMC format.\n"
                            "\tNote that VCF files must be in plain text or bgzip format.\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for extend:\n");
            fprintf(stderr, "\t   --min-count [INT] \tmin k-mer abundance, including [1]\n");
            fprintf(stderr, "\t   --max-count [INT] \tmax k-mer abundance, excluding [inf]\n");
            fprintf(stderr, "\t   --reference [STR] \tbasename of reference sequence (for parsing VCF files) []\n");
            fprintf(stderr, "\t   --fwd-and-reverse \tadd both forward and reverse complement sequences [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t-a --annotator [STR] \tannotator to extend []\n");
            fprintf(stderr, "\t-o --outfile-base [STR]\tbasename of output file []\n");
            // fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case ALIGN: {
            fprintf(stderr, "Usage: %s align -i <GRAPH> [options] FASTQ1 [[FASTQ2] ...]\n\n", prog_name.c_str());

            fprintf(stderr, "\t   --fwd-and-reverse \t\talign both forward and reverse complement sequences [off]\n");
            fprintf(stderr, "\t   --header-comment-delim [STR]\tdelimiter for joining fasta header with comment [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --map \t\t\tmap k-mers to graph exactly instead of aligning.\n");
            fprintf(stderr, "\t         \t\t\t\tTurned on if --count-kmers or --query-presence are set [off]\n");
            fprintf(stderr, "\t-k --kmer-length [INT]\t\tlength of mapped k-mers (at most graph's k) [k]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --count-kmers \t\tfor each sequence, report the number of k-mers discovered in graph [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --query-presence \t\ttest sequences for presence, report as 0 or 1 [off]\n");
            fprintf(stderr, "\t   --discovery-fraction [FLOAT] fraction of k-mers required to count sequence [1.0]\n");
            fprintf(stderr, "\t   --kmer-mapping-mode \t\tlevel of heuristics to use for mapping k-mers (0, 1, or 2) [0]\n");
            fprintf(stderr, "\t   --filter-present \t\treport only present input sequences as FASTA [off]\n");
            fprintf(stderr, "\n");
            // fprintf(stderr, "\t-d --distance [INT] \t\tmax allowed alignment distance [0]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "Available options for alignment:\n");
            fprintf(stderr, "\t-o --outfile-base [STR]\tbasename of output file []\n");
            fprintf(stderr, "\t   --align-seed-unimems \t\t\tuse maximum exact matches in unitigs as seeds [off]\n");
            fprintf(stderr, "\t   --align-edit-distance \t\t\tuse unit costs for scoring matrix [off]\n");
            fprintf(stderr, "\t   --align-alternative-alignments \t\tthe number of alternative paths to report per seed [1]\n");
            fprintf(stderr, "\t   --align-match-score [INT]\t\t\tpositive match score [2]\n");
            fprintf(stderr, "\t   --align-mm-transition-penalty [INT]\t\tpositive transition penalty (DNA only) [1]\n");
            fprintf(stderr, "\t   --align-mm-transversion-penalty [INT]\tpositive transversion penalty (DNA only) [2]\n");
            fprintf(stderr, "\t   --align-gap-open-penalty [INT]\t\tpositive gap opening penalty [3]\n");
            fprintf(stderr, "\t   --align-gap-extension-penalty [INT]\t\tpositive gap extension penalty [1]\n");
            fprintf(stderr, "\t   --align-queue-size [INT]\t\t\tmaximum size of the priority queue for alignment [50]\n");
            fprintf(stderr, "\t   --align-vertical-bandwidth [INT]\t\tmaximum width of a window to consider in alignment step [10]\n");
            fprintf(stderr, "\t   --align-min-seed-length [INT]\t\tthe minimum length of a seed [graph k]\n");
            fprintf(stderr, "\t   --align-max-seed-length [INT]\t\tthe maximum length of a seed [graph k]\n");
            fprintf(stderr, "\t   --align-min-cell-score [INT]\t\t\tthe minimum value that a cell in the alignment table can hold [0]\n");
            fprintf(stderr, "\t   --align-min-path-score [INT]\t\t\tthe minimum score that a reported path can have [0]\n");
            fprintf(stderr, "\t   --align-max-num-seeds-per-locus [INT]\tthe maximum number of allowed inexact seeds per locus [1]\n");
        } break;
        case COMPARE: {
            fprintf(stderr, "Usage: %s compare [options] GRAPH1 GRAPH2\n\n", prog_name.c_str());

            // fprintf(stderr, "Available options for compare:\n");
            // fprintf(stderr, "\t   --internal \t\tcompare internal graph representations\n");
        } break;
        case MERGE: {
            fprintf(stderr, "Usage: %s merge -o <graph_basename> [options] GRAPH1 GRAPH2 [[GRAPH3] ...]\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for merge:\n");
            fprintf(stderr, "\t-b --bins-per-thread [INT] \tnumber of bins each thread computes on average [1]\n");
            fprintf(stderr, "\t   --dynamic \t\t\tdynamic merge by adding traversed paths [off]\n");
            fprintf(stderr, "\t   --part-idx [INT] \t\tidx to use when doing external merge []\n");
            fprintf(stderr, "\t   --parts-total [INT] \t\ttotal number of parts in external merge[]\n");
            fprintf(stderr, "\t-p --parallel [INT] \t\tuse multiple threads for computation [1]\n");
        } break;
        case CONCATENATE: {
            fprintf(stderr, "Usage: %s concatenate -o <graph_basename> [options] [[CHUNK] ...]\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for merge:\n");
            fprintf(stderr, "\t   --graph [STR] \tgraph representation: succinct / bitmap [succinct]\n");
            fprintf(stderr, "\t-i --infile-base [STR] \tload graph chunks from files '<infile-base>.<suffix>.<type>.chunk' []\n");
            fprintf(stderr, "\t-l --len-suffix [INT] \titerate all possible suffixes of the length given [0]\n");
            fprintf(stderr, "\t-c --canonical \t\tcanonical graph mode (e.g. for read sets) [off]\n");
            fprintf(stderr, "\t   --clear-dummy \terase all redundant dummy edges [off]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case TRANSFORM: {
            fprintf(stderr, "Usage: %s transform -o <outfile-base> [options] GRAPH\n\n", prog_name.c_str());

            // fprintf(stderr, "\t-o --outfile-base [STR] basename of output file []\n");
            fprintf(stderr, "\t   --clear-dummy \terase all redundant dummy edges [off]\n");
            fprintf(stderr, "\t   --prune-tips [INT] \tprune all dead ends of this length and shorter [0]\n");
            fprintf(stderr, "\t   --state [STR] \tchange state of succinct graph: fast / faster / dynamic / small [fast]\n");
            fprintf(stderr, "\t   --to-adj-list \twrite adjacency list to file [off]\n");
            fprintf(stderr, "\t   --to-fasta \t\textract sequences from graph and dump to compressed FASTA file [off]\n");
            fprintf(stderr, "\t   --unitigs \t\textract all unitigs from graph and dump to compressed FASTA file [off]\n");
            fprintf(stderr, "\t   --primary-kmers \toutput each k-mer only in one if its forms (canonical/non-canonical) [off]\n");
            fprintf(stderr, "\t   --to-gfa \t\tdump graph layout to GFA [off]\n");
            fprintf(stderr, "\t   --header [STR] \theader for sequences in FASTA output []\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case ASSEMBLE: {
            fprintf(stderr, "Usage: %s assemble -o <outfile-base> [options] GRAPH\n"
                            "\tAssemble contigs from de Bruijn graph and dump to compressed FASTA file.\n\n", prog_name.c_str());

            // fprintf(stderr, "\t-o --outfile-base [STR] \t\tbasename of output file []\n");
            fprintf(stderr, "\t   --prune-tips [INT] \tprune all dead ends of this length and shorter [0]\n");
            fprintf(stderr, "\t   --unitigs \t\textract unitigs [off]\n");
            fprintf(stderr, "\t   --primary-kmers \toutput each k-mer only in one if its forms (canonical/non-canonical) [off]\n");
            fprintf(stderr, "\t   --to-gfa \t\tdump graph layout to GFA [off]\n");
            fprintf(stderr, "\t   --header [STR] \theader for sequences in FASTA output []\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t-a --annotator [STR] \t\t\tannotator to load []\n");
            fprintf(stderr, "\t   --label-mask-in [STR] \t\tlabel to include in masked graph\n");
            fprintf(stderr, "\t   --label-mask-out [STR] \t\tlabel to exclude from masked graph\n");
            fprintf(stderr, "\t   --label-mask-in-fraction [FLOAT] \tminimum fraction of mask-in labels among the set of masked labels [1.0]\n");
            fprintf(stderr, "\t   --label-mask-out-fraction [FLOAT] \tmaximum fraction of mask-out labels among the set of masked labels [0.0]\n");
            fprintf(stderr, "\t   --label-other-fraction [FLOAT] \tmaximum fraction of other labels allowed [1.0]\n");
            fprintf(stderr, "\t   --filter-by-kmer \t\t\tmask out graph k-mers individually [off]\n");
        } break;
        case STATS: {
            fprintf(stderr, "Usage: %s stats [options] GRAPH1 [[GRAPH2] ...]\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for stats:\n");
            fprintf(stderr, "\t   --print \t\tprint graph table to the screen [off]\n");
            fprintf(stderr, "\t   --print-internal \tprint internal graph representation to screen [off]\n");
            fprintf(stderr, "\t   --count-dummy \tshow number of dummy source and sink edges [off]\n");
            fprintf(stderr, "\t-a --annotator [STR] \tannotation []\n");
            fprintf(stderr, "\t   --print-col-names \tprint names of the columns in annotation to screen [off]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case ANNOTATE: {
            fprintf(stderr, "Usage: %s annotate -i <GRAPH> [options] FILE1 [[FILE2] ...]\n"
                            "\tEach file is given in FASTA, FASTQ, VCF, or KMC format.\n"
                            "\tNote that VCF files must be in plain text or bgzip format.\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for annotate:\n");
            fprintf(stderr, "\t   --min-count [INT] \tmin k-mer abundance, including [1]\n");
            fprintf(stderr, "\t   --max-count [INT] \tmax k-mer abundance, excluding [inf]\n");
            fprintf(stderr, "\t   --reference [STR] \tbasename of reference sequence (for parsing VCF files) []\n");
            fprintf(stderr, "\t   --fwd-and-reverse \tprocess both forward and reverse complement sequences [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --anno-type [STR] \ttarget annotation representation: column / row [column]\n");
            fprintf(stderr, "\t-a --annotator [STR] \tannotator to update []\n");
            fprintf(stderr, "\t   --sparse \t\tuse the row-major sparse matrix to annotate graph [off]\n");
            fprintf(stderr, "\t-o --outfile-base [STR] basename of output file [<GRAPH>]\n");
            fprintf(stderr, "\t   --separately \tannotate each file independently and dump to the same directory [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --anno-filename \t\tinclude filenames as annotation labels [off]\n");
            fprintf(stderr, "\t   --anno-header \t\textract annotation labels from headers of sequences in files [off]\n");
            fprintf(stderr, "\t   --header-comment-delim [STR]\tdelimiter for joining fasta header with comment [off]\n");
            fprintf(stderr, "\t   --header-delimiter [STR]\tdelimiter for splitting annotation header into multiple labels [off]\n");
            fprintf(stderr, "\t   --anno-label [STR]\t\tadd label to annotation for all sequences from the files passed []\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
            // fprintf(stderr, "\t   --fast \t\t\tannotate in fast regime (leads to repeated labels and bigger annotation) [off]\n");
        } break;
        case ANNOTATE_COORDINATES: {
            fprintf(stderr, "Usage: %s coordinate -i <GRAPH> [options] FASTA1 [[FASTA2] ...]\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for annotate:\n");
            fprintf(stderr, "\t   --fwd-and-reverse \t\tprocess both forward and reverse complement sequences [off]\n");
            fprintf(stderr, "\t-a --annotator [STR] \t\tannotator to update []\n");
            fprintf(stderr, "\t-o --outfile-base [STR] \tbasename of output file [<GRAPH>]\n");
            fprintf(stderr, "\t   --coord-binsize [INT]\tstepsize for k-mer coordinates in input sequences from the fasta files [1000]\n");
            fprintf(stderr, "\t   --fast \t\t\tannotate in fast regime [off]\n");
            fprintf(stderr, "\t-p --parallel [INT] \t\tuse multiple threads for computation [1]\n");
        } break;
        case MERGE_ANNOTATIONS: {
            fprintf(stderr, "Usage: %s merge_anno -o <annotator_basename> [options] ANNOT1 [[ANNOT2] ...]\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for annotate:\n");
            fprintf(stderr, "\t   --anno-type [STR] \ttarget annotation representation [column]\n");
            fprintf(stderr, "\t\t"); fprintf(stderr, annotation_list); fprintf(stderr, "\n");
            // fprintf(stderr, "\t   --sparse \t\tuse the row-major sparse matrix to annotate graph [off]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case TRANSFORM_ANNOTATION: {
            fprintf(stderr, "Usage: %s transform_anno -o <annotator_basename> [options] ANNOTATOR\n\n", prog_name.c_str());

            fprintf(stderr, "\t-o --outfile-base [STR] basename of output file []\n");
            fprintf(stderr, "\t   --rename-cols [STR]\tfile with rules for renaming annotation labels []\n");
            fprintf(stderr, "\t                      \texample: 'L_1 L_1_renamed\n");
            fprintf(stderr, "\t                      \t          L_2 L_2_renamed\n");
            fprintf(stderr, "\t                      \t          L_2 L_2_renamed\n");
            fprintf(stderr, "\t                      \t          ... ...........'\n");
            fprintf(stderr, "\t   --anno-type [STR] \ttarget annotation format [column]\n");
            fprintf(stderr, "\t\t"); fprintf(stderr, annotation_list); fprintf(stderr, "\n");
            fprintf(stderr, "\t   --arity  \t\tarity in the brwt tree [2]\n");
            fprintf(stderr, "\t   --greedy  \t\tuse greedy column partitioning in brwt construction [off]\n");
            fprintf(stderr, "\t   --fast  \t\ttransform annotation in memory without streaming [off]\n");
            fprintf(stderr, "\t   --dump-raw-anno  \tdump the columns of the annotator as separate binary files [off]\n");
            fprintf(stderr, "\t   --dump-text-anno  \tdump the columns of the annotator as separate text files [off]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case RELAX_BRWT: {
            fprintf(stderr, "Usage: %s relax_brwt -o <annotator_basename> [options] ANNOTATOR\n\n", prog_name.c_str());

            fprintf(stderr, "\t-o --outfile-base [STR] basename of output file []\n");
            fprintf(stderr, "\t   --relax-arity [INT] \trelax brwt tree to optimize arity limited to this number [10]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
        } break;
        case QUERY: {
            fprintf(stderr, "Usage: %s query -i <GRAPH> -a <ANNOTATION> [options] FILE1 [[FILE2] ...]\n"
                            "\tEach input file is given in FASTA or FASTQ format.\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for query:\n");
            fprintf(stderr, "\t   --fwd-and-reverse \tquery both forward and reverse complement sequences [off]\n");
            fprintf(stderr, "\t   --sparse \t\tuse row-major sparse matrix for row annotation [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --count-labels \t\tcount labels for k-mers from querying sequences [off]\n");
            fprintf(stderr, "\t   --num-top-labels \t\tmaximum number of frequent labels to print [off]\n");
            fprintf(stderr, "\t   --discovery-fraction [FLOAT] fraction of labeled k-mers required for annotation [1.0]\n");
            fprintf(stderr, "\t   --labels-delimiter [STR]\tdelimiter for annotation labels [\":\"]\n");
            fprintf(stderr, "\t   --suppress-unlabeled \tdo not show results for sequences missing in graph [off]\n");
            // fprintf(stderr, "\t-d --distance [INT] \tmax allowed alignment distance [0]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t-p --parallel [INT] \tuse multiple threads for computation [1]\n");
            fprintf(stderr, "\t   --cache-size [INT] \tnumber of uncompressed rows to store in the cache [0]\n");
        } break;
        case SERVER_QUERY: {
            fprintf(stderr, "Usage: %s server_query -i <GRAPH> -a <ANNOTATION> [options]\n\n", prog_name.c_str());

            fprintf(stderr, "Available options for server_query:\n");
            fprintf(stderr, "\t   --port [INT] \tTCP port for incoming connections [5555]\n");
            fprintf(stderr, "\t   --sparse \t\tuse the row-major sparse matrix to annotate graph [off]\n");
            // fprintf(stderr, "\t-o --outfile-base [STR] \tbasename of output file []\n");
            // fprintf(stderr, "\t-d --distance [INT] \tmax allowed alignment distance [0]\n");
            fprintf(stderr, "\t-p --parallel [INT] \tmaximum number of parallel connections [1]\n");
            fprintf(stderr, "\t   --cache-size [INT] \tnumber of uncompressed rows to store in the cache [0]\n");
        } break;
        case CALL_VARIANTS: {
            fprintf(stderr, "Usage: %s call_variants -i <GRAPH> -a <annotation> [options]\n", prog_name.c_str());

            fprintf(stderr, "Available options for call_variants:\n");
            fprintf(stderr, "\t-o --outfile-base [STR] \t\tbasename of output file []\n");
            fprintf(stderr, "\t   --label-mask-in [STR] \t\tlabel to include in masked graph []\n");
            fprintf(stderr, "\t   --label-mask-out [STR] \t\tlabel to exclude from masked graph []\n");
            fprintf(stderr, "\t   --label-mask-in-fraction [FLOAT] \tminimum fraction of mask-in labels among the set of masked labels [1.0]\n");
            fprintf(stderr, "\t   --label-mask-out-fraction [FLOAT] \tmaximum fraction of mask-out labels among the set of masked labels [0.0]\n");
            fprintf(stderr, "\t   --label-other-fraction [FLOAT] \tmaximum fraction of other labels allowed [1.0]\n");
            fprintf(stderr, "\t   --filter-by-kmer \t\t\tmask out graph k-mers individually [off]\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "\t   --call-bubbles \t\tcall labels from bubbles [off]\n");
            fprintf(stderr, "\t   --call-breakpoints \t\tcall labels from breakpoints [off]\n");
            fprintf(stderr, "\t   --label-filter [STR] \tdiscard variants with this label []\n");
            fprintf(stderr, "\t   --taxonomy-map [STR] \tfilename of taxonomy map file []\n");
            fprintf(stderr, "\t   --cache-size [INT] \t\tnumber of uncompressed rows to store in the cache [0]\n");
        } break;
        case PARSE_TAXONOMY: {
            fprintf(stderr, "Usage: %s parse_taxonomy -o <OUTBASE> [options]\n", prog_name.c_str());

            fprintf(stderr, "Available options for parse_taxonomy:\n");
            fprintf(stderr, "\t-o --outfile-base [STR] basename of output file []\n");
            fprintf(stderr, "\t   --accession [STR] \tfilename of the accession2taxid.gz file []\n");
            fprintf(stderr, "\t   --taxonomy [STR] \tfilename of the nodes.dmp file []\n");
        } break;
    }

    fprintf(stderr, "\n\tGeneral options:\n");
    fprintf(stderr, "\t-v --verbose \t\tswitch on verbose output [off]\n");
    fprintf(stderr, "\t-h --help \t\tprint usage info\n");
    fprintf(stderr, "\n");
}
