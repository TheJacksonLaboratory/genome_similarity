#include <iostream>
#include <iterator>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

#include "umdhmm-v1.02/nrutil.h"
#include "umdhmm-v1.02/hmm.h"

#include "DataStruct.h"
#include "CallHmm.h"

namespace {
void PrintHmm (const HMM& hmm, const int& T, const int* O) {
	std::cerr << "DEBUG: CallHmm::HmmAndViterbi" << std::endl;
	std::cerr << "=====Start HMM table printing=====" << std::endl;
	std::cerr << "N, M: " << hmm.N << ", " << hmm.M << std::endl;
	std::cerr << "A" << std::endl;
	for (int i = 0; i < hmm.N; ++i) {
		std::cerr << "A" << i << std::endl;
		for (int j = 0; j < hmm.N; ++j)
			std::cerr << hmm.A[i][j] << " ";
		std::cerr << std::endl;
	}

	std::cerr << "B" << std::endl;
	for (int i = 0; i < hmm.N; ++i) {
		std::cerr << "B" << i << std::endl;
		for (int j = 0; j < hmm.M; ++j)
			std::cerr << hmm.B[i][j] << " ";
		std::cerr << std::endl;
	}

	std::cerr << "pi" << std::endl;
	for (int i = 0; i < hmm.N; ++i) {
		std::cerr << hmm.pi[i] << " ";
	}

	std::cerr << std::endl;
	std::cerr << "T: " << T << std::endl;
	std::cerr << "O" << std::endl;
	for (int i = 0; i < T; ++i) {
		std::cerr << O[i] << " ";
	}
	std::cerr << std::endl;
	std::cerr << "=====End HMM table printing=====" << std::endl;
}

inline bool SortByCoordinate(const SHmmStats & a, const SHmmStats & b) {
	if (a.chr != b.chr) return a.chr < b.chr;
	else return a.pos < b.pos;
}

inline bool SortByCoordinateHeap(const SHmmStatsHeap & a, const SHmmStatsHeap & b) {
	return a.hmm_stats.pos < b.hmm_stats.pos;
}

inline bool SortByLength(const SHmmStatsHeap & a, const SHmmStatsHeap & b) {
	return a.hmm_stats.length < b.hmm_stats.length;
}

inline bool CheckMerge(SHmmStats & pilot, const SHmmStats & target) {
	bool merged = false;
	// There should not any overlap between two.
	if ((pilot.pos + pilot.length <= target.pos) || (target.pos + target.length <= pilot.pos)) {
		const bool pilot_is_left_hand = (pilot.pos + pilot.length - 1 < target.pos);
		const unsigned int gap = pilot_is_left_hand ? target.pos - pilot.pos - pilot.length
						: pilot.pos - target.pos - target.length;

		// Merge
		if ((gap / static_cast<float>(pilot.length)) < 0.1) {
			merged = true;
			if (pilot_is_left_hand) {
				const unsigned int end_pos = target.pos + target.length - 1;
				pilot.length = end_pos - pilot.pos + 1;
			} else {
				pilot.length = pilot.pos + pilot.length - 1 - target.pos + 1;
				pilot.pos = target.pos;
			}
		}
	}

	return merged;
}

void ConsolidateStats(std::vector <SHmmStatsHeap> & smooth_result, std::vector <SHmmStatsHeap> & heap) {
	std::sort(heap.begin(), heap.end(), SortByLength);
	for (std::vector <SHmmStatsHeap>::reverse_iterator ite = heap.rbegin(); ite != heap.rend(); ++ite) {
		if (!smooth_result[ite->id].merged && ite->hmm_stats.stats != 3) {
#ifdef DEBUG
			std::cerr << "[Merging Anchor] " << ite->hmm_stats.chr << "\t" << ite->hmm_stats.pos << "\t" 
					<< ite->hmm_stats.length << "\t" << ite->hmm_stats.stats << std::endl;
#endif
			// Forward merging
			for (unsigned int i = ite->id + 1; i < smooth_result.size(); ++i) {
#ifdef DEBUG
				std::cerr << "\t[Merging Target] " << smooth_result[i].hmm_stats.chr << "\t" << smooth_result[i].hmm_stats.pos << "\t" << smooth_result[i].hmm_stats.length << "\t" << smooth_result[i].hmm_stats.stats << std::endl;
#endif
				if (smooth_result[i].merged) break;
				if (smooth_result[i].hmm_stats.stats == 3) continue;
				//const bool consistant_type = ite->hmm_stats.stats == smooth_result[i].hmm_stats.stats;
				//const bool consistant_type = ((ite->hmm_stats.stats == 1 || ite->hmm_stats.stats == 2) 
				//					&& (smooth_result[i].hmm_stats.stats == 1 || smooth_result[i].hmm_stats.stats == 2))
				//				|| ((ite->hmm_stats.stats == 4 || ite->hmm_stats.stats == 5)
				//					&& (smooth_result[i].hmm_stats.stats == 4 || smooth_result[i].hmm_stats.stats == 5));
				const bool consistant_type = (ite->hmm_stats.stats == smooth_result[i].hmm_stats.stats)
								|| ((ite->hmm_stats.stats == 4 || ite->hmm_stats.stats == 5)
									&& (smooth_result[i].hmm_stats.stats == 4 || smooth_result[i].hmm_stats.stats == 5));
				if (!consistant_type) { // Different stats
					break;
				} else {
					if (CheckMerge(ite->hmm_stats, smooth_result[i].hmm_stats)) {
#ifdef DEBUG
						std::cerr << "\tMERGE SUCCESSFUL" << std::endl;
#endif
						smooth_result[i].merged = true;
					} else {
#ifdef DEBUG
						std::cerr << "\tMERGE FAIL" << std::endl;
#endif
						break;
					}
				}
			}
#ifdef DEBUG
			std::cerr << "Forward merging done." << std::endl;
#endif
			// Backward merging
			for (unsigned int i = ite->id; i > 0; --i) {
#ifdef DEBUG
				std::cerr << "\t[Merging Target] " << smooth_result[i - 1].hmm_stats.chr << "\t" << smooth_result[i - 1].hmm_stats.pos << "\t" << smooth_result[i - 1].hmm_stats.length << "\t" << smooth_result[i - 1].hmm_stats.stats << std::endl;
#endif
				if (smooth_result[i - 1].merged) break;
				if (smooth_result[i - 1].hmm_stats.stats == 3) continue;
				//const bool consistant_type = ite->hmm_stats.stats == smooth_result[i - 1].hmm_stats.stats;
				//const bool consistant_type = ((ite->hmm_stats.stats == 1 || ite->hmm_stats.stats == 2) 
				//					&& (smooth_result[i - 1].hmm_stats.stats == 1 || smooth_result[i - 1].hmm_stats.stats == 2))
				//				|| ((ite->hmm_stats.stats == 4 || ite->hmm_stats.stats == 5)
				//					&& (smooth_result[i - 1].hmm_stats.stats == 4 || smooth_result[i - 1].hmm_stats.stats == 5));
				const bool consistant_type = (ite->hmm_stats.stats == smooth_result[i - 1].hmm_stats.stats)
								|| ((ite->hmm_stats.stats == 4 || ite->hmm_stats.stats == 5)
									&& (smooth_result[i - 1].hmm_stats.stats == 4 || smooth_result[i - 1].hmm_stats.stats == 5));
				if (!consistant_type) { // Different stats
					break;
				} else {
					if (CheckMerge(ite->hmm_stats, smooth_result[i - 1].hmm_stats)) {
#ifdef DEBUG
						std::cerr << "\tMERGE SUCCESSFUL" << std::endl;
#endif
						smooth_result[i - 1].merged = true;
					} else {
#ifdef DEBUG
						std::cerr << "\tMERGE FAIL" << std::endl;
#endif
						break;
					}
				}
			}
		}
	}
}

void MergeStats (std::vector <SHmmStats> & result, std::vector <SHmmStatsHeap> & smooth_result) {
	//std::vector <SHmmStatsHeap> smooth_result;
	unsigned int vector_id = 0, out_stats_length = 0, total_out_stats_length = 0, out_stats_count = 0;
	SHmmStatsHeap tmp_heap(result.front(), vector_id);
	smooth_result.push_back(tmp_heap);
	for (std::vector <SHmmStats>::const_iterator ite = std::next(result.begin()); ite != result.end(); ++ite) {
		if (ite->length < 5000 || ite->stats == smooth_result.back().hmm_stats.stats) {
			smooth_result.back().hmm_stats.length += ite->length;
			if ((smooth_result.back().hmm_stats.stats == 3)
			// If the stats of smooth_result.back().hmm_stats.stats is 3, we absorb the new stats anyway.
			// We prefer normal stage.
				|| (ite->stats == smooth_result.back().hmm_stats.stats && ite->length / static_cast<double>(out_stats_length) > 0.3)) {
				// If the stats of smooth_result.back().hmm_stats.stats is NOT 3, we absorb when length of the current stats is larger than 30% of out_stats_length.
				out_stats_length = 0;
				out_stats_count = 0;
			} else {
				// Two stats are different.
				total_out_stats_length += ite->length;
				out_stats_length += ite->length;
				++out_stats_count;
			}
			// Too many total_out_stats_length so far, we stop extend smooth_result.back().
			if (total_out_stats_length / static_cast<double>(smooth_result.back().hmm_stats.length) > 0.2) {
				smooth_result.back().hmm_stats.length -= out_stats_length;

				ite = std::prev(ite, out_stats_count - 1);
				tmp_heap.hmm_stats = *ite;
				tmp_heap.id = ++vector_id;
				smooth_result.push_back(tmp_heap);
				
				total_out_stats_length = 0;
				out_stats_length = 0;
				out_stats_count = 0;
			}
		} else { // !(ite->length < 5000 || ite->stats == smooth_result.back().hmm_stats.stats)
			// A new stats is created.
			tmp_heap.hmm_stats = *ite;
			tmp_heap.id = ++vector_id;
			smooth_result.push_back(tmp_heap);
			total_out_stats_length = 0;
			out_stats_length = 0;
			out_stats_count = 0;
		}
	}
}

void SmoothStats(std::vector<SHmmStats> & cnvs, const std::string & ref_name, 
			const std::vector <SReadDepth>& read_depth, const int bin_size, const unsigned int minimum_report_size, 
			const int* q, const int T) {
	if (read_depth.size() != T) {
		std::cerr << "ERROR: HMM read_depth's size does not match with the number of stats." << std::endl;
		return;
	}

	std::vector <SHmmStats> result;
	result.reserve(T);
	std::vector <SReadDepth>::const_iterator rd_ite = read_depth.begin();
	// Ccollapse stats.
#ifdef DEBUG
	std::cerr << "POS\tn_count\tlow_mq_alignments\tOri_stats" << std::endl;
#endif
	for (int i = 1; i <= T; ++i, ++rd_ite) {
		// If there are >50% N's in the region, the region won't be taken in account so we set the stats to NORMAL.
#ifdef DEBUG
		std::cerr << rd_ite->pos << "\t" << rd_ite->n_count << "\t" << rd_ite->low_mq_alignments << "\t" << q[i] << std::endl;
#endif
		int cur_stat = (rd_ite->n_count * 2) > bin_size ? 3 : q[i];
		// If the cur_stat is 1 (homo DEL), we don't estimate low_mq_alignments due to we don't expect enough reads to do so.
		// If there are >50% low qual alignments in the region, the region won't be taken in account so we set the stats to NORMAL.
		if (cur_stat != 1) cur_stat = rd_ite->low_mq_alignments > 0.5 ? 3 : cur_stat;
		
		if (result.empty() || cur_stat != result.back().stats) { // Create the init hmm_stats.
			SHmmStats tmp(rd_ite->pos, cur_stat, 0);
			result.push_back(tmp);
		}
		result.back().length += bin_size;
	}

#ifdef DEBUG
	std::cerr << "HMM before smoothing" << std::endl;
	for (std::vector <SHmmStats>::const_iterator ite = result.begin(); ite != result.end(); ++ite) {
		std::cerr << ite->pos << "\t" << ite->stats << "\t" << ite->length << std::endl;
	}
#endif
	std::vector <SHmmStatsHeap> smooth_result;
	MergeStats(result, smooth_result);
	result.clear(); // Save some memory

#ifdef DEBUG
	std::cerr << "HMM after smoothing" << std::endl;
	for (std::vector <SHmmStatsHeap>::const_iterator ite = smooth_result.begin(); ite != smooth_result.end(); ++ite) {
		std::cerr << ite->hmm_stats.pos << "\t" << ite->hmm_stats.stats << "\t" << ite->hmm_stats.length << std::endl;
	}
#endif

	// Merge segments from the largest one
	std::vector <SHmmStatsHeap> heap = smooth_result;
	ConsolidateStats(smooth_result, heap);

#ifdef DEBUG
	std::sort(heap.begin(), heap.end(), SortByCoordinateHeap);
	std::cerr << "HMM after consolidating" << std::endl;
	for (std::vector <SHmmStatsHeap>::const_iterator ite = heap.begin(); ite != heap.end(); ++ite) {
		std::cerr << ite->hmm_stats.pos << "\t" << ite->hmm_stats.stats << "\t" << ite->hmm_stats.length << "\t" 
			<< (smooth_result[ite->id].merged ? "MERGED: T" : "MERGED: F") << std::endl;
	}
#endif

	// Dump the final results	
	for (std::vector <SHmmStatsHeap>::const_iterator ite = heap.begin(); ite != heap.end(); ++ite) {
		if (!smooth_result[ite->id].merged && ite->hmm_stats.stats != 3 && ite->hmm_stats.length > minimum_report_size) {
			cnvs.push_back(ite->hmm_stats);
			cnvs.back().chr = ref_name;
		}
		std::sort(cnvs.begin(), cnvs.end(), SortByCoordinate);
	}
	
}
} // namespace

