/*******************************************************************************
 Copyright 2006-2012 Lukas Käll <lukas.kall@scilifelab.se>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 *******************************************************************************/

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/assign.hpp>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
using namespace boost::algorithm;
#include "DataSet.h"
#include "Globals.h"
#include "MassHandler.h"
#include "Normalizer.h"
#include "PosteriorEstimator.h"
#include "Scores.h"
#include "SetHandler.h"
#include "ssl.h"

inline bool operator>(const ScoreHolder& one, const ScoreHolder& other) {
    return (one.score > other.score) || (one.score == other.score && one.pPSM->scan > other.pPSM->scan) || (one.score == other.score && one.pPSM->scan == other.pPSM->scan && one.pPSM->expMass > other.pPSM->expMass) || (one.score == other.score && one.pPSM->scan == other.pPSM->scan && one.pPSM->expMass == other.pPSM->expMass && one.label > other.label);
}

inline bool operator<(const ScoreHolder& one, const ScoreHolder& other) {
    return (one.score < other.score) || (one.score == other.score && one.pPSM->scan < other.pPSM->scan) || (one.score == other.score && one.pPSM->scan == other.pPSM->scan && one.pPSM->expMass < other.pPSM->expMass) || (one.score == other.score && one.pPSM->scan == other.pPSM->scan && one.pPSM->expMass == other.pPSM->expMass && one.label < other.label);
}

inline double truncateTo(double truncateMe, const char* length) {
    char truncated[64];
    char format[64];
    strncpy(format, "%.", 3);
    strncat(format, length, 64);
    strncat(format, "lf\n", 4);
    snprintf(truncated, sizeof(truncated), format, truncateMe);
    return atof(truncated);
}

void ScoreHolder::printPSM(ostream& os, bool printDecoys, bool printExpMass) {
    if (!isDecoy() || printDecoys) {
        os << "    <psm p:psm_id=\"" << pPSM->getId() << "\"";
        if (printDecoys) {
            if (isDecoy())
                os << " p:decoy=\"true\"";
            else
                os << " p:decoy=\"false\"";
        }
        os << ">" << endl;
        os << "      <svm_score>" << fixed << score << "</svm_score>" << endl;
        os << "      <q_value>" << scientific << q << "</q_value>" << endl;
        os << "      <pep>" << scientific << pep << "</pep>" << endl;

        if (printExpMass) {
            os << "      <exp_mass>" << fixed << setprecision(4) << pPSM->expMass << "</exp_mass>" << endl;
        }

        os << "      <calc_mass>" << fixed << setprecision(3) << pPSM->calcMass << "</calc_mass>" << endl;
        /* Remove this tag for now */
        /* if (isfinite(pPSM->getRetentionTime())) {
              os << "      <retentionTime>" << fixed << setprecision (3) << pPSM->getRetentionTime() << "</retentionTime>" << endl;
            } */

        if (pPSM->getPeptideSequence().size() > 0) {
            string n = pPSM->getFlankN();
            string c = pPSM->getFlankC();
            string centpep = pPSM->getPeptideSequence();
            os << "      <peptide_seq n=\"" << n << "\" c=\"" << c << "\" seq=\"" << centpep << "\"/>" << endl;
        }

        std::vector<std::string>::const_iterator pidIt = pPSM->proteinIds.begin();
        for (; pidIt != pPSM->proteinIds.end(); ++pidIt) {
            os << "      <protein_id>" << getRidOfUnprintablesAndUnicode(*pidIt) << "</protein_id>" << endl;
        }

        os << "      <p_value>" << scientific << p << "</p_value>" << endl;
        os << "    </psm>" << endl;
    }
}

