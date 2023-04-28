#include <algorithm>
#include <assert.h>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

#include "spoa/spoa.hpp"
#include "HaplotypeGenerator.h"
#include "RepeatBlock.h"
#include "../stringops.h"

void HaplotypeGenerator::trim(int ideal_min_length, int32_t& region_start, int32_t& region_end, std::vector<std::string>& sequences) const {
  int min_len = INT_MAX;
  for (unsigned int i = 0; i < sequences.size(); i++)
    min_len = std::min(min_len, (int)sequences[i].size());
  if (min_len <= ideal_min_length)
    return;

  int max_left_trim = 0, max_right_trim = 0;
  while (max_left_trim < min_len-ideal_min_length){
    unsigned int j = 1;
    while (j < sequences.size()){
      if (sequences[j][max_left_trim] != sequences[j-1][max_left_trim])
	break;
      j++;
    }
    if (j != sequences.size()) 
      break;
    max_left_trim++;
  }
  while (max_right_trim < min_len-ideal_min_length){
    char c = sequences[0][sequences[0].size()-1-max_right_trim];
    unsigned int j = 1;
    while (j < sequences.size()){
      if (sequences[j][sequences[j].size()-1-max_right_trim] != c)
	break;
      j++;
    }
    if (j != sequences.size())
      break;
    max_right_trim++;
  }

  // Don't trim past the flanks
  max_left_trim  = std::min(LEFT_PAD,  max_left_trim);
  max_right_trim = std::min(RIGHT_PAD, max_right_trim);

  // Don't trim past the padding flanks
  max_left_trim  = std::max(0, std::min(min_len-RIGHT_PAD, max_left_trim));
  max_right_trim = std::max(0, std::min(min_len-LEFT_PAD,  max_right_trim));

  // Determine the left and right trims that clip as much as possible
  // but are as equal in size as possible
  int left_trim, right_trim;
  if (min_len - 2*std::min(max_left_trim, max_right_trim) <= ideal_min_length){
    left_trim = right_trim = std::min(max_left_trim, max_right_trim);
    while (min_len - left_trim - right_trim < ideal_min_length){
      if (left_trim > right_trim)
	left_trim--;
      else
	right_trim--;
    }
  }
  else {
    if (max_left_trim > max_right_trim){
      right_trim = max_right_trim;
      left_trim  = std::min(max_left_trim, min_len-ideal_min_length-max_right_trim);
    }
    else {
      left_trim  = max_left_trim;
      right_trim = std::min(max_right_trim, min_len-ideal_min_length-max_left_trim);
    }
  }

  // Adjust sequences and position
  for (unsigned int i = 0; i < sequences.size(); i++)
    sequences[i] = sequences[i].substr(left_trim, sequences[i].size()-left_trim-right_trim);
  region_start += left_trim;
  region_end   -= right_trim;
}

bool HaplotypeGenerator::extract_sequence(const Alignment& aln, int32_t region_start, int32_t region_end, std::string& seq) const {
  if (aln.get_start() >= region_start) return false;
  if (aln.get_stop()  <= region_end)   return false;

  int align_index = 0; // Index into alignment string
  int char_index  = 0; // Index of current base in current CIGAR element
  int32_t pos     = aln.get_start();
  auto cigar_iter = aln.get_cigar_list().begin();
  // Extract region sequence if fully spanned by alignment
  std::stringstream reg_seq;
  while (cigar_iter != aln.get_cigar_list().end()){
    if (char_index == cigar_iter->get_num()){
      cigar_iter++;
      char_index = 0;
    }
    else if (pos > region_end){
      if (reg_seq.str() == "")
	seq = "";
      else
	seq = uppercase(reg_seq.str());
      return true;
    }
    else if (pos == region_end){
      if (cigar_iter->get_type() == 'I'){
	reg_seq << aln.get_alignment().substr(align_index, cigar_iter->get_num());
	align_index += cigar_iter->get_num();
	char_index = 0;
	cigar_iter++;
      }
      else {
	if (reg_seq.str() == "")
	  seq = "";
	else
	  seq = uppercase(reg_seq.str());
	return true;
      }
    }
    else if (pos >= region_start){
      int32_t num_bases = std::min(region_end-pos, cigar_iter->get_num()-char_index);
      switch(cigar_iter->get_type()){	 
      case 'I':
	num_bases = cigar_iter->get_num();
	reg_seq << aln.get_alignment().substr(align_index, num_bases);
	break;
      case '=': case 'X':
	reg_seq << aln.get_alignment().substr(align_index, num_bases);
	pos += num_bases;
	break;
      case 'D':
	pos += num_bases;
	break;
      default:
	printErrorAndDie("Invalid CIGAR char in extractRegionSequences()");
	break;
      }
      align_index += num_bases;
      char_index  += num_bases;
    }
    else {
      int32_t num_bases;
      if (cigar_iter->get_type() == 'I')
	num_bases = cigar_iter->get_num()-char_index;
      else {
	num_bases = std::min(region_start-pos, cigar_iter->get_num()-char_index);
	pos      += num_bases;
      }
      align_index  += num_bases;
      char_index   += num_bases;
    }
  }
  printErrorAndDie("Logical error in extract_sequence");
  return false;
}