namespace CallHmm { 
bool HmmAndViterbi (std::vector<SHmmStats> & cnvs, const std::string & ref_name, const std::vector <SReadDepth> & read_depth, 
	const int bin_size, const unsigned int minimum_report_size, const int coverage) {
	if (read_depth.empty()) return false;

	// Init HMM
	int T = read_depth.size();
	int* O = new int [T + 1]; // observation sequence O[1..T]
	memset(O, 0, sizeof(O));
	for (std::vector <SReadDepth>::const_iterator ite = read_depth.begin(); ite != read_depth.end(); ++ite) {
		//const int rd_diff = (ite->count - coverage) / static_cast<double>(ite->kmer_score);
		//int tmp_o = round((coverage + rd_diff) / coverage * 50);
		//if (tmp_o < 0) tmp_o = 0;
		const int tmp_o = round(ite->count / static_cast<float>(coverage) * 50);
		O[std::distance(read_depth.begin(), ite) + 1] = std::min(tmp_o, 180);
	}

	HMM hmm;
	hmm.N  = hmm_N;
	hmm.M  = hmm_M;
	hmm.A  = new double* [hmm.N + 1];
	hmm.B  = new double* [hmm.N + 1];
	hmm.pi = new double [hmm.N + 1];

	for (int i = 1; i <= hmm.N; ++i) {
		hmm.A[i] = new double [hmm.N + 1];
		hmm.B[i] = new double [hmm.M + 1];
		std::memcpy(hmm.A[i] + 1, hmm_A[i - 1], sizeof(double)* hmm.N);
		std::memcpy(hmm.B[i] + 1, hmm_B[i - 1], sizeof(double) * hmm.M);
	}
	std::memcpy(hmm.pi + 1, hmm_pi, sizeof(double) * hmm.N);

	int* q = new int [T + 1]; // resultant states
	memset(q, 0, sizeof(q));
	int** psi = new int* [T + 1];
	double **delta = new double* [T + 1];
	for (int i = 1; i <= T; ++i) {
		psi[i] = new int [hmm.N + 1];
		memset(psi[i], 0, sizeof(psi[i]));
		delta[i] = new double [hmm.N + 1];
		memset(delta[i], 0.0, sizeof(delta[i]));
	}
	double logproba = 0.0;

	// End of Init HMM
	
//#ifdef DEBUG
//PrintHmm(hmm, T, O);
//#endif
	
	ViterbiLog(&hmm, T, O, delta, psi, q, &logproba);
	SmoothStats(cnvs, ref_name, read_depth, bin_size, minimum_report_size,q, T);

	// Clean up
	for (int i = 1; i <= hmm.N; ++i) {
		delete hmm.A[i];
		delete hmm.B[i];
	}
	
	for (int i = 1; i <= T; ++i) {
		delete psi[i];
		delete delta[i];
	}
	
	delete O;
	delete hmm.A;
	delete hmm.B;
	delete hmm.pi;
	delete q;
	delete psi;
	delete delta;
	return true;
}
} // namespace CallHmm