void ScoreHolder::printPeptide(ostream& os, bool printDecoys, bool printExpMass, Scores& fullset) {
    if (!isDecoy() || printDecoys) {
        os << "    <peptide p:peptide_id=\"" << pPSM->getPeptideSequence() << "\"";
        if (printDecoys) {
            if (isDecoy())
                os << " p:decoy=\"true\"";
            else
                os << " p:decoy=\"false\"";
        }
        os << ">" << endl;

        os << "      <svm_score>" << fixed << score << "</svm_score>" << endl;
        os << "      <q_value>" << scientific << q << "</q_value>" << endl;
        os << "      <pep>" << scientific << pep << "</pep>" << endl;

        if (printExpMass) {
            os << "      <exp_mass>" << fixed << setprecision(4) << pPSM->expMass << "</exp_mass>" << endl;
        }
        os << "      <calc_mass>" << fixed << setprecision(3) << pPSM->calcMass << "</calc_mass>" << endl;

        std::vector<std::string>::const_iterator pidIt = pPSM->proteinIds.begin();
        for (; pidIt != pPSM->proteinIds.end(); ++pidIt) {
            os << "      <protein_id>" << getRidOfUnprintablesAndUnicode(*pidIt) << "</protein_id>" << endl;
        }

        os << "      <p_value>" << scientific << p << "</p_value>" << endl;
        os << "      <psm_ids>" << endl;

        // output all psms that contain the peptide
        std::vector<PSMDescription*>::const_iterator psmIt = fullset.getPsms(pPSM).begin();
        for (; psmIt != fullset.getPsms(pPSM).end(); ++psmIt) {
            os << "        <psm_id>" << (*psmIt)->getId() << "</psm_id>" << endl;
        }
        os << "      </psm_ids>" << endl;
        os << "    </peptide>" << endl;
    }
}

std::string ScoreHolder::getCharge(std::string id) {
    /* Aposymb_Proteome_DIA_RAW_A01_Q1.00148.00148.1_2 -> 1_2*/
    std::string last_element(id.substr(id.rfind("_") - 1));
    /* 1_2 -> 1 */
    return last_element.substr(0, last_element.find("_"));
}

void ScoreHolder::printPepXML(ostream& os, map<char, float>& aaWeight, int index) {
    /* std::cerr << pepXMLBaseName << std::endl; */
    std::string id = pPSM->getId();
    /* Get scan ids */
    unsigned int scan = pPSM->scan;
    unsigned int native_id = scan - 1;
    /* Get charge */
    std::string assumed_charge = getCharge(id);
    /* Get RT */
    double RT = pPSM->getRetentionTime();
    /*  uncalibrated_precursor_neutral_mass ? */
    double expMass = pPSM->expMass;
    /*  precursor_neutral_mass ? */
    double calcMass = pPSM->calcMass;

    os << "        <spectrum_query spectrum=\"" << id << "\" precursor_neutral_mass=\"" << expMass << "\" assumed_charge=\"" << assumed_charge << "\" end_scan=\"" << scan << "\" index=\"" << index << "\" retention_time_sec=\"" << fixed << setprecision(3) << pPSM->getRetentionTime() << "\" start_scan=\"" << scan << "\">" << endl;
    std::string centpep = pPSM->getPeptideSequence();
    std::string trimmed_pep = trim_left_copy_if(centpep, is_any_of("n"));
    regex r("\\[(.*?)\\]");
    std::string peptide_sequence = regex_replace(trimmed_pep, r, "");
    os << "            <search_result>" << endl;

    /* Print protein information */
    size_t n_protein = 0;
    /* Placeholders */
    int hit_rank = 1;
    int massdiff = 1;
    /* num_tot_proteins */
    int num_tot_proteins = pPSM->proteinIds.size();

    std::vector<std::string>::const_iterator pidIt = pPSM->proteinIds.begin();
    for (; pidIt != pPSM->proteinIds.end(); ++pidIt) {
        if (n_protein == 0) {
            /*  set calc_neutral_pep_mass  as calcMass as placeholder for now */
            os << "                <search_hit calc_neutral_pep_mass=\"" << calcMass << "\" num_tot_proteins=\"" << num_tot_proteins << "\" hit_rank=\"" << hit_rank << "\" massdiff=\"" << massdiff << "\" peptide=\"" << peptide_sequence << "\" protein=\"" << getRidOfUnprintablesAndUnicode(*pidIt) << "\">" << endl;
        } else {
            os << "                    <alternative_protein protein=\"" << getRidOfUnprintablesAndUnicode(*pidIt) << "\"/>" << endl;
        }
        n_protein++;
    }
    /* Print modification */
    string subject(trimmed_pep);
    smatch match;
    long int mod_pos = 0;
    float mod_weight;
    size_t n_mod = 0;

    while (regex_search(subject, match, r)) {
        mod_pos += match.position(0);
        // suffix to find the rest of the string.
        if (mod_pos == 0) {
            mod_weight = std::stof(match.str(1)) + 1.0074;
            if (n_mod == 0) {
                os << "                    <modification_info mod_nterm_mass=\"" << round(mod_weight * 1000) / 1000 << "\">" << endl;
            }
        } else {
            mod_weight = aaWeight[peptide_sequence.at(mod_pos - 1)] + std::stof(match.str(1));
            if (n_mod == 0) {
                os << "                    <modification_info>" << endl;
            }
            os << "                        <mod_aminoacid_mass mass=\"" << round(mod_weight * 1000) / 1000 << "\" position=\"" << std::to_string(mod_pos) << "\" />" << endl;
        }
        subject = match.suffix().str();
        n_mod++;
    }
    if (n_mod != 0) {
        os << "                    </modification_info>" << endl;
    }

    /* Print Percolator information */
    os << "                    <analysis_result analysis=\"peptideprophet\">" << endl;
    os << "                        <peptideprophet_result probability=\"" << scientific << 1.0 - pep << "\" />" << endl;
    os << "                    </analysis_result>" << endl;
    os << "                </search_hit>" << endl;
    os << "            </search_result>" << endl;
    os << "        </spectrum_query>" << endl;
}