void HaplotypeGenerator::poa(const std::vector<std::string>& seqs, std::string& consensus) const {
    std::unique_ptr<spoa::AlignmentEngine> alignment_engine;
    alignment_engine =spoa::AlignmentEngine::Create(static_cast<spoa::AlignmentType>(1),(std::int8_t) 1, (std::int8_t) -1,(std::int8_t) -1);
    spoa::Graph graph{};
    for (const auto& seq: seqs) {

        auto alignment = alignment_engine->Align(seq, graph);
        graph.AddAlignment(alignment, seq);

    }
    consensus = graph.GenerateConsensus();
}

void HaplotypeGenerator::needleman_wunsch(const std::string& cent_seq, const std::string& read_seq, int& score) const {
  const int n = cent_seq.size(), m = read_seq.size();
  const int gap_score = 1, match_score = 0, mismatch_score = 1;
  int dp[n+1][m+1];

  for (int i = 0; i < n+1; i++){
    dp[i][0] = i * gap_score;
  }

  for (int j = 0; j < m+1; j++){
    dp[0][j] = j * gap_score;
  }

  for (int i = 1; i < n+1; i++){
    for (int j = 1; j < m+1; j++){
      int S = (cent_seq[i-1] == read_seq[j-1]) ? match_score : mismatch_score;
      dp[i][j] = std::min(dp[i-1][j] + gap_score, std::min(dp[i][j-1] + gap_score, dp[i-1][j-1] + S));
    }
  }
  score = dp[n][m];
}

// Clustering reads based on the similarity between them. The similarity is computed based on edit distance.
void HaplotypeGenerator::greedy_clustering(const std::vector<std::string>& seqs, std::map<std::string, std::vector<std::string>>& clusters) const {
  std::vector<std::string> centroids;
  centroids.push_back(seqs[0]); // first centroid is the first sequence
  clusters[seqs[0]].push_back(seqs[0]);;
  for (int i = 1; i < seqs.size(); i++) {
    int T = 0.1 * seqs[i].size();
    int min_score = INT_MAX;
    int min_cntr;
    for (int j = 0; j < centroids.size(); j++) {
      int score;
      needleman_wunsch(seqs[i], centroids[j], score);
      if (score < min_score) {
        min_score = score;
        min_cntr = j;
      }
    }
    if (min_score < T) { // the sequence is similar enough to one of the current centroids
      clusters[centroids[min_cntr]].push_back(seqs[i]);
    }
    else { // make a new centroid
      centroids.push_back(seqs[i]);
      clusters[seqs[i]].push_back(seqs[i]);
    }
  }
}

// Optimizing clusters by performing poa in each cluster to
bool HaplotypeGenerator::merge_clusters(const std::vector<std::string>& new_centroids, std::map<std::string, std::vector<std::string>>& clusters) const {
    bool updated = false;
    for (int i = 0; i < new_centroids.size(); i++){
        int T = 0.1 * new_centroids[i].size(); // TODO make it constant
        for (int j = 0; j < new_centroids.size(); j++){
            if (i != j && (clusters.find(new_centroids[i]) != clusters.end()) && (clusters.find(new_centroids[j]) != clusters.end())){
                int score;
                needleman_wunsch(new_centroids[i], new_centroids[j], score); // Find the edit distance between centroids of two clusters
                if (score < T){
                    updated = true;
                    for (auto seq:clusters[new_centroids[j]]){
                        clusters[new_centroids[i]].push_back(seq); // Merge clusters if the centroids are similar enough
                    }
                    clusters.erase(new_centroids[j]);
                }
            }
        }
    }
    return updated;
}

