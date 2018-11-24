#include <stdio.h>

#include "gtest/gtest.h"

#define private public
#define protected public

#include "dbg_succinct.hpp"
#include "kmer.hpp"
#include "utils.hpp"

typedef sdsl::uint256_t KMerBaseType;
const size_t kBitsPerChar = utils::KmerExtractor::kLogSigma;
typedef KMer<KMerBaseType, kBitsPerChar> KMER;
const size_t kSizeOfKmer = sizeof(KMerBaseType);


template <typename KMER>
std::string kmer_codec(const std::string &test_kmer);

std::string kmer_codec_256(const std::string &test_kmer) {
    return kmer_codec<KMER>(test_kmer);
}

void test_kmer_codec_256(const std::string &test_kmer,
                         const std::string &test_compare_kmer) {
    ASSERT_EQ(test_kmer.length(), test_compare_kmer.length());
    ASSERT_EQ(test_compare_kmer.length(), kmer_codec_256(test_kmer).length());
    EXPECT_EQ(test_compare_kmer, kmer_codec_256(test_kmer));
}

TEST(KmerEncodeTest_256, Invertible) {
    test_kmer_codec_256("ATGG", "ATGG");
}

/*
TEST(KmerEncodeTest_256, Operations) {
    for (uint8_t j = 1; j <= kMax; ++j) {
        char curchar = utils::KmerExtractor::decode(j - 1);
        std::string long_seq = std::string(2, curchar);
        KMER kmer(long_seq, utils::KmerExtractor::encode);
        int shift = kSizeOfKmer * 8 / kBitsPerChar;
        for (int i = 3; i <= shift; ++i) {
            kmer <<= kBitsPerChar;
            kmer |= j;
            ASSERT_EQ(kmer.to_string(utils::KmerExtractor::alphabet),
                      long_seq + std::string(i - 2, curchar));
        }
        while (shift--) {
            kmer >>= kBitsPerChar;
            ASSERT_EQ(kmer.to_string(utils::KmerExtractor::alphabet),
                      long_seq + std::string(shift - 2, curchar));
        }
    }
}
*/

TEST(KmerEncodeTest_256, BitShiftBuild) {
    std::string long_seq = "ATGCCTGA";
    while (long_seq.length() < kSizeOfKmer * 8 / kBitsPerChar) {
        long_seq += long_seq;
    }
    long_seq = long_seq.substr(0, kSizeOfKmer * 8 / kBitsPerChar);
    //test bit shifting
    KMER kmer_builtup(utils::KmerExtractor::encode(
        std::string(long_seq.rbegin() + 1,
                    long_seq.rbegin() + 3)
    ));
    for (int i = long_seq.length() - 4; i >= 0; --i) {
        kmer_builtup.seq_ = kmer_builtup.seq_.operator<<(kBitsPerChar);
        kmer_builtup.seq_ |= utils::KmerExtractor::encode(long_seq[i]) + 1;
    }
    kmer_builtup.seq_ = kmer_builtup.seq_.operator<<(kBitsPerChar);
    kmer_builtup.seq_ |= utils::KmerExtractor::encode(long_seq[long_seq.length() - 1]) + 1;
    std::string dec = kmer_builtup.to_string(utils::KmerExtractor::alphabet);
    ASSERT_EQ(long_seq, dec);

    test_kmer_codec_256(long_seq, long_seq);
}

TEST(KmerEncodeTest_256, UpdateKmer) {
    KMER kmer[2] = {
        KMER(utils::KmerExtractor::encode("ATGC")),
        KMER(utils::KmerExtractor::encode("TGCT"))
    };
    KMER updated = kmer[0];
    KMER::update_kmer(4, utils::KmerExtractor::encode('T'),
                         utils::KmerExtractor::encode('C'), &updated.seq_);
    EXPECT_EQ(kmer[1], updated);
    EXPECT_EQ(kmer[0], kmer[1].prev_kmer(4, utils::KmerExtractor::encode('A')));
}

TEST(KmerEncodeTest_256, UpdateKmerLong) {
    std::string long_seq = "ATGCCTGA";
    while (long_seq.length() < kSizeOfKmer * 8 / kBitsPerChar) {
        long_seq += long_seq;
    }
    long_seq = long_seq.substr(0, kSizeOfKmer * 8 / kBitsPerChar);
    std::string long_seq_alt(long_seq.substr(1));
    long_seq_alt.push_back('T');
    KMER kmer[2] = {
        KMER(utils::KmerExtractor::encode(long_seq)),
        KMER(utils::KmerExtractor::encode(long_seq_alt))
    };
    KMER::update_kmer(long_seq.length(),
                utils::KmerExtractor::encode('T'),
                utils::KmerExtractor::encode(long_seq.back()),
                &kmer[0].seq_);
    EXPECT_EQ(kmer[1], kmer[0]);
    EXPECT_EQ(kmer[1].to_string(utils::KmerExtractor::alphabet),
              kmer[0].to_string(utils::KmerExtractor::alphabet));
}