void Scores::merge(std::vector<Scores>& sv, double fdr, bool skipNormalizeScores, std::vector<std::vector<double> >& all_w) {
    scores_.clear();
    std::vector<Scores>::iterator cvBinScores = sv.begin();
    std::vector<std::vector<double> >::iterator weights = all_w.begin();
    for (; cvBinScores != sv.end(); cvBinScores++, weights++) {
        sort(cvBinScores->begin(), cvBinScores->end(), greater<ScoreHolder>());
        cvBinScores->checkSeparationAndSetPi0();
        cvBinScores->calcQ(fdr);
        if (!skipNormalizeScores) {
            cvBinScores->normalizeScores(fdr, *weights);
        }
        copy(cvBinScores->begin(), cvBinScores->end(), back_inserter(scores_));
    }
    postMergeStep();
}

void Scores::postMergeStep() {
    sort(scores_.begin(), scores_.end(), greater<ScoreHolder>());
    totalNumberOfDecoys_ = static_cast<unsigned int>(count_if(scores_.begin(),
                                                              scores_.end(),
                                                              mem_fn(&ScoreHolder::isDecoy)));
    totalNumberOfTargets_ = static_cast<unsigned int>(count_if(scores_.begin(),
                                                               scores_.end(),
                                                               mem_fn(&ScoreHolder::isTarget)));
    targetDecoySizeRatio_ = totalNumberOfTargets_ / max(1.0, (double)totalNumberOfDecoys_);
    checkSeparationAndSetPi0();
}

double Scores::calcScore(const double* feat, const std::vector<double>& w) const {
    std::size_t ix = FeatureNames::getNumFeatures();
    double score = w[ix];
    for (; ix--;) {
        score += feat[ix] * w[ix];
    }
    return score;
}

void Scores::scoreAndAddPSM(ScoreHolder& sh,
                            const std::vector<double>& rawWeights, FeatureMemoryPool& featurePool) {
    const unsigned int numFeatures = static_cast<unsigned int>(FeatureNames::getNumFeatures());

    for (unsigned int j = 0; j < numFeatures; j++) {
        sh.score += sh.pPSM->features[j] * rawWeights[j];
    }
    sh.score += rawWeights[numFeatures];

    featurePool.deallocate(sh.pPSM->features);
    sh.pPSM->deleteRetentionFeatures();

    if (sh.label == 1) {
        ++totalNumberOfTargets_;
    } else if (sh.label == -1) {
        ++totalNumberOfDecoys_;
    }

    if (sh.label != 1 && sh.label != -1) {
        std::cerr << "Warning: the PSM " << sh.pPSM->getId()
                  << " has a label not in {1,-1} and will be ignored." << std::endl;
        PSMDescription::deletePtr(sh.pPSM);
    } else {
        scores_.push_back(sh);
    }
}

int Scores::calcBalancedFDR(double treshold) {
    double c_decoy(0.5), c_target(0.0), factor( nullTargetWinProb_ / ( 1.0 - nullTargetWinProb_ ) );
    for_each(scores_.begin(), scores_.end(), [&](ScoreHolder& score) {
        if (score.isDecoy()) {
            c_decoy += 1.0;
        } else {
            c_target += 1.0;
        }
        score.q = ( c_target / c_decoy ) * factor;
    });
    reverse(scores_.begin(), scores_.end());
    double previous_q = 1.0;
    for_each(scores_.begin(), scores_.end(), [&](ScoreHolder& score) {
        previous_q = std::min(score.q, previous_q);
        score.q = previous_q;
    });
    reverse(scores_.begin(), scores_.end());

    // Calcultaing the number of target PSMs with q-value less than treshold
    ScoreHolder limit(treshold, 1);
    auto upper = lower_bound(scores_.begin(), scores_.end(), limit);
    return upper - scores_.begin();
}