void HaplotypeGenerator::gen_candidate_seqs(const std::string& ref_seq, int ideal_min_length,
					    const std::vector< std::vector<Alignment> >& alignments, const std::vector<std::string>& vcf_alleles,
					    int32_t& region_start, int32_t& region_end, std::vector<std::string>& sequences) const {
  assert(sequences.empty());
  std::map<std::string, double> sample_counts;
  std::map<std::string, int> read_counts, must_inc;
  int tot_reads = 0, tot_samples = 0;

  // Determine the number of reads and number of samples supporting each allele
  for (unsigned int i = 0; i < alignments.size(); i++){
    int samp_reads = 0;
    std::map<std::string, int> counts;
    for (unsigned int j = 0; j < alignments[i].size(); j++){
      std::string subseq;
      if (extract_sequence(alignments[i][j], region_start, region_end, subseq)){
	read_counts[subseq] += 1;
	counts[subseq]      += 1;
	tot_reads++;
	samp_reads++;
      }
    }

    // Identify alleles strongly supported by sample
    for (auto iter = counts.begin(); iter != counts.end(); iter++){
      if (iter->second >= MIN_READS_STRONG_SAMPLE && iter->second >= MIN_FRAC_STRONG_SAMPLE*samp_reads)
	must_inc[iter->first] += 1;
      sample_counts[iter->first] += iter->second*1.0/samp_reads;
    }

    if (samp_reads > 0)
      tot_samples++;
  }

  // Add VCF alleles to list (apart from reference sequence) and remove from other data structures
  int ref_index = -1;
  for (unsigned int i = 0; i < vcf_alleles.size(); i++){
    sequences.push_back(vcf_alleles[i]);
    auto iter_1 = sample_counts.find(vcf_alleles[i]);
    if (iter_1 != sample_counts.end()){
      sample_counts.erase(iter_1);
      read_counts.erase(vcf_alleles[i]);
    }
    auto iter_2 = must_inc.find(vcf_alleles[i]);
    if (iter_2 != must_inc.end())
      must_inc.erase(iter_2);
    if (vcf_alleles[i].compare(ref_seq) == 0)
      ref_index = i;
  }
  
  // Add alleles with strong support from a subset of samples
  for (auto iter = must_inc.begin(); iter != must_inc.end(); iter++){
    if (iter->second >= MIN_STRONG_SAMPLES){
      auto iter_1 = sample_counts.find(iter->first);
      auto iter_2 = read_counts.find(iter->first);
      sample_counts.erase(iter_1);
      read_counts.erase(iter_2);
      sequences.push_back(iter->first);
      if (iter->first.compare(ref_seq) == 0)
	ref_index = sequences.size()-1;
    }
  }

  // Identify additional alleles satisfying thresholds
  for (auto iter = sample_counts.begin(); iter != sample_counts.end(); iter++){
    if (iter->second > MIN_FRAC_SAMPLES*tot_samples || read_counts[iter->first] > MIN_FRAC_READS*tot_reads){
      sequences.push_back(iter->first);
      if (ref_index == -1 && (iter->first.compare(ref_seq) == 0))
	ref_index = sequences.size()-1;
    }
  }
  
  // Arrange reference sequence as first element
  if (ref_index == -1)
    sequences.insert(sequences.begin(), ref_seq);
  else {
    sequences[ref_index] = sequences[0];
    sequences[0]         = ref_seq;
  }


  // Identify how many alignments don't have a candidate haplotype:
  std::vector<std::string> not_added_total;
  for (unsigned int i = 0; i < alignments.size(); i++){
    std::vector<std::string> not_added_sample;
    int samp_reads = 0;
    for (unsigned int j = 0; j < alignments[i].size(); j++){
      std::string subseq;
      if (extract_sequence(alignments[i][j], region_start, region_end, subseq)) {
        samp_reads++;
        if (std::find(sequences.begin(), sequences.end(), subseq) == sequences.end()) {
          not_added_sample.push_back(subseq);
        }
      }
    }
    if (not_added_sample.size() > samp_reads*0.25){ // TODO make it a constant
      //std::copy(not_added_sample.begin(),not_added_sample.end(),std::inserter(not_added_total,not_added_total.end()));
      not_added_total.insert(not_added_total.end(), not_added_sample.begin(), not_added_sample.end());
    }
  }
  if (not_added_total.size() > 0) {
    std::cerr << "Skipped reads " << not_added_total.size() << std::endl;
    std::sort(not_added_total.begin() + 1, not_added_total.end(), orderByLengthAndSequence);
    not_added_total.erase( unique( not_added_total.begin(), not_added_total.end() ), not_added_total.end()); //remove duplicate elements
    std::map<std::string, std::vector<std::string>> clusters;
    greedy_clustering(not_added_total, clusters);

    bool not_converged = true;
    // refining clusters by replacing centroids from one of the
    // sequences to partial order alignment of strings in each cluster
    // continue until convergence
    while (not_converged) {
         std::map<std::string, std::vector<std::string>> updated_clusters;
         std::vector<std::string> new_centroids;
         for (auto iter = clusters.begin(); iter != clusters.end(); iter++) {
              // std::cout << "centroid " << iter->first << " " << iter->second.size() << std::endl;
              std::string consensus;
              poa(iter->second, consensus); // Find the centroid of each cluster by performing poa on strings of each cluster
              new_centroids.push_back(consensus);
              // std::cout << "consensus  " << consensus << std::endl;
              updated_clusters[consensus] = iter->second;
         }
         std::sort(new_centroids.begin() + 1, new_centroids.end(), orderByLengthAndSequence);
         not_converged = merge_clusters(new_centroids, updated_clusters);
         clusters = updated_clusters;
    }

     // Add centroids of refined clusters to haplotype sequences
     for (auto iter = clusters.begin(); iter != clusters.end(); iter++) {
       //std::cout << "consensus sequence size " << iter->first.size() << ". n sequences: " << iter->second.size() << std::endl;
       if (iter->second.size() > 9) { // only include clusters that have considerable reads.
        std::cerr << "added haplotype of size " << iter->first.size() << std::endl;
        if (std::find(sequences.begin(), sequences.end(), iter->first) == sequences.end()){ // if the centeroid is not already in the candidate haplotypes
            sequences.push_back(iter->first);
        }
       }
     }
  }

  // Sort regions by length and then by sequence (apart from reference sequence)
  std::sort(sequences.begin()+1, sequences.end(), orderByLengthAndSequence);

  for (auto seq:sequences){
    std::cout << "candidate haplotype size: " << seq.size() << std::endl;
  }

  // Clip identical regions
  trim(ideal_min_length, region_start, region_end, sequences);
}