TEST(KmerEncodeTest_256, UpdateKmerVsConstruct) {
    std::string long_seq0 = "AAGGCAGCCTACCCCTCTGTCTCCACCTTTGAGAAACACTCATCCTCAGGCCATGCAGTGGAAN";
    long_seq0.resize(std::min(kSizeOfKmer * 8 / kBitsPerChar,
                              long_seq0.size()));
    std::string long_seq1 =  "AGGCAGCCTACCCCTCTGTCTCCACCTTTGAGAAACACTCATCCTCAGGCCATGCAGTGGAANT";
    long_seq1.resize(std::min(kSizeOfKmer * 8 / kBitsPerChar,
                              long_seq0.size()));
    auto seq0 = utils::KmerExtractor::encode(long_seq0);
    KMER kmer0(KMER::pack_kmer(seq0.begin(), seq0.size()));
    KMER::update_kmer(
            long_seq0.length(),
            utils::KmerExtractor::encode(long_seq1.back()),
            utils::KmerExtractor::encode(long_seq0.back()),
            reinterpret_cast<KMerBaseType*>(&kmer0));
    std::string reconst_seq1 = kmer0.to_string(utils::KmerExtractor::alphabet);
    EXPECT_EQ(long_seq1, reconst_seq1);

    seq0.emplace_back(utils::KmerExtractor::encode(long_seq1.back()));
    KMER kmer1(KMER::pack_kmer(seq0.begin() + 1, seq0.size() - 1));
    std::string reconst_seq2 = kmer1.to_string(utils::KmerExtractor::alphabet);
    EXPECT_EQ(long_seq1, reconst_seq2);
}

TEST(KmerEncodeTest_256, InvertibleEndDol) {
    test_kmer_codec_256("ATG$", "ATG$");
}

TEST(KmerEncodeTest_256, InvertibleStartDol) {
    test_kmer_codec_256("$ATGG", "$ATGG");
}

TEST(KmerEncodeTest_256, InvertibleBothDol) {
    test_kmer_codec_256("$ATG$", "$ATG$");
}

#ifndef _PROTEIN_GRAPH
TEST(KmerEncodeTest_256, InvalidChars) {
    test_kmer_codec_256("ATGH", "ATGN");
}
#endif

void test_kmer_less_256(const std::string &k1,
                        const std::string &k2, bool truth) {
    KMER kmer[2] = {
        KMER(utils::KmerExtractor::encode(k1)),
        KMER(utils::KmerExtractor::encode(k2))
    };
    ASSERT_EQ(truth, kmer[0] < kmer[1]);
}

TEST(KmerEncodeTest_256, LessEdge) {
    test_kmer_less_256("ATGC", "ATGG", true);
}

TEST(KmerEncodeTest_256, Less) {
    test_kmer_less_256("ACTG", "GCTG", true);
}

TEST(KmerEncodeTest_256, LessLong) {
    test_kmer_less_256(
        std::string(kSizeOfKmer * 8 / kBitsPerChar - 1, 'A') +  "C",
        std::string(kSizeOfKmer * 8 / kBitsPerChar - 1, 'A') +  "T",
        true
    );

    test_kmer_less_256(
        std::string(kSizeOfKmer * 8 / kBitsPerChar - 2, 'A') + "CA",
        std::string(kSizeOfKmer * 8 / kBitsPerChar - 2, 'A') + "TA",
        true
    );
}

void test_kmer_suffix_256(std::string k1, std::string k2, bool truth) {
    KMER kmer[2] = {
        KMER(utils::KmerExtractor::encode(k1)),
        KMER(utils::KmerExtractor::encode(k2))
    };
    ASSERT_EQ(truth, KMER::compare_suffix(kmer[0], kmer[1], 1));
}

TEST(KmerEncodeTest_256, CompareSuffixTrue) {
    test_kmer_suffix_256("ACTG", "GCTG", true);
}

TEST(KmerEncodeTest_256, CompareSuffixFalse) {
    test_kmer_suffix_256("ATTG", "ACTG", false);
}

TEST(KmerEncodeTest_256, CompareSuffixTrueLong) {
    std::string long_seq(kSizeOfKmer * 8 / kBitsPerChar, 'A');

    *(long_seq.rbegin()) = 'T';
    *(++long_seq.rbegin()) = 'C';

    std::string long_seq_alt(long_seq);

    long_seq_alt[0] = 'T';
    KMER kmer[2] = {
        KMER(utils::KmerExtractor::encode(long_seq)),
        KMER(utils::KmerExtractor::encode(long_seq_alt))
    };
    ASSERT_TRUE(KMER::compare_suffix(kmer[0], kmer[1], 1));

    //shift, then compare
    long_seq_alt[kSizeOfKmer * 8 / kBitsPerChar - 2] = 'T';

    kmer[0].seq_
        = kmer[0].seq_ >> static_cast<int>((kSizeOfKmer * 8 / kBitsPerChar - 2)
                                                * kBitsPerChar);

    kmer[1] = KMER(utils::KmerExtractor::encode(
        long_seq_alt.substr(kSizeOfKmer * 8 / kBitsPerChar - 2)
    ));

    ASSERT_TRUE(KMER::compare_suffix(kmer[0], kmer[1], 1));
}

TEST(KmerEncodeTest_256, CompareSuffixFalseLong) {
    std::string long_seq(kSizeOfKmer * 8 / kBitsPerChar, 'A');

    *(long_seq.rbegin()) = 'T';
    *(++long_seq.rbegin()) = 'C';

    std::string long_seq_alt(long_seq);

    long_seq_alt[1] = 'T';

    test_kmer_suffix_256(long_seq, long_seq_alt, false);
}

TEST(KmerEncodeTest_256, SizeOfClass) {
    EXPECT_EQ(kSizeOfKmer, sizeof(KMER));
}

#if _DNA_GRAPH
TEST(KmerEncodeTest_256, TestPrint) {
    KMER kmer(std::vector<uint64_t>(sizeof(KMerBaseType) * 8 / kBitsPerChar, 1),
              sizeof(KMerBaseType) * 8 / kBitsPerChar);
    std::stringstream ss;
    ss << kmer;
    std::string out;
    ss >> out;
    EXPECT_EQ("2492492492492492492492492492492492492492492492492492492492492492", out);
}
#endif