void Scores::print(int label, std::ostream& os) {
    std::vector<ScoreHolder>::iterator scoreIt = scores_.begin();
    os << "PSMId\t";
    if (PSMDescription::hasSpectrumFileName()) {
        os << "filename\t";
    }
    os << "score\tq-value\tposterior_error_prob\tpeptide\tproteinIds";
    if (is_output_rt_) {
        os << "\trt\n";
    } else {
        os << "\n";
    }

    for (; scoreIt != scores_.end(); ++scoreIt) {
        if (scoreIt->label == label) {
            std::ostringstream out;
            scoreIt->pPSM->printProteins(out);
            ResultHolder rh(scoreIt->score, scoreIt->q, scoreIt->pep, scoreIt->pPSM->getId(), scoreIt->pPSM->getFullPeptide(), out.str(), scoreIt->pPSM->getSpectrumFileName());
            if (is_output_rt_) {
                rh.retentionTime = scoreIt->pPSM->getRetentionTime();
                rh.outputRT = true;  // Ideally this should be set once outside this loop, but ResultHolder needs to be initialized before.
            }

            os << rh << std::endl;
        }
    }
}

void Scores::populateWithPSMs(SetHandler& setHandler) {
    scores_.clear();
    setHandler.populateScoresWithPSMs(scores_, 1);
    setHandler.populateScoresWithPSMs(scores_, -1);
    totalNumberOfTargets_ = static_cast<unsigned int>(setHandler.getSizeFromLabel(1));
    totalNumberOfDecoys_ = static_cast<unsigned int>(setHandler.getSizeFromLabel(-1));
    targetDecoySizeRatio_ = (double)totalNumberOfTargets_ / totalNumberOfDecoys_;

    if (VERB > 1) {
        cerr << "Train/test set contains " << totalNumberOfTargets_
             << " positives and " << totalNumberOfDecoys_
             << " negatives, size ratio=" << targetDecoySizeRatio_
             << " and pi0=" << pi0_ << endl;
    }

    if (totalNumberOfTargets_ == 0) {
        ostringstream oss;
        oss << "Error: no target PSMs were provided.\n";
        if (NO_TERMINATE) {
            cerr << oss.str() << "No-terminate flag set: ignoring error." << std::endl;
        } else {
            throw MyException(oss.str());
        }
    }

    if (totalNumberOfDecoys_ == 0) {
        ostringstream oss;
        oss << "Error: no decoy PSMs were provided.\n";
        if (NO_TERMINATE) {
            cerr << oss.str() << "No-terminate flag set: ignoring error." << std::endl;
        } else {
            throw MyException(oss.str());
        }
    }

    // check for the minimum recommended number of positive and negative hits
    if (totalNumberOfTargets_ <= (unsigned)(FeatureNames::getNumFeatures() * 5)) {
        std::cerr << "Warning : the number of positive samples read is too small to perform a correct classification.\n"
                  << std::endl;
    }
    if (totalNumberOfDecoys_ <= (unsigned)(FeatureNames::getNumFeatures() * 5)) {
        std::cerr << "Warning : the number of negative samples read is too small to perform a correct classification.\n"
                  << std::endl;
    }
}

/**
 * Divides the PSMs from pin file into xval_fold cross-validation sets based on
 * their spectrum scan number
 * @param train vector containing the training sets of PSMs
 * @param test vector containing the test sets of PSMs
 * @param xval_fold number of folds in train and test
 */