void HaplotypeGenerator::get_aln_bounds(const std::vector< std::vector<Alignment> >& alignments,
					int32_t& min_aln_start, int32_t& max_aln_stop) const {
  // Determine the minimum and maximum alignment boundaries
  min_aln_start = INT_MAX;
  max_aln_stop  = INT_MIN;
  for (auto vec_iter = alignments.begin(); vec_iter != alignments.end(); vec_iter++){
    for (auto aln_iter = vec_iter->begin(); aln_iter != vec_iter->end(); aln_iter++){
      min_aln_start = std::min(min_aln_start, aln_iter->get_start());
      max_aln_stop  = std::max(max_aln_stop,  aln_iter->get_stop());
    }
  }
}

bool HaplotypeGenerator::add_vcf_haplotype_block(int32_t pos, const std::string& chrom_seq,
						 const std::vector<std::string>& vcf_alleles, const StutterModel* stutter_model){
  if (!failure_msg_.empty())
    printErrorAndDie("Unable to add a VCF haplotype block, as a previous addition failed");
  assert(!vcf_alleles.empty());
  int32_t region_start = pos;
  int32_t region_end   = region_start + vcf_alleles[0].size();
  assert(uppercase(vcf_alleles[0]).compare(uppercase(chrom_seq.substr(region_start, region_end-region_start))) == 0);

  // Ensure that we don't exceed the chromosome bounds
  if (region_start < REF_FLANK_LEN || region_end + REF_FLANK_LEN >= chrom_seq.size()){
    failure_msg_ = "Haplotype blocks are too near to the chromosome ends";
    return false;
  }

  // Ensure that the haplotype block doesn't overlap with previous blocks
  if (!hap_blocks_.empty() && (region_start < hap_blocks_.back()->end() + MIN_BLOCK_SPACING)){
    failure_msg_ = "Haplotype blocks are too near to one another";
    return false;
  }

  hap_blocks_.push_back(new RepeatBlock(region_start, region_end, uppercase(vcf_alleles[0]), stutter_model->period(), stutter_model));
  for (unsigned int i = 1; i < vcf_alleles.size(); i++){
    std::string seq = uppercase(vcf_alleles[i]);
    hap_blocks_.back()->add_alternate(seq);
  }

  return true;
}

