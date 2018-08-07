// ============================================================================
//                    PriSeT - The Primer Search Tool
// ============================================================================

/*!\file
 * \brief Global Functions for Primer Constraint Checking.
 * \author Marie Hoffmann <marie.hoffmann AT fu-berlin.de>
 */

#pragma once

#include <algorithm>
#include <cmath>

// satisfies the primer_config_concept.
namespace priset
{
//!\brief Wallace rule to compute the melting temperature of a primer sequence.
template<typename sequence_type, typename float_type>
// todo: require base type of string compatible with char via requires concept
float_type primer_melt_wallace(sequence_type primer)
{
    size_t cnt_AT = std::count_if(primer.begin(), primer.end(), [](char c) {return c == 'A' || c == 'T';});
    size_t cnt_CG = std::count_if(primer.begin(), primer.end(), [](char c) {return c == 'C' || c == 'G';});
    return 2*cnt_AT + 4*cnt_CG;
}

//!\brief Salt-adjusted method to compute the melting temperature of a primer sequence.
// input primer:string sequence, Na:float molar Natrium ion concentration
template<typename sequence_type, typename float_type>
float_type primer_melt_salt(sequence_type primer, float_type Na)
{
    float_type cnt_CG = std::count_if(primer.begin(), primer.end(), [](char c) {return c == 'C' || c == 'G';});
    return 100.5 + 41.0*cnt_CG/primer.size() - 820.0/primer.size() + 16.6*std::log10(Na);
}

//!\brief  variation measure for a block of aligned sequences in terms of number of columns
// having different nucleotides. A low score indicates high conservation.
// input sequences:[[]], pos:int, offset:int
/*def variation_score(aligned_sequences, pos, offset):
    transposed = [[aseq.seq[i] for aseq in aligned_sequences] for i in range(pos, pos+offset)]
    score = sum([1 if len(set(col)) > 1 else 0 for col in transposed])
    return score
*/

//!\brief Compress a window of aligned sequences to 1-letter encode.
// codes: '|'
template<typename sequence_type>
// todo: use aligned sequence type
// block needs to be gap-free, N is ignored due to its ambiguity, all sequences are padded to the same length
std::vector<dna> block_compress(std::vector<sequence_type> const aligned_sequences,
    size_t const pos, size_t const offset)
{
    // assert that block end doesn't exceed the aligned sequences lengths
    assert(aligned_sequences[0].size() - pos >= offset);
    //matchstr = ['N' for _ in range(min(offset, len(aligned_sequences[0].seq)-pos))]
    std::vector<dna> as_cx(offset);
    //codes = {1: '|', 2: '2', 3: '3', 4: '4'}
    unsigned short int mask_A = 1, mask_C = 2, mask_G = 4, mask_T = 8;
    for (unsigned int i = 0; i < offset; ++i)
    {
        for (unsigned int j = 0; j < aligned_sequences.size(); ++j)
        {
            unsigned short int column_mask = 0;
            switch(aligned_sequences[i][j])
            {
                case 'A': column_mask |= mask_A; break;
                case 'C': column_mask |= mask_C; break;
                case 'G': column_mask |= mask_G; break;
                case 'T': column_mask |= mask_T; break;
                default: std::cout << "Warning: block_variety scans block with unknown symbol '" <<
                    aligned_sequences[i][j] << "'" << std::endl;
            }
        }
        // build dna string conform to key set of priset::str2dna map
        std::string dna_str = "";
        if (column_mask & mask_A == mask_A)
            dna_str.append('A');
        if (column_mask & mask_C == mask_C)
            dna_str.append('C');
        if (column_mask & mask_G == mask_G)
            dna_str.append('G');
        if (column_mask & mask_T == mask_T)
            dna_str.append('T');

        as_cx[i] = str2dna[dna_str];
    }
    return as_cx
}

/*
# check if all target sequences satisfy melting temperature range and do not differ too much
def filter_melt(aligned_sequences, pos, offset, cfg):
    melt = [primer_melt_wallace(aseq.seq[pos:pos+offset]) for aseq in aligned_sequences]
    max_melt, min_melt = max(melt), min(melt)
    if min_melt >= cfg.var['min_melt_temp'] and max_melt <= cfg.var['max_melt_temp'] and max_melt - min_melt <= cfg.var['max_melt_diff']:
        return True, min_melt, max_melt
    return False, min_melt, max_melt

# GC content in the primer should be between 40-60%, returns True if seqs pass the filter
def filter_GC_content(aligned_sequences, pos, offset, cfg):
    GC_min, GC_max = int(cfg.var['gc_content'][0]*offset), int(cfg.var['gc_content'][1]*offset)
    logging.debug('GC_min, max = [{}, {}]'.format(GC_min, GC_max))
    GC_cnts = [len([1 for nt in aseq.seq[pos:pos+offset] if nt.upper() in ['C', 'G']]) for aseq in aligned_sequences]
    GC_cnts.sort()
    logging.debug('GC_ctns = ' + str(GC_cnts))
    if GC_cnts[0] < GC_min or GC_cnts[-1] > GC_max:
        return False, GC_cnts[0], GC_cnts[-1]
    return True, GC_cnts[0], GC_cnts[-1]

# check for GC at 3' end, DNA sense/'+': 5' to 3', antisense/'-': 3' to 5', should be <= 3 in last 5 bps
def filter_GC_clamp(sequence, sense='+'):
    if sense == '+':
        gc = len([1 for nt in sequence[-5:] if nt in ['C', 'G']])
    else:
        gc = len([1 for nt in sequence[:5] if nt in ['C', 'G']])
    if gc > 3:
        return False, gc
    return True, gc

# check for 2ndary structure hairpin, may only be present at 3' end with a delta(G) = -2 kcal/mol,
# or internally with a delta(G) of -3 kcal/mol
# TODO: upper limit for loop length is disrespected currently
def filter_hairpin(seq, cfg):
    n, min_loop_len = len(seq), int(cfg.var['hairpin_loop_len'][0])
    palindrome_len_rng = range(3, len(seq)/2 - min_loop_len + 1)
    seq_ci = complement(seq)[::-1] # inverted, complemented sequence
    for i in range(len(seq) - 2*palindrome_len_rng[0] - min_loop_len):
        for m in palindrome_len_rng:
            for i_inv in range(n - 2*m - min_loop_len):
                if seq[i:i+m] == seq_ci[i_inv:i_inv+m]:
                    #print seq[i:i+m], ' == ', seq_ci[i_inv:i_inv+m]
                    return False
    return True

'''
    Same sense interaction: sequence is partially homologous to itself.
'''
def filter_selfdimer(seq, cfg):
    seq_rev = seq[::-1]
    return filter_crossdimer(seq, seq[::-1], cfg)

'''
    Pairwise interaction: sequence s is partially homologous to sequence t.
    If primer binding is too strong in terms of delta G, less primers are available for DNA binding.
    Todo: Use discrete FFT convolution to compute all free Gibb's energy values for all overlaps in O(nlogn).
'''
def filter_crossdimer(s, t, cfg):
    n, m = len(s), len(t)
    cnv = [0 for _ in range(len(s)+len(t)-1)]
    for n in range(len(s) + len(t) - 1):
        cnv[n] = sum([delta_G(s[m], t[n-m]) for m in range(len(s))])
    if min(cnv) < cfg.var['delta_G_cross']:
        return False
    return True, min(cnv)

# translate into complementary string without reversing
def complement(dna_sequence):
    m = {'A': 'T', 'C': 'G', 'G': 'C', 'T': 'A', 'N': 'N'}
    return ''.join([m[nt] for nt in dna_sequence])


# one-letter encoding for set of aligned sequences, no gaps
def compress_helper(aligned_sequences, pos, length, bin_codes):
    seq_x = ''
    for i in range(pos, pos+length):
        code = reduce(lambda x, y: x|y, [bin_codes[aseq.seq[i]] for aseq in aligned_sequences])
        seq_x += one_letter_encode[code]
    return seq_x

def compress(aligned_sequences, pos, length):
    return compress_helper(aligned_sequences, pos, length, {'A': 1, 'C': 2, 'G': 4, 'T': 8})

def complement_compress(aligned_sequences, pos, length):
    return compress_helper(aligned_sequences, pos, length, {'A': 8, 'C': 4, 'G': 2, 'T': 1})[::-1]
*/

} // namespace priset