void Scores::createXvalSetsBySpectrum(std::vector<Scores>& train,
                                      std::vector<Scores>& test, const unsigned int xval_fold,
                                      FeatureMemoryPool& featurePool) {
    // set the number of cross validation folds for train and test to xval_fold
    train.resize(xval_fold, Scores(usePi0_));
    test.resize(xval_fold, Scores(usePi0_));
    // remain keeps track of residual space available in each fold
    std::vector<int> remain(xval_fold);
    // set values for remain: initially each fold is assigned (tot number of
    // scores_ / tot number of folds)
    int fold = static_cast<int>(xval_fold), ix = static_cast<int>(scores_.size());
    while (fold--) {
        remain[static_cast<std::size_t>(fold)] = ix / (fold + 1);
        ix -= remain[static_cast<std::size_t>(fold)];
    }

    std::sort(scores_.begin(), scores_.end(), OrderScanHash());

    if (scores_.size() == 0) {
        ostringstream oss;
        oss << "Error: no scored PSMs were provided.\n";
        if (NO_TERMINATE) {
            cerr << oss.str() << "No-terminate flag set: ignoring error." << std::endl;
            return;
        } else {
            throw MyException(oss.str());
        }
    }

    // put scores into the folds; choose a fold (at random) and change it only
    // when scores from a new spectra are encountered
    unsigned int previousSpectrum = scores_.begin()->pPSM->scan;
    size_t randIndex = PseudoRandom::lcg_rand() % xval_fold;
    for (std::vector<ScoreHolder>::iterator it = scores_.begin();
         it != scores_.end(); ++it) {
        const unsigned int curScan = (*it).pPSM->scan;
        const ScoreHolder sh = (*it);
        // if current score is from a different spectra than the one encountered in
        // the previous iteration, choose new fold

        if (previousSpectrum != curScan) {
            randIndex = PseudoRandom::lcg_rand() % xval_fold;
            // allow only indexes of folds that are non-full
            while (remain[randIndex] <= 0) {
                randIndex = PseudoRandom::lcg_rand() % xval_fold;
            }
        }
        // insert
        for (unsigned int i = 0; i < xval_fold; ++i) {
            if (i == randIndex) {
                test[i].addScoreHolder(sh);
            } else {
                train[i].addScoreHolder(sh);
            }
        }
        // update number of free position for used fold
        --remain[randIndex];
        // set previous spectrum to current one for next iteration
        previousSpectrum = curScan;
    }

    // calculate ratios of target over decoy for train and test set
    for (unsigned int i = 0; i < xval_fold; ++i) {
        train[i].recalculateSizes();
        test[i].recalculateSizes();
    }

    if (featurePool.isInitialized()) {
        boost::unordered_map<double*, double*> movedAddresses;
        size_t idx = 0;
        for (unsigned int i = 0; i < xval_fold; ++i) {
            bool isTarget = true;
            test[i].reorderFeatureRows(featurePool, isTarget, movedAddresses, idx);
            isTarget = false;
            test[i].reorderFeatureRows(featurePool, isTarget, movedAddresses, idx);
        }
    }
}

void Scores::recalculateSizes() {
    totalNumberOfTargets_ = 0;
    totalNumberOfDecoys_ = 0;
    std::vector<ScoreHolder>::const_iterator scoreIt = scores_.begin();
    for (; scoreIt != scores_.end(); ++scoreIt) {
        if (scoreIt->isTarget()) {
            ++totalNumberOfTargets_;
        } else {
            ++totalNumberOfDecoys_;
        }
    }
    targetDecoySizeRatio_ = totalNumberOfTargets_ / (double)totalNumberOfDecoys_;
}

void Scores::reorderFeatureRows(FeatureMemoryPool& featurePool,
                                bool isTarget, boost::unordered_map<double*, double*>& movedAddresses, size_t& idx) {
    size_t numFeatures = FeatureNames::getNumFeatures();
    std::vector<ScoreHolder>::const_iterator scoreIt = scores_.begin();
    for (; scoreIt != scores_.end(); ++scoreIt) {
        if (scoreIt->isTarget() == isTarget) {
            double* newAddress = featurePool.addressFromIdx(static_cast<unsigned int>(idx++));
            double* oldAddress = scoreIt->pPSM->features;
            while (movedAddresses.find(oldAddress) != movedAddresses.end()) {
                oldAddress = movedAddresses[oldAddress];
            }
            std::swap_ranges(oldAddress, oldAddress + numFeatures, newAddress);
            scoreIt->pPSM->features = newAddress;
            if (oldAddress != newAddress) {
                movedAddresses[newAddress] = oldAddress;
            }
        }
    }
}

/**
 * Linear rescaling of SVM scores such that q=fdr = 0 and the median decoy = -1.
 * @param fdr fdr-cutoff to use, typically --trainFDR
 * @param weights SVM weights used to compute SVM scores
 */