bool HaplotypeGenerator::add_haplotype_block(const Region& region, const std::string& chrom_seq, const std::vector< std::vector<Alignment> >& alignments,
					     const std::vector<std::string>& vcf_alleles, const StutterModel* stutter_model){
  if (!failure_msg_.empty())
    printErrorAndDie("Unable to add a haplotype block, as a previous addition failed");

  // Ensure that we don't exceed the chromosome bounds
  if (region.start() < REF_FLANK_LEN + LEFT_PAD || region.stop() + REF_FLANK_LEN + RIGHT_PAD > chrom_seq.size()){
    failure_msg_ = "Haplotype blocks are too near to the chromosome ends";
    return false;
  }

  // Extract the alignment boundaries
  int32_t min_aln_start, max_aln_stop;
  get_aln_bounds(alignments, min_aln_start, max_aln_stop);

  // Ensure that we have some spanning alignments to work with
  int32_t region_start = region.start() - LEFT_PAD;
  int32_t region_end   = region.stop()  + RIGHT_PAD;
  std::string ref_seq  = uppercase(chrom_seq.substr(region_start, region_end-region_start));
  if (min_aln_start + 5 >= region_start || max_aln_stop - 5 <= region_end){
    failure_msg_ = "No spanning alignments";
    return false;
  }

  // Extend each VCF allele by padding size
  std::vector<std::string> padded_vcf_alleles;
  if (vcf_alleles.size() != 0){
    std::string lflank = uppercase(chrom_seq.substr(region_start,  region.start()-region_start));
    std::string rflank = uppercase(chrom_seq.substr(region.stop(), region_end-region.stop()));
    for (unsigned int i = 0; i < vcf_alleles.size(); i++)
      padded_vcf_alleles.push_back(lflank + uppercase(vcf_alleles[i]) + rflank);
    assert(padded_vcf_alleles[0].compare(ref_seq) == 0);
  }
  
  // Extract candidate STR sequences (using some padding to ensure indels near STR ends are included)
  std::vector<std::string> sequences;
  int ideal_min_length = 3*region.period(); // Would ideally have at least 3 repeat units in each allele after trimming
  gen_candidate_seqs(ref_seq, ideal_min_length, alignments, padded_vcf_alleles, region_start, region_end, sequences);

  // Ensure that the new haplotype block won't overlap with previous blocks
  if (!hap_blocks_.empty() && (region_start < hap_blocks_.back()->end() + MIN_BLOCK_SPACING)){
    failure_msg_ = "Haplotype blocks are too near to one another";
    return false;
  }

  // Add the new haplotype block
  hap_blocks_.push_back(new RepeatBlock(region_start, region_end, sequences.front(), stutter_model->period(), stutter_model));
  for (unsigned int i = 1; i < sequences.size(); i++)
    hap_blocks_.back()->add_alternate(sequences[i]);
  return true;
}

bool HaplotypeGenerator::fuse_haplotype_blocks(const std::string& chrom_seq){
  if (!failure_msg_.empty())
    printErrorAndDie("Unable to fuse haplotype blocks, as previous additions failed");
  if (hap_blocks_.empty())
    printErrorAndDie("Unable to fuse haplotype blocks, as none have been added");
  assert(REF_FLANK_LEN > 10);
  assert(hap_blocks_.front()->start() >= REF_FLANK_LEN);
  assert(hap_blocks_.back()->end() + REF_FLANK_LEN <= chrom_seq.size());

  // Trim boundaries so that the reference flanks aren't too long
  int32_t min_start = std::min(hap_blocks_.front()->start()-10, std::max(hap_blocks_.front()->start() - REF_FLANK_LEN, min_aln_start_));
  int32_t max_stop  = std::max(hap_blocks_.back()->end()+10,    std::min(hap_blocks_.back()->end()    + REF_FLANK_LEN, max_aln_stop_));

  // Interleave the existing variant blocks with new reference-only haplotype blocks
  std::vector<HapBlock*> fused_blocks;
  int32_t start = min_start;
  for (int i = 0; i < hap_blocks_.size(); i++){
    int32_t end = hap_blocks_[i]->start();
    fused_blocks.push_back(new HapBlock(start, end, uppercase(chrom_seq.substr(start, end-start))));
    fused_blocks.push_back(hap_blocks_[i]);
    start = hap_blocks_[i]->end();
  }
  fused_blocks.push_back(new HapBlock(start, max_stop, uppercase(chrom_seq.substr(start, max_stop-start))));

  hap_blocks_ = fused_blocks;
  finished_   = true;
  return true;
}