void Scores::normalizeScores(double fdr, std::vector<double>& weights) {
    unsigned int medianIndex = std::max(0u, totalNumberOfDecoys_ / 2u), decoys = 0u;

    if (scores_.size() == 0) {
        ostringstream oss;
        oss << "Error: no scored PSMs were provided.\n";
        if (NO_TERMINATE) {
            cerr << oss.str() << "No-terminate flag set: ignoring error." << std::endl;
            return;
        } else {
            throw MyException(oss.str());
        }
    }

    std::vector<ScoreHolder>::iterator it = scores_.begin();
    double fdrScore = it->score;
    double medianDecoyScore = fdrScore + 1.0;

    for (; it != scores_.end(); ++it) {
        if (it->q < fdr)
            fdrScore = it->score;
        if (it->isDecoy()) {
            if (++decoys == medianIndex) {
                medianDecoyScore = it->score;
                break;
            }
        }
    }

    double diff = fdrScore - medianDecoyScore;
    if (diff <= 0) {
        ostringstream oss;
        oss << "Error: median decoy score <= score at " << fdr * 100
            << "\% FDR. Cannot rescale scores to merge cross validation bins, try lowering --trainFDR.\n";
        if (NO_TERMINATE) {
            cerr << oss.str() << "No-terminate flag set: apply offset such that median "
                 << "decoy has score -1, but skipping rescaling." << std::endl;
            diff = 1.0;
            fdrScore += 1.0;
        } else {
            throw MyException(oss.str());
        }
    }

    std::vector<ScoreHolder>::iterator scoreIt = scores_.begin();
    for (; scoreIt != scores_.end(); ++scoreIt) {
        scoreIt->score -= fdrScore;
        scoreIt->score /= diff;
    }
    Normalizer::endScoreNormalizeWeights(weights, weights, fdrScore, diff);
}

/**
 * Calculates the SVM cost/score of each PSM and sorts them
 * and calculate q values for each PSM
 * @param w normal vector used for SVM cost
 * @param fdr FDR threshold specified by user (default 0.01)
 * @return number of true positives
 */
int Scores::calcScores(std::vector<double>& w, double fdr, bool skipDecoysPlusOne) {
    onlyCalcScores(w);
    return calcQ(fdr, skipDecoysPlusOne);
}

/**
 * Calculates the SVM cost/score of each PSM and sorts them
 * @param w normal vector used for SVM cost
 * @param fdr FDR threshold specified by user (default 0.01)
 * @return 0 on successfull execution
 */
int Scores::onlyCalcScores(std::vector<double>& w) {
    std::size_t ix;
    std::vector<ScoreHolder>::iterator scoreIt = scores_.begin();
    for (; scoreIt != scores_.end(); ++scoreIt) {
        scoreIt->score = calcScore(scoreIt->pPSM->features, w);
    }
    sort(scores_.begin(), scores_.end(), greater<ScoreHolder>());
    if (VERB > 3) {
        if (scores_.size() >= 10) {
            cerr << "10 best scores and labels" << endl;
            for (ix = 0; ix < 10; ix++) {
                cerr << scores_[ix].score << " " << scores_[ix].label << endl;
            }
            cerr << "10 worst scores and labels" << endl;
            for (ix = scores_.size() - 10; ix < scores_.size(); ix++) {
                cerr << scores_[ix].score << " " << scores_[ix].label << endl;
            }
        } else {
            cerr << "Too few scores to display top and bottom PSMs (" << scores_.size() << " scores found)." << endl;
        }
    }
    return 0;
}


void Scores::getScoreLabelPairs(std::vector<pair<double, bool> >& combined) {
    combined.clear();
    transform(scores_.begin(), scores_.end(), back_inserter(combined),
              mem_fn(&ScoreHolder::toPair));
}

/**
 * Calculates the q-value for each psm in scores_: the q-value is the minimal
 * FDR of any set that includes the particular psm
 * @param fdr FDR threshold specified by user (default 0.01)
 * @return number of true positives
 */
int Scores::calcQ(double fdr, bool skipDecoysPlusOne) {
    assert(totalNumberOfDecoys_ + totalNumberOfTargets_ == size());

    std::vector<pair<double, bool> > combined;
    getScoreLabelPairs(combined);

    std::vector<double> qvals;
    PosteriorEstimator::setNegative(true);  // also get q-values for decoys
    PosteriorEstimator::getQValues(pi0_, combined, qvals, skipDecoysPlusOne);

    // set q-values and count number of positives
    std::vector<double>::const_iterator qIt = qvals.begin();
    std::vector<ScoreHolder>::iterator scoreIt = scores_.begin();

    int numPos = 0;
    for (; qIt != qvals.end(); ++qIt, ++scoreIt) {
        scoreIt->q = *qIt;
        if (scoreIt->q < fdr && scoreIt->isTarget()) ++numPos;
    }

    return numPos;
}

void Scores::generateNegativeTrainingSet(AlgIn& data, const double cneg) {
    std::size_t ix2 = 0;
    std::vector<ScoreHolder>::const_iterator scoreIt = scores_.begin();
    for (; scoreIt != scores_.end(); ++scoreIt) {
        if (scoreIt->isDecoy()) {
            data.vals[ix2] = scoreIt->pPSM->features;
            data.Y[ix2] = -1;
            // data.C[ix2] = cneg;
            ix2++;
        }
    }
    data.negatives = static_cast<int>(ix2);
}

void Scores::generatePositiveTrainingSet(AlgIn& data, const double fdr,
                                         const double cpos, const bool trainBestPositive) {
    std::size_t ix2 = static_cast<std::size_t>(data.negatives);
    int p = 0;

    std::vector<ScoreHolder>::iterator lastUniqueIt = scores_.end();
    if (trainBestPositive) {
        std::sort(scores_.begin(), scores_.end(), OrderScanLabel());
        lastUniqueIt = std::unique(scores_.begin(), scores_.end(), UniqueScanLabel());
        std::sort(scores_.begin(), lastUniqueIt, greater<ScoreHolder>());
    }

    std::vector<ScoreHolder>::const_iterator scoreIt = scores_.begin();
    for (; scoreIt != lastUniqueIt; ++scoreIt) {
        if (scoreIt->isTarget()) {
            if (scoreIt->q <= fdr) {
                data.vals[ix2] = scoreIt->pPSM->features;
                data.Y[ix2] = 1;
                // data.C[ix2] = cpos;
                ix2++;
                ++p;
            }
        }
    }
    data.positives = p;
    data.m = static_cast<int>(ix2);
}

void Scores::weedOutRedundant() {
    std::map<std::string, unsigned int> peptideSpecCounts;
    double specCountQvalThreshold = -1.0;
    weedOutRedundant(peptideSpecCounts, specCountQvalThreshold);
}

/**
 * Routine that sees to that only unique peptides are kept (used for analysis
 * on peptide-fdr rather than psm-fdr)
 */
void Scores::weedOutRedundant(std::map<std::string, unsigned int>& peptideSpecCounts, double specCountQvalThreshold) {
    // lexicographically order the scores_ (based on peptides names,labels and scores)
    std::sort(scores_.begin(), scores_.end(), lexicOrderProb());

    /*
     * much simpler version but it does not fill up the peptide-PSM map:
     * scores_.erase(std::unique(scores_.begin(), scores_.end(), mycmp), scores_.end());
     */

    std::string previousPeptide = "";
    int previousLabel = 0;
    size_t lastWrittenIdx = 0u;
    for (size_t idx = 0u; idx < scores_.size(); ++idx) {
        std::string currentPeptide = scores_.at(idx).pPSM->getPeptideSequence();
        int currentLabel = scores_.at(idx).label;
        if (currentPeptide != previousPeptide || currentLabel != previousLabel) {
            // insert as a new score
            scores_.at(lastWrittenIdx++) = scores_.at(idx);
            previousPeptide = currentPeptide;
            previousLabel = currentLabel;
        }
        // append the psm
        peptidePsmMap_[scores_.at(lastWrittenIdx - 1).pPSM].push_back(scores_.at(idx).pPSM);
        if (specCountQvalThreshold > 0.0 && scores_.at(idx).q < specCountQvalThreshold) {
            ++peptideSpecCounts[currentPeptide];
        }
    }
    scores_.resize(lastWrittenIdx);
    postMergeStep();
}

/**
 * Routine that sees to that only unique spectra are kept for TDC
 */
void Scores::weedOutRedundantTDC() {
    // order the scores (based on spectra id and score)
    std::sort(scores_.begin(), scores_.end(), OrderScanMassCharge());
    scores_.erase(std::unique(scores_.begin(), scores_.end(), UniqueScanMassCharge()), scores_.end());

    /* does not actually release memory because of memory fragmentation
    double previousExpMass = 0.0;
    unsigned int previousScan = 0u;
    size_t lastWrittenIdx = 0u;
    for (size_t idx = 0u; idx < scores_.size(); ++idx){
      double currentExpMass = scores_.at(idx).pPSM->expMass;
      int currentScan = scores_.at(idx).pPSM->scan;
      if (currentExpMass != previousExpMass || currentScan != previousScan) {
        // insert as a new score
        scores_.at(lastWrittenIdx++).swap(scores_.at(idx));
        previousScan = currentScan;
        previousExpMass = currentExpMass;
      } else {
        PSMDescription::deletePtr(scores_.at(idx).pPSM);
      }
    }
    scores_.resize(lastWrittenIdx);
    */
    postMergeStep();
}

/**
 * Routine that sees to that only 1 target and 1 decoy spectra are kept for
 * mix-max when using multiple hits per spectrum and separate searches
 */
void Scores::weedOutRedundantMixMax() {
    // order the scores (based on spectra id and score)
    std::sort(scores_.begin(), scores_.end(), OrderScanMassLabelCharge());
    scores_.erase(std::unique(scores_.begin(), scores_.end(), UniqueScanMassLabelCharge()), scores_.end());

    postMergeStep();
}

int Scores::getInitDirection(const double initialSelectionFdr, std::vector<double>& direction) {
    int bestPositives = -1;
    int bestFeature = -1;
    bool lowBest = false;

    // for determining the initial direction, the decoys+1 in the FDR estimates
    // is too restrictive for small datasets
    bool skipDecoysPlusOne = true;

    for (unsigned int featNo = 0; featNo < FeatureNames::getNumFeatures(); featNo++) {
        for (std::vector<ScoreHolder>::iterator scoreIt = scores_.begin();
             scoreIt != scores_.end(); ++scoreIt) {
            scoreIt->score = scoreIt->pPSM->features[featNo];
        }
        sort(scores_.begin(), scores_.end());
        // check once in forward direction (i = 0, higher scores are better) and
        // once in backward direction (i = 1, lower scores are better)
        for (int i = 0; i < 2; i++) {
            if (i == 1) {
                reverse(scores_.begin(), scores_.end());
            }
            int positives = calcQ(initialSelectionFdr, skipDecoysPlusOne);
            if (positives > bestPositives) {
                bestPositives = positives;
                bestFeature = static_cast<int>(featNo);
                lowBest = (i == 0);
            }
        }
    }
    for (std::size_t ix = FeatureNames::getNumFeatures(); ix--;) {
        direction[ix] = 0;
    }

    if (bestPositives <= 0) {
        ostringstream oss;
        oss << "Error in the input data: cannot find an initial direction with "
            << "positive training examples. "
            << "Consider setting/raising the initial training FDR threshold (--train-fdr-initial)." << std::endl;
        if (NO_TERMINATE) {
            cerr << oss.str();
            std::cerr << "No-terminate flag set: setting initial direction to the "
                      << "first feature and ignoring the error." << std::endl;
            bestFeature = 0;
        } else {
            throw MyException(oss.str() + "Terminating.\n");
        }
    }

    if (bestFeature >= 0) {
        direction[static_cast<std::size_t>(bestFeature)] = (lowBest ? -1 : 1);
    }

    if (VERB > 1) {
        cerr << "Selected feature " << bestFeature + 1
             << " as initial direction. Could separate "
             << bestPositives << " training set positives with q<" << initialSelectionFdr << " in that direction." << endl;
    }
    return bestPositives;
}

void Scores::checkSeparationAndSetPi0() {
    std::vector<pair<double, bool> > combined;
    getScoreLabelPairs(combined);

    std::vector<double> pvals;
    PosteriorEstimator::getPValues(combined, pvals);

    pi0_ = 1.0;
    bool tooGoodSeparation = PosteriorEstimator::checkSeparation(pvals);
    if (tooGoodSeparation) {
        ostringstream oss;
        oss << "Error in the input data: too good separation between target "
            << "and decoy PSMs.\n";
        if (NO_TERMINATE) {
            cerr << oss.str();
            if (usePi0_) {
                std::cerr << "No-terminate flag set: setting pi0 = 1 and ignoring error." << std::endl;
            } else {
                std::cerr << "No-terminate flag set: ignoring error." << std::endl;
            }
        } else {
            throw MyException(oss.str() + "Terminating.\n");
        }
    } else if (usePi0_) {
        pi0_ = PosteriorEstimator::estimatePi0(pvals);
    }
}

void Scores::calcPep() {
    std::vector<pair<double, bool> > combined;
    getScoreLabelPairs(combined);

    std::vector<double> peps;
    // Logistic regression on the data
    PosteriorEstimator::estimatePEP(combined, usePi0_, pi0_, peps, true);
    for (size_t ix = 0; ix < scores_.size(); ix++) {
        scores_[ix].pep = peps[ix];
    }
}

unsigned Scores::getQvaluesBelowLevel(double level) {
    unsigned hits = 0;
    std::vector<ScoreHolder>::const_iterator scoreIt = scores_.begin();
    for (; scoreIt != scores_.end(); ++scoreIt) {
        if (scoreIt->isTarget() && scoreIt->q < level) {
            hits++;
        }
    }
    return hits;
}

void Scores::setUsePi0(bool usePi0) {
    usePi0_ = usePi0;
}
