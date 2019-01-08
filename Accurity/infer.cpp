#include "infer.h"
using namespace std;

//20171228 sort peak in descending order by no_of_windows
struct peak_greater_no_of_windows
{
    inline bool operator() (const OnePeak& peak1, const OnePeak& peak2)
    {
        return (peak1.no_of_windows > peak2.no_of_windows);
    }
};

Infer::Infer(string configFilepath, string segment_data_input_path,
             string snp_data_input_path,
             string output_dir,
             float segment_stddev_divider,
             int snp_coverage_min, float snp_coverage_var_vs_mean_ratio,
             int no_of_peaks_for_logL,
             int debug, int auto_)
        : _configFilepath(configFilepath),
          _segment_data_input_path(segment_data_input_path),
          _snp_data_input_path(snp_data_input_path),
          _output_dir(output_dir),
          _segment_stddev_divider(segment_stddev_divider),
          _snp_coverage_min(snp_coverage_min),
          _snp_coverage_var_vs_mean_ratio(snp_coverage_var_vs_mean_ratio),
          _no_of_peaks_for_logL(no_of_peaks_for_logL),
          _debug(debug),
          _auto(auto_)
{
    _periodObjVector.reserve(5);
    _snp_maf_stddev_divider = 20.0;
    if (_segment_stddev_divider<=0){
        cerr << fmt::format("ERROR: _segment_stddev_divider {} less than or equal to 0.\n", _segment_stddev_divider);
        exit(3);
    }
    if (_snp_coverage_min<=0){
        cerr << fmt::format("ERROR: _snp_coverage_min {} less than or equal to 0.\n", _snp_coverage_min);
        exit(3);
    }
    if (_snp_coverage_var_vs_mean_ratio<=0){
        cerr << fmt::format("ERROR: _snp_coverage_var_vs_mean_ratio {} less than or equal to 0.\n",
                            _snp_coverage_var_vs_mean_ratio);
        exit(3);
    }
    if (_no_of_peaks_for_logL<=0){
        cerr << fmt::format("ERROR: _no_of_peaks_for_logL {} less than or equal to 0.\n",
                            _no_of_peaks_for_logL);
        exit(3);
    }

    _returnCode = 0;
    _SNPs.resize(NUM_AUTO_CHR, vector<OneSNP>());
    _rc_ratio_segments.resize(MAX_RATIO_RANGE_HIGH_RES + 1,
                              vector<OneSegment>());
    _total_no_of_snps = 0;
    _total_no_of_snps_used = 0;
    _total_no_of_segments = 0;
    _total_no_of_segments_used = 0;

    _period_discover_run_type = 1;
    _genome_len_cnv_all = 0;
    _genome_len_clonal = 0;
    _ploidy_cnv_all = 0.0;
    _ploidy_clonal = 0.0;

    _probInstance = Prob();
    _period_obj_from_autocor = OnePeriod();
    _config = read_para(_configFilepath);

    // initialize all arrays
    for (int rc_ratio_int = 0; rc_ratio_int < MAX_RATIO_HIGH_RES+1;
         rc_ratio_int++)
        _ratio_int_pdf_vec.push_back(0.0);
    for (int i = 0; i <= kPeriodMax; i++) _cor_array[i] = 0.0;

    string tmp_file_path;
    _infer_outf.open(fmt::format("{}/infer.out.tsv", _output_dir).c_str());
    _infer_details_outf.open(fmt::format("{}/infer.out.details.tsv", _output_dir).c_str());

    if (_debug > 0)
    {
        tmp_file_path = _output_dir + "/rc_logLikelihood.log.tsv";
        _rc_logL_outf.open(tmp_file_path.c_str(), ios::trunc);

        tmp_file_path = _output_dir + "/snp_maf_exp_vs_adj.tsv";
        _snp_maf_exp_vs_adj_outf.open(tmp_file_path.c_str(),
                                      ios::trunc);
        _snp_maf_exp_vs_adj_outf
                << "period_int" << "\t" << "no_of_copy_nos_bf_1st_peak" << "\t" <<
                "peak_index" << "\t" << "cp" << "\t" << "major_allele_cp" << "\t" <<
                "fpeak" << "\t" << "purity" << "\t" << "ploidy" << "\t" <<
                "major_allele_fraction_exp" << "\t" <<
                "snp_coverage_mean_of_one_peak" << "\t" <<
                "snp_coverage_var_of_one_peak" << "\t" <<
                "no_of_snps.peak" << "\t" << "maf_exp_adjusted" << endl;

        tmp_file_path = _output_dir + "/snp_logL.log.tsv";
        _snp_logL_outf.open(tmp_file_path.c_str(), ios::trunc);
        _snp_logL_outf << "period_int" << "\t" <<
                       "no_of_copy_nos_bf_1st_peak" << "\t" <<
                       "peak_index" << "\t" <<
                       "peak_obj.no_of_maf_peaks" << "\t" <<
                       "index.maf_peak" << "\t" <<
                       "seg_count_per_maf_peak[i]" << "\t" <<
                       "var_of_maf_per_maf_peak[i]" << "\t" <<
                       "sq_diff_per_maf_peak[i]" << "\t" <<
                       "no_of_snps_per_maf_peak[i]" << "\t" <<
                       "std_per_maf_peak[i]" << "\t" << "lod_snp" << "\t" <<
                       "logL_snp" << "\t" << "logL_of_one_maf_peak" << "\t" <<
                       "ssum_sq_diff" << "\t" <<
                       "peak_obj.snp_maf_var" << "\t" <<
                       "no_of_snps_of_one_rc_peak" << "\t" <<
                       "currentPeriodObj.no_of_maf_peaks" << endl;
    }
    cerr <<"_segment_stddev_divider=" << _segment_stddev_divider << endl;
    cerr <<"_snp_maf_stddev_divider=" << _snp_maf_stddev_divider << endl;
    cerr <<"_snp_covearge_min=" << _snp_coverage_min << endl;
    cerr <<"_snp_coverage_var_vs_mean_ratio=" << _snp_coverage_var_vs_mean_ratio << endl;
    cerr <<"_no_of_peaks_for_logL=" << _no_of_peaks_for_logL << endl;

}

Infer::~Infer()
{
    _SNPs.clear();
    _rc_ratio_segments.clear();
    _infer_outf.close();
    _infer_details_outf.close();

    if (_debug > 0)
    {
        _rc_logL_outf.close();
        _snp_maf_exp_vs_adj_outf.close();
        _snp_logL_outf.close();
        rc_ratio_by_chr_out_file.flush();
        rc_ratio_by_chr_out_file.close();
    }
}

void Infer::recalibrate_Q_and_purity_based_on_cnv_ploidy(OnePeriod &best_period_obj){
    cerr << "Recalibrating Q and purity based on CNV ploidy ..." ;
    best_period_obj.rc_ratio_int_of_cp_2_corrected = FRESOLUTION -
                                                     (best_period_obj.ploidy_corrected-2.0)*best_period_obj.period_int;
    best_period_obj.purity_corrected = 2.0*best_period_obj.period_int
                                       /best_period_obj.rc_ratio_int_of_cp_2_corrected;
    cerr<< "Q=" << best_period_obj.rc_ratio_int_of_cp_2_corrected
        << " purity=" << best_period_obj.purity_corrected
        << " ploidy=" << best_period_obj.ploidy_corrected
        << endl;
}

int Infer::run()
{
    getSNPDataFromFile(_snp_data_input_path);
    getSegmentDataFromFile(_segment_data_input_path);
    calculate_autocor();
    if (_debug > 0) {
        string output_filepath = _output_dir + "/auto.tsv";
        ofstream tmp2(output_filepath.c_str());
        tmp2 << "read_count_ratio" << "\t" << "correlation"
             << "\n";
        for (int i = 0; i <= kPeriodMax; i++)
            tmp2 << i / FRESOLUTION << "\t" << _cor_array[i] << "\n";
        tmp2.close();

        output_snp_maf_by_segment();
    }
    /*** will be updated as the one to find the largest difference with the
     * smallest valley OnePeriod ***/

    vector<OnePeriod> candidate_period_vec;
    if (_auto > 0) {
        double left_x, right_x;
        double autocor_shift_diff[kPeriodMax];
        calc_autocor_shift_diff(autocor_shift_diff, left_x, right_x);
        _period_discover_run_type = 1;
        candidate_period_vec = infer_candidate_period_by_GADA(autocor_shift_diff, left_x, right_x,
                                                              _period_discover_run_type);
        /*
        if (candidate_period_vec.empty()){
            _period_discover_run_type = 2;
            candidate_period_vec = infer_candidate_period_by_GADA(autocor_shift_diff, left_x, right_x,
                                                                  _period_discover_run_type);
        }
        */
    } else {
        _returnCode = infer_candidate_period_by_autocor(_period_obj_from_autocor);
        candidate_period_vec.push_back(_period_obj_from_autocor);
        switch (_returnCode) {
            case 0:
                // 0=good
                break;
            case 1:
                _infer_outf << "CNV profile too noisy!\n";
                cerr << "CNV profile too noisy!\n";
                return 0;
            case 2:
                _infer_outf << "Not enough copy number variation!\n";
                cerr << "Not enough copy number variation!\n";
                return 0;
            default:
                return _returnCode;
        }
    }

    if(candidate_period_vec.empty()){
        string status_msg = "ERROR: No candidate period discovered.\n";
        _infer_outf << status_msg;
        cerr << status_msg;
        return 0;
    }

    _period_obj_from_logL = infer_best_period_by_logL(candidate_period_vec);

    if (_period_obj_from_logL.logL>0 && _period_obj_from_logL.best_purity>0) {

        _period_obj_from_logL.ploidy_corrected = output_copy_number_segments(_period_obj_from_logL,
                                                                             _period_obj_from_logL.peak_obj_vector);
        recalibrate_Q_and_purity_based_on_cnv_ploidy(_period_obj_from_logL);

        if (_period_obj_from_logL.purity_corrected>0 &&
            _period_obj_from_logL.purity_corrected<=1 &&
            _period_obj_from_logL.ploidy_corrected>=MIN_PLOIDY && _period_obj_from_logL.ploidy_corrected<=MAX_PLOIDY) {
            output_logL(_period_obj_from_logL, _periodObjVector);
        } else {
            cerr << fmt::format("ERROR: purity_corrected {} not in (0,1] or ploidy_corrected {} not in [{}, {}].\n",
                                _period_obj_from_logL.purity_corrected,
                                _period_obj_from_logL.ploidy_corrected,
                                MIN_PLOIDY,
                                MAX_PLOIDY);
        }
        if (_debug > 0) {
            output_snp_maf_by_peak(_period_obj_from_logL.peak_obj_vector);
            output_rc_ratio_of_peaks(_period_obj_from_logL.peak_obj_vector);
            output_peak_bounds(_period_obj_from_logL.peak_obj_vector);
        }
    } else {
        cerr << fmt::format("ERROR: logL {}<=0 or best_purity {} <=0!\n",
                            _period_obj_from_logL.logL,
                            _period_obj_from_logL.best_purity);
        return 0;
    }

    if (_debug > 2) {
        // below is about subclone peaks
        _sub_outf.open(fmt::format("{}/sub.tsv", _output_dir).c_str());
        _sub_outf << "period_int" << "\t" <<
                  "pool_hist_smooth[_half_period_int + period_int]"
                  << endl;

        _sub_peak_outf.open(fmt::format("{}/sub_peaks.final.tsv", _output_dir).c_str());
        _sub_peak_outf << "(_opt_purity / best_period * abs(called_peaks[i]))"
                       << endl;

        _half_period_int = _period_obj_from_logL.period_int / 2;
        for (int peak = _first_peak_obj.peak_center_int;
             peak < _ratio_int_pdf_vec.size();
             peak += _period_obj_from_logL.period_int) {
            for (int candidate_period_int = -_half_period_int;
                 candidate_period_int <= _half_period_int;
                 candidate_period_int++) {
                if (candidate_period_int + peak < 0 ||
                    candidate_period_int + peak > _ratio_int_pdf_vec.size())
                    continue;
                _pool_hist[_half_period_int + candidate_period_int] +=
                        _ratio_int_pdf_vec[peak + candidate_period_int];
            }
        }

        double pool_hist_smooth[2 * _half_period_int + 1];
        _probInstance.calc_window_average(_pool_hist, pool_hist_smooth,
                                          2 * _half_period_int + 1, 5);

        for (int candidate_period_int = -_half_period_int;
             candidate_period_int <= 30; candidate_period_int++) {
            _sub_outf << candidate_period_int << "\t" << pool_hist_smooth
            [_half_period_int + candidate_period_int] << endl;
        }

        vector<double> called_peaks =
                call_subclone_peaks(pool_hist_smooth, _half_period_int + 1);

        for (unsigned int i = 0; i < called_peaks.size(); i++) {
            if (i % 7 == 0)
                _sub_peak_outf << (_period_obj_from_logL.best_purity /
                                   _period_obj_from_logL.period_int *
                                   abs(called_peaks[i])) << "\t";
            // _sub_peak_outf<<called_peaks[i]<<" ";
            if (i % 7 == 6) _sub_peak_outf << endl;
        }
        _sub_outf.close();
        _sub_peak_outf.close();
    }
    return _returnCode;
}

// calculate the autocorrelation of the segmented, smoothed histogram of read
// count data.
// store auto-correlation into array cor_raw_array
void Infer::calculate_autocor()
{
    /*** correlation is calculated using the largest MAX_NUM_OF_COR_TO_SUM
     * number
     * of summands.  ***/
    cerr << "Calculating auto correlation ...";
    double cor_raw_array[kPeriodMax + 1];
    for (int shift = 0; shift <= kPeriodMax; shift++) {
        double sum_cor = 0;
        // cerr<<sum_cor<<endl;
        vector<double> all_terms;
        for (int i = 0; i + shift < _ratio_int_pdf_vec.size(); i++) {
            all_terms.push_back(_ratio_int_pdf_vec[i] *
                                _ratio_int_pdf_vec[i + shift]);
        }

        /** get the few MAX_NUM_OF_COR_TO_SUM summands **/
        sort(all_terms.begin(), all_terms.end(), larger);
        // for(int i=0;i<all_terms.size();i++) cerr<<all_terms[i]<<"\n";
        // cerr<<endl;
        int num_term = 0;
        for (int tm = 0; tm < min((int)all_terms.size(), MAX_NUM_OF_COR_TO_SUM);
             tm++) {
            sum_cor += all_terms[tm];
            num_term++;
        }
        if (num_term > 0)
            cor_raw_array[shift] = sum_cor;
        else
            cor_raw_array[shift] = 0;
        // cerr<<shift<<" "<<sum_cor<<endl;
    }

    // averaging with window size 4
    _cor_array[0] =
            (cor_raw_array[0] + cor_raw_array[1] + cor_raw_array[2]) / 3.0;
    _cor_array[1] = (cor_raw_array[0] + cor_raw_array[1] + cor_raw_array[2] +
                     cor_raw_array[3]) /
                    4.0;
    for (int i = 2; i <= kPeriodMax - 2; i++)
        _cor_array[i] =
                (cor_raw_array[i - 2] + cor_raw_array[i - 1] + cor_raw_array[i] +
                 cor_raw_array[i + 1] + cor_raw_array[i + 2]) /
                5.0;
    _cor_array[kPeriodMax - 1] =
            (cor_raw_array[kPeriodMax - 3] + cor_raw_array[kPeriodMax - 2] +
             cor_raw_array[kPeriodMax - 1] + cor_raw_array[kPeriodMax]) /
            4.0;
    _cor_array[kPeriodMax] =
            (cor_raw_array[kPeriodMax - 2] + cor_raw_array[kPeriodMax - 1] +
             cor_raw_array[kPeriodMax]) /
            3.0;
    cerr << "Done.\n";
}

void Infer::calc_autocor_shift_diff(double* all_diff, double &left_x, double &right_x) {
    cerr << "Calculating auto correlation shift-1 difference ..." << endl;
    double shift_diff;
    string tmp_file_path = _output_dir + "/GADA.in.tsv";
    ofstream gada_input_file;
    gada_input_file.open(tmp_file_path.c_str(), ios::trunc);
    gada_input_file << "period" << "\t" << "cor_shift_diff" << "\t" << "round_int" << endl;

    // add the GSL library to estimate shift_diff_min;
    double scale_cor;
    if (_cor_array[0] <= 0) {
        scale_cor = 10;
    } else {
        scale_cor = log10(_cor_array[0]);
    }
    for (int i = 0; i < kPeriodMax; i++) {
        if (_cor_array[i + 1] <= 0 || _cor_array[i] <= 0) {
            // avoid nan from log10(negative)
            shift_diff = 0;
        } else {
            shift_diff = (log10(_cor_array[i + 1]) / scale_cor) - (log10(_cor_array[i]) / scale_cor);
        }
        all_diff[i] = shift_diff;
        if (shift_diff > 0) {
            gada_input_file << i << "\t" << shift_diff
                            << "\t" << 1 << endl;
        } else {
            gada_input_file << i << "\t" << shift_diff
                            << "\t" << -1 << endl;
        }

    }

    //fit all_diff to a normal distribution to filter out flat area in auto-correlation
    long c_start = 0;
    long c_end = (long)kPeriodMax;
    float mean_value = 0.0;
    float sigma_value = 0.0;
    calculate_median_mad(all_diff, c_start, c_end, mean_value, sigma_value);
    double mean = (double)mean_value;
    double sigma = (double)sigma_value;
    left_x = gsl_cdf_gaussian_Pinv(0.4, sigma) + mean;
    right_x = gsl_cdf_gaussian_Qinv(0.4, sigma) + mean;
    string status_msg = fmt::format("#mean is: {}, sigma is: {}\n", mean, sigma);
    cerr << status_msg;
    gada_input_file << status_msg;
    status_msg = fmt::format("#shift_diff exclusion zone is : {} {}\n", left_x, right_x);
    gada_input_file << status_msg;
    gada_input_file.close();
    cerr << "Done.\n";
}
vector<OnePeriod> Infer:: infer_candidate_period_by_GADA(double* all_diff, double left_x, double right_x, int run_type)
{
    //run_type 1: require positive and negative slope distinction + normal distribution threshold
    //run_type 2: only normal distribution threshold
    cerr << fmt::format("Inferring candidate periods through GADA, run_type={}, left_x={}, right_x={} ...\n",
                        run_type, left_x, right_x);
    vector<int> period_int_vec;
    vector<double> _cor_array_shift_one_vec;
    double shift_diff;
    double positive_slope_min = max(0.0, right_x);
    double negative_slope_max = min(0.0, left_x);

    if (run_type==2){
        positive_slope_min = right_x;
        negative_slope_max = left_x;
    }
    for (int i = 0; i < kPeriodMax; i++) {
        //normalize correlation to [0,1]
        //shift_diff = _cor_array[i + 1]/_cor_array[0] - _cor_array[i]/_cor_array[0];
        shift_diff = all_diff[i];
        if (shift_diff > positive_slope_min) {
            period_int_vec.push_back(i);
            _cor_array_shift_one_vec.push_back(1);
        } else if (shift_diff < negative_slope_max){
            period_int_vec.push_back(i);
            _cor_array_shift_one_vec.push_back(-1);
        }

    }
    cerr << fmt::format("Initiating GADA instance ...");
    //initiate a GADA instance
    long M=_cor_array_shift_one_vec.size();    // no of data points in *input_array
    double *input_array=(double *)calloc(M, sizeof(double));
    for (int i=0; i<M; i++){
        input_array[i] = _cor_array_shift_one_vec[i];
    }

    double sigma2=-1;  //variance observed, if negative value, it will be estimated by the mean of the differences
    // I would recommend to be estimated on all the chromosomes and as a trimmed mean
    double BaseAmp=0.0;
    double a=0.5; //SBL hyper prior parameter
    double T=5.0; //backward elimination threshold
    long MinSegLen=10; // minimal length of a segment
    long debug=0;// verbosity ... set equal to 1 to see messages of SBLandBE(). 0 to not see them
    double convergenceDelta=1E-8;//1E-10 or 1E-8 seems to work well for this parameter. -- => ++ conv time
    //1E8 better than 1E10 seems to work well for this parameter. -- => -- conv time
    long maxNoOfInterations=50000; // 10000 id enough usually
    double convergenceMaxAlpha=1E8; // 1E8 maximum number of iterations to reach convergence ...
    double convergenceB=1E-20; //a number related to convergence = 1E-20
    int reportIntervalDuringBE=100000; // how often to report progress during backward elimination, default is 100k

    BaseGADA baseGADA = BaseGADA(
            input_array,
            M,
            sigma2,
            BaseAmp,
            a,
            T,
            MinSegLen,
            debug,
            convergenceDelta,
            maxNoOfInterations,
            convergenceMaxAlpha,
            convergenceB,
            reportIntervalDuringBE);
    baseGADA.SBLandBE();
    baseGADA.IextToSegLen();
    baseGADA.IextWextToSegAmp();
    cerr << fmt::format("GADA done\n");

    if (_debug>0) {
        //output the result
        ofstream _gada_seg_outf;
        _gada_seg_outf.open(fmt::format("{}/GADA.out.tsv", _output_dir).c_str(), ios::trunc);
        _gada_seg_outf << "Start" << "\t" << "End" << "\t" << "Length" << "\t" << "Ampl" << endl;
        for (int i = 0; i < baseGADA.K + 1; i++) {
            int period_start = period_int_vec[baseGADA.Iext[i]];
            int period_end = period_int_vec[baseGADA.Iext[i + 1] - 1];
            _gada_seg_outf << period_start << "\t"
                           << period_end << "\t"
                           << baseGADA.SegLen[i] << "\t"
                           << baseGADA.SegAmp[i] << endl;
        }
        _gada_seg_outf.close();
    }

    //select the candidate period
    vector<OnePeriod> candidate_period_vec;
    int min_period_segment_len = 10;
    int max_period_int = 600;
    for (int i = 0; i < baseGADA.K ; i++) {
        double delta = baseGADA.SegAmp[i] - baseGADA.SegAmp[i + 1];
        long current_segment_len = baseGADA.SegLen[i];
        long next_segment_len = baseGADA.SegLen[i];
        if (baseGADA.SegAmp[i] > 0 && baseGADA.SegAmp[i + 1] < 0 &&
            current_segment_len>=min_period_segment_len && next_segment_len >=min_period_segment_len) {
            //find the period with highest auto-correlation between positive and negative slopes
            double max_auto_cor = -1;
            int candidate_period_int = -1;
            if (run_type==1) {
                for (int period_int_tmp = period_int_vec[baseGADA.Iext[i + 1] - 1];
                     period_int_tmp < min(max_period_int, period_int_vec[baseGADA.Iext[i + 1]]);
                     period_int_tmp++) {
                    double auto_cor_tmp = _cor_array[period_int_tmp];
                    if (auto_cor_tmp > max_auto_cor) {
                        max_auto_cor = auto_cor_tmp;
                        candidate_period_int = period_int_tmp;
                    }
                }
            } else {
                candidate_period_int = (period_int_vec[baseGADA.Iext[i + 1] - 1] + period_int_vec[baseGADA.Iext[i + 1]])/2;
            }
            //lowe_bound and upper_bound are same. no more refining. THIS is the period.
            if (candidate_period_int>0 && candidate_period_int<=max_period_int) {
                OnePeriod candidate_period;
                candidate_period.period_int = candidate_period_int;
                candidate_period.lower_bound_int = candidate_period_int;
                candidate_period.upper_bound_int = candidate_period_int;
                candidate_period.auto_cor_value = max_auto_cor;
                candidate_period_vec.push_back(candidate_period);
            }
        }
    }

    vector <OnePeriod> candidate_period_top_two;
    int no_of_candidates = candidate_period_vec.size();
    if (no_of_candidates>=2) {
        sort(candidate_period_vec.begin(), candidate_period_vec.end(), greater<OnePeriod>());
        double max_autocor_of_candidate_period = candidate_period_vec[0].auto_cor_value;
        for (int i=0; i<2; i++){
            OnePeriod candidate_period = candidate_period_vec[i];
            if (max_autocor_of_candidate_period/candidate_period.auto_cor_value<10.0) {
                candidate_period_top_two.push_back(candidate_period);
            }
        }
    } else{
        candidate_period_top_two = candidate_period_vec;
    }

    //free input_array?
    //free(input_array);
    cerr << fmt::format("Found {} candidate periods.\n", candidate_period_vec.size());
    return candidate_period_top_two;
}

int Infer::infer_candidate_period_by_autocor(OnePeriod &period_obj)
{
    cerr << "Inferring best period from auto-correlation data ..." << endl;
    // find the best period
    // ppe_v6 consider the following cases:
    // 	1) a subset of cases with Whole Genome Duplications
    //	2) cases with not enough CNV signals
    //	3) cases with noisy CNV profiles

    int dmax_idx =
            -1;  // index for the maximum difference with the previous minimum
    int pmin_idx = -1;  // index for the minimum auto-correlation
    float dmax = -1e99, pmin = 1e99;
    float all_dif[kPeriodMax + 1];
    for (int i = 0; i <= kPeriodMax; i++)
    {
        if (_cor_array[i] < pmin)
        {
            pmin = _cor_array[i];
            pmin_idx = i;
        }
        float dif = _cor_array[i] - pmin;
        all_dif[i] = dif;
        if (dif > dmax)
        {
            dmax = dif;
            dmax_idx = i;
            _valley = pmin_idx;
            // cerr<<"dmax_idx "<<dmax_idx<<"\n";
        }
    }
    double second_max = _cor_array[dmax_idx];
    double thre = DEV1 * second_max;
    int period_min = dmax_idx - 1;
    for (; period_min > kPeriodMin &&
           (_cor_array[period_min] > thre ||
            dmax_idx - period_min < kPeriodHalfWidthMax);
           period_min--)
        ;
    int period_max = dmax_idx + 1;
    for (; period_max <= kPeriodMax &&
           (_cor_array[period_max] > thre ||
            period_max - dmax_idx < kPeriodHalfWidthMax);
           period_max++)
        ;
    // cerr<<dmax_idx << "\t" << period_min << "\t" << period_max NewLineMACRO;

    int shoulder_left = -1, shoulder_right = -1;
    for (int i = dmax_idx - 1; i > max(0, dmax_idx - 200); i--)
        if (_cor_array[i] <= SHOULDER_RATIO * _cor_array[dmax_idx])
        {
            shoulder_left = i;
            break;
        }
    for (int i = dmax_idx + 1; i < min(kPeriodMax, dmax_idx + 200); i++)
        if (_cor_array[i] <= SHOULDER_RATIO_LEFT * _cor_array[dmax_idx])
        {
            shoulder_right = i;
            break;
        }

    if (shoulder_left == -1 || shoulder_right == -1)
    {
        return 1;
    }

    if (_cor_array[dmax_idx] < 0.001 * _cor_array[0])
    {
        return 2;
    }

    vector<int> autocor_hist_peak_pos_vector;
    for (int shift = 1; shift <= kPeriodMax; shift++)
    {
        int scope = 20;  // ##
        bool is_peak = true;
        for (int i = max(0, shift - scope); i < shift; i++)
        {
            if (_cor_array[i] >= _cor_array[shift])
            {
                is_peak = false;
                break;
            }
        }
        if (!is_peak) continue;
        for (int i = shift + 1; i <= min(kPeriodMax, shift + scope); i++)
        {
            if (_cor_array[i] > _cor_array[shift])
            {
                is_peak = false;
                break;
            }
        }
        if (is_peak) autocor_hist_peak_pos_vector.push_back(shift);
    }
    // refine
    if (period_max - period_min <= 100)
    {
        double half = dmax_idx / 2.0, width = (period_max - period_min) / 6.0,
                half_max = half + width, half_min = max(0.0, half - width);
        float min_discrepancy = 9999;
        for (uint i = 0; i < autocor_hist_peak_pos_vector.size(); i++)
        {
            if (all_dif[autocor_hist_peak_pos_vector[i]] >=
                0.25 * _cor_array[dmax_idx] &&
                autocor_hist_peak_pos_vector[i] >= half_min &&
                autocor_hist_peak_pos_vector[i] <= half_max)
            {
                if (abs(autocor_hist_peak_pos_vector[i] - half) <
                    min_discrepancy)
                    min_discrepancy =
                            abs(autocor_hist_peak_pos_vector[i] - half);
                else
                    continue;
                dmax_idx = autocor_hist_peak_pos_vector[i];
                double peak_cor = _cor_array[dmax_idx];
                thre = DEV1 * peak_cor;
                cerr << dmax_idx << "\t" << peak_cor << "\t" << thre NewLineMACRO;
                period_min = dmax_idx - 1;
                for (; period_min > kPeriodMin &&
                       (_cor_array[period_min] > thre ||
                        dmax_idx - period_min < kPeriodHalfWidthMax);
                       period_min--)
                    ;
                period_max = dmax_idx + 1;
                for (; period_max <= kPeriodMax &&
                       (_cor_array[period_max] > thre ||
                        period_max - dmax_idx < kPeriodHalfWidthMax);
                       period_max++)
                    ;
                break;
            }
        }
    }

    period_obj.period_int = dmax_idx;
    period_obj.lower_bound_int = period_min;
    period_obj.upper_bound_int = period_max;
    cerr << "best period from autocorrelation: " << "\t" << dmax_idx << "\t" <<
         "lower bound: " << "\t" << period_min << "\t" <<
         "upper bound: " << "\t" << period_max NewLineMACRO;
    return 0;
}

OnePeak Infer::find_first_peak_ab_init(int candidate_period_int)
{
    OnePeak first_peak_obj = find_first_peak_given_bounds(
            candidate_period_int, kFirstPeakMin, kFirstPeakMax + kPeakHalfWidthMax);
    cerr << " Find_first_peak_ab_init() for period: " << candidate_period_int << endl
         << "  first peak: " << first_peak_obj.peak_center_int << endl;
    cerr << "  lower bound: " << first_peak_obj.lower_bound_int << endl;
    cerr << "  upper bound: " << first_peak_obj.upper_bound_int << endl;
    return first_peak_obj;
}

OnePeak Infer::find_first_peak_given_bounds(int candidate_period_int,
                                            int first_peak_lower_bound_int,
                                            int first_peak_upper_bound_int)
{
    /***
    The best start position for a given candidate_period_int.
  use the sum_of_window_count_at_periodic_peaks to choose the best first peak.

    It can correspond to regions with no peaks
    ***/
    if (_debug > 0)
    {
        cerr << "Finding first peak, period_int: "
             << candidate_period_int << ", within bounds of ("
             << first_peak_lower_bound_int << "-" << first_peak_upper_bound_int
             << ")... " << endl;
    }
    OnePeak first_peak_obj = OnePeak();
    double max_sum = -1;
    double all_sum[kFirstPeakMax + kPeakHalfWidthMax] = {0};  // add on 12-22,
    // change the
    // RESOLUTION
    int peak_width_assumed = candidate_period_int/4;
    for (int first_peak_int = first_peak_lower_bound_int;
         first_peak_int <= first_peak_upper_bound_int; first_peak_int++)
    {
        if (_ratio_int_pdf_vec[first_peak_int] < kPeakHeightMin)
            continue;
        all_sum[first_peak_int] += _ratio_int_pdf_vec[first_peak_int];
        // plus its neighbors
        for (int j = 1; j <= peak_width_assumed; j++)
        {
            all_sum[first_peak_int] +=
                    _ratio_int_pdf_vec[first_peak_int - j];
            all_sum[first_peak_int] +=
                    _ratio_int_pdf_vec[first_peak_int + j];
        }

        for (int a_peak_int = first_peak_int + candidate_period_int;
             a_peak_int < _ratio_int_pdf_vec.size() - peak_width_assumed;
             a_peak_int += candidate_period_int)
        {
            all_sum[first_peak_int] += _ratio_int_pdf_vec[a_peak_int];
            for (int j = 1; j <= peak_width_assumed; j++)
            {
                // cerr<<"all_sum "<<all_sum[first_peak_int]<<"
                // "<<a_peak_int<<endl;
                all_sum[first_peak_int] +=
                        _ratio_int_pdf_vec[a_peak_int - j];
                all_sum[first_peak_int] +=
                        _ratio_int_pdf_vec[a_peak_int + j];
            }
        }
        if (all_sum[first_peak_int] > max_sum)
        {
            max_sum = all_sum[first_peak_int];
            first_peak_obj.peak_center_int = first_peak_int;
        }
    }


    // get lower and upper bound
    int best_first_peak = first_peak_obj.peak_center_int;
    int candidate_peak_half_width = 1;
    for (; candidate_peak_half_width <= kPeakHalfWidthMax &&
           best_first_peak - candidate_peak_half_width >= kFirstPeakMin &&
           best_first_peak + candidate_peak_half_width <=
           kFirstPeakMax + kPeakHalfWidthMax;
           candidate_peak_half_width++)
    {
        if (all_sum[best_first_peak - candidate_peak_half_width] +
            all_sum[best_first_peak + candidate_peak_half_width] <
            2 * DEV2 * all_sum[best_first_peak])
            break;
    }
    //set maximum peak width to 1/2 of period.
    candidate_peak_half_width = min(candidate_peak_half_width, peak_width_assumed);

    first_peak_obj.half_width_int = candidate_peak_half_width;
    first_peak_obj.lower_bound_int =
            max(0, first_peak_obj.peak_center_int - candidate_peak_half_width);
    first_peak_obj.upper_bound_int =
            first_peak_obj.peak_center_int + candidate_peak_half_width;
    if (_debug > 0)
    {
        cerr << "  best_first_peak center: " << best_first_peak << endl
             << "  sum of window count at all periodic peaks: "
             << all_sum[best_first_peak] << endl
             << "  half_width_int: " << candidate_peak_half_width << endl;
    }
    return first_peak_obj;
}

OnePeak Infer::refine_first_peak(int candidate_period_int,
                                 OnePeak &first_peak_obj)
{
    /***
    The best start position for a given candidate_period_int. It can correspond
    to regions with
    no peaks
    ***/
    return find_first_peak_given_bounds(candidate_period_int,
                                        first_peak_obj.lower_bound_int,
                                        first_peak_obj.upper_bound_int);
}

vector<OnePeak> Infer::find_peaks(OnePeriod &period_obj,
                                  OnePeak &first_peak_obj)
{
    int period_int = period_obj.period_int;
    int peak_index = 0;
    vector<OnePeak> peak_obj_vector;

    if (first_peak_obj.half_width_int > period_int / 2) {
        first_peak_obj.half_width_int = (int) (period_int / 2 * 0.9);
    }
    int half_width_int = first_peak_obj.half_width_int;
    int first_peak_center_int = first_peak_obj.peak_center_int;
    for (int i = 0;
         first_peak_center_int + period_int*i <= MAX_RATIO_HIGH_RES; i++) {
        //initial peak center and no_of_periods_since_1st_peak
        int peak_center_int = first_peak_center_int + period_int*i;
        int lower_bound_int = max(peak_center_int - half_width_int, 0);
        int upper_bound_int = min(peak_center_int + half_width_int, MAX_RATIO_HIGH_RES);

        OnePeak peak_obj =
                OnePeak(peak_center_int, i, lower_bound_int, upper_bound_int, half_width_int);
        peak_obj.ResetCounters();
        peak_obj.no_of_periods_since_1st_peak = i;

        vector<int> peak_rc_ratio_vector;
        for (int rc_ratio_int = lower_bound_int; rc_ratio_int <= upper_bound_int;
             rc_ratio_int++) {
            // newly add on June 3 2012 to plot snp mafs
            peak_rc_ratio_vector.push_back(rc_ratio_int);
        }
        //refine peak center
        //TODO refine peak half_width_int
        refine_peak_center(peak_obj, peak_rc_ratio_vector, period_int, first_peak_center_int);
        peak_obj_vector.push_back(peak_obj);
    }

    OneSegmentSNPs oneSegmentSNPs;
    for (int i=0; i<peak_obj_vector.size(); i++) {
        OnePeak& peak_obj = peak_obj_vector[i];
        peak_obj.ResetCounters();
        double coverage_squared_sum = 0.0;
        for (int rc_ratio_int = peak_obj.lower_bound_int; rc_ratio_int <= peak_obj.upper_bound_int;
             rc_ratio_int++) {
            // newly add on June 3 2012 to plot snp mafs
            peak_obj.segment_rc_ratio_vector.push_back(rc_ratio_int);
            for (uint seg = 0; seg < _rc_ratio_segments[rc_ratio_int].size();
                 seg++) {
                OneSegment oneSegment = _rc_ratio_segments[rc_ratio_int][seg];
                peak_obj.segment_obj_vector.push_back(oneSegment);
                oneSegmentSNPs = oneSegment.oneSegmentSNPs;
                if (oneSegmentSNPs.no_of_snps <= 0)
                    continue;
                //maf_mean is log10(maf_mean), hence minus sign
                kernel_smoothing(-oneSegmentSNPs.maf_mean*RESOLUTION, oneSegmentSNPs.maf_stddev*RESOLUTION,
                                 oneSegmentSNPs.no_of_snps, peak_obj.maf_int_pdf_vec);
                peak_obj.snp_coverage_sum +=
                        oneSegmentSNPs.coverage_mean * oneSegmentSNPs.no_of_snps;
                peak_obj.snp_coverage_squared_sum +=
                        oneSegmentSNPs.coverage_mean *
                        oneSegmentSNPs.coverage_mean * oneSegmentSNPs.no_of_snps;
                coverage_squared_sum += oneSegmentSNPs.coverage_squared_sum;
                peak_obj.snp_coverage_var_sum += oneSegmentSNPs.coverage_var;
                peak_obj.no_of_snps += oneSegmentSNPs.no_of_snps;
                peak_obj.no_of_windows +=
                        _rc_ratio_segments[rc_ratio_int][seg].no_of_windows;
            }

        }
        if (peak_obj.no_of_snps>0) {
            peak_obj.snp_coverage_mean =
                    peak_obj.snp_coverage_sum / peak_obj.no_of_snps;
            peak_obj.snp_coverage_var = coverage_squared_sum / double(peak_obj.no_of_snps)
                                        - peak_obj.snp_coverage_mean * peak_obj.snp_coverage_mean;
        }
        peak_index++;
    }

    if (_debug>0){
        cerr << fmt::format("Found {} peaks.\n", peak_obj_vector.size());
    }
    return peak_obj_vector;
}

int Infer::output_peak_bounds(vector<OnePeak> &peak_obj_vector)
{
    string tmp_file_path = _output_dir + "/peak_bounds.tsv";
    cerr << fmt::format("Outputting peak bounds to {} ... ", tmp_file_path);
    ofstream peak_bounds_outf;
    peak_bounds_outf.open(tmp_file_path.c_str());
    peak_bounds_outf << "lowerBound\tupperBound\n";
    vector<OnePeak>::iterator it = peak_obj_vector.begin();
    int counter = 0;
    for (; it != peak_obj_vector.end(); it++)
    {
        counter++;
        peak_bounds_outf << fmt::format("{}\t{}\n", it->lower_bound_int / FRESOLUTION,
                                        it->upper_bound_int / FRESOLUTION);
    }
    /*
    // cerr<<"no_of_snps_in_peak "<<no_of_snps_in_peak<<"\n";
    if (no_of_snps_in_peak > 10) {
      stringstream ss;
      ss << "snp_maf_" << peak_index;
      string output_filepath = _output_dir + "/" + ss.str();
      ofstream snp_plot(output_filepath.c_str());
      for (int ii = 0; ii <= 101; ii++)
        if (frac_vs_size[ii] > 0)
          snp_plot << ii << "\t" << frac_vs_size[ii] << "\n";
      snp_plot.close();
    }
    */
    peak_bounds_outf.close();
    cerr << fmt::format(" {} peaks.\n", counter);
    return 0;
}

OnePeriod Infer::infer_best_period_by_logL(vector<OnePeriod> &candidate_period_vec)
{
    cerr << fmt::format("Inferring the best period by log likelihood from {} candidates ... \n",
                        candidate_period_vec.size());
    double best_period_logL = (-1e99);
    OnePeriod best_period_obj = OnePeriod();
    for (int candidate_period_index=0;
         candidate_period_index<candidate_period_vec.size();
         candidate_period_index++)
    {
        OnePeriod &candidate_period = candidate_period_vec[candidate_period_index];
        int candidate_period_int = candidate_period.period_int;

        string status_msg = fmt::format("### candidate period_int: {}\n", candidate_period_int);
        cerr << status_msg;
        _infer_details_outf << status_msg;
        candidate_period.first_peak_obj = find_first_peak_ab_init(candidate_period_int);
        candidate_period.first_peak_int = candidate_period.first_peak_obj.peak_center_int;
        candidate_period.width = candidate_period.first_peak_obj.half_width_int;
        // find the first peak
        // peak must in a auto correlation field and
        // first peak < 1000
        //  _num_peak_less_one_half = (_one_half -_first_peak_obj.peak_center_int + 1) /
        // _period_obj_from_autocor.period_int;

        //candidate_period.first_peak_obj =
        //    refine_first_peak(candidate_period_int, first_peak_obj_prior);
        int first_peak_int = candidate_period.first_peak_obj.peak_center_int;
        candidate_period.peak_obj_vector = find_peaks(
                candidate_period, candidate_period.first_peak_obj);

        // for each period, sum likelihood of all peaks (segments and snps)
        candidate_period.ResetCounters();
        double sum_adj_logL = 0;
        if (_debug > 0)
        {
            _rc_logL_outf << "period_int" << "\t"
                          << candidate_period_int << "\t"
                          << "half-width" << "\t"
                          << candidate_period.period_int - candidate_period.lower_bound_int << endl;
            _rc_logL_outf << "peak_index" << "\t" << "peak_center_float" << "\t"
                          << "logL_peak" << "\t" << "candidate_period.logL"
                          << endl;
        }
        //set no_of_peaks_for_logL for this period
        candidate_period.no_of_peaks_for_logL = min(_no_of_peaks_for_logL, int(candidate_period.peak_obj_vector.size()));
        for (unsigned int peak_index = 0;
             peak_index < candidate_period.no_of_peaks_for_logL;
             peak_index++)
        {
            OnePeak &peak_obj =
                    candidate_period.peak_obj_vector[peak_index];
            float peak_center_float = peak_obj.peak_center_int * 1.0 / RESOLUTION;
            double adj_logL;
            float logL_peak = calc_one_peak_logL_rc(
                    peak_center_float, peak_obj, candidate_period, adj_logL);
            candidate_period.logL += logL_peak;
            sum_adj_logL += adj_logL;
            if (_debug > 0)
            {
                _rc_logL_outf << peak_index << "\t" << peak_center_float << "\t"
                              << logL_peak << "\t"
                              << candidate_period.logL << endl;
            }
        }
        if (candidate_period.no_of_windows <= 0) continue;
        // float
        // readCount_logL_penalty=0.5*log(candidate_period.no_of_windows)*total_used_peaks;
        // TODO the total used peak is (10 * RESOLUTION - first_peak_int)/period_int ?
        candidate_period.logL_rc_penalty =
                -0.5 * log(candidate_period.no_of_windows) *
                (10 * RESOLUTION - first_peak_int) / candidate_period_int;
        candidate_period.logL += candidate_period.logL_rc_penalty;
        candidate_period.logL_rc = candidate_period.logL;
        candidate_period.adj_logL_rc =
                -log(sqrt(sum_adj_logL / candidate_period.no_of_windows) /
                     candidate_period_int *
                     FRESOLUTION);
        //-0.5*log(candidate_period.no_of_segments)/candidate_period.no_of_segments-0.5*log(candidate_period.no_of_segments)*(50*1000-first_peak_int)/period_int/candidate_period.no_of_segments;

        this->infer_no_of_copy_nos_bf_1st_peak_for_one_period_by_logL_snp(candidate_period);

        candidate_period.logL += candidate_period.best_logL_snp;
        //20171229 take average
        candidate_period.logL = candidate_period.logL/candidate_period.no_of_peaks_for_logL;
        if (_debug>0) {
            cerr << fmt::format(" best_logL_snp: {}\n", candidate_period.best_logL_snp);
            cerr << fmt::format(" no_of_peaks_for_logL: {}\n", candidate_period.no_of_peaks_for_logL);
            cerr << fmt::format(" purity: {}\n", candidate_period.best_purity);
            cerr << fmt::format(" ploidy: {}\n", candidate_period.best_ploidy);
            cerr << fmt::format(" logL: {}\n", candidate_period.logL);
        }
        if (candidate_period.best_purity>0 && candidate_period.logL > best_period_logL){
            best_period_logL = candidate_period.logL;
            _first_peak_obj = candidate_period.first_peak_obj;
            best_period_obj = candidate_period;
        }
        _periodObjVector.push_back(candidate_period);
    }
    cerr << fmt::format("### Best period from likelihood: {}\n", best_period_obj.period_int)
         << "  best_purity: " << best_period_obj.best_purity << endl
         << "  best_ploidy: " << best_period_obj.best_ploidy << endl
         << "  Q: " << best_period_obj.rc_ratio_int_of_cp_2 << endl
         << "  logL: " << best_period_obj.logL << endl
         << "  best_no_of_copy_nos_bf_1st_peak: " << best_period_obj.best_no_of_copy_nos_bf_1st_peak << endl
         << "  first_peak_int: " << best_period_obj.first_peak_int << endl;
    return best_period_obj;
}

int Infer::output_logL(OnePeriod &best_period_obj,
                       vector<OnePeriod> &period_obj_vector)
{
    cerr << "Outputting logL ...";
    int best_period_int = best_period_obj.period_int;
    double best_period_logL = best_period_obj.logL;

    _infer_outf << "purity" << "\t"
                << "ploidy" << "\t"
                << "purity_naive" << "\t"
                << "ploidy_naive" << "\t"
                << "rc_ratio_of_cp_2" << "\t"
                << "rc_ratio_of_cp_2_corrected" << "\t"
                << "segment_stddev_divider" << "\t"
                << "snp_maf_stddev_divider" << "\t"
                << "snp_coverage_min" << "\t"
                << "snp_coverage_var_vs_mean_ratio" << "\t"
                << "period_discover_run_type\t"
                << "no_of_peaks_for_logL"
                << endl;
    _infer_outf << setprecision(5)
                << best_period_obj.purity_corrected << "\t"
                << best_period_obj.ploidy_corrected << "\t"
                << best_period_obj.best_purity << "\t"
                << best_period_obj.best_ploidy << "\t"
                << best_period_obj.rc_ratio_int_of_cp_2 << "\t"
                << best_period_obj.rc_ratio_int_of_cp_2_corrected << "\t"
                << _segment_stddev_divider << "\t"
                << _snp_maf_stddev_divider << "\t"
                << _snp_coverage_min << "\t"
                << _snp_coverage_var_vs_mean_ratio << "\t"
                << _period_discover_run_type << "\t"
                << _no_of_peaks_for_logL
                << endl;
    _infer_outf << "logL" << "\t"
                << "period" << "\t"
                << "best_no_of_copy_nos_bf_1st_peak" << "\t"
                << "first_peak_int"
                << endl;
    _infer_outf << best_period_obj.logL << "\t"
                << best_period_int << "\t"
                << best_period_obj.best_no_of_copy_nos_bf_1st_peak << "\t"
                << best_period_obj.first_peak_int
                << endl;
    _infer_outf << "no_of_segments" << "\t"
                << "no_of_segments_used" << "\t"
                << "no_of_snps" << "\t"
                << "no_of_snps_used" << endl;
    _infer_outf << _total_no_of_segments << "\t" << _total_no_of_segments_used << "\t"
                << _total_no_of_snps << "\t"
                << _total_no_of_snps_used
                << endl;
    if (_debug > 0)
    {
        // output all candidate snp likelihoods of the best period object
        _infer_details_outf << "best_period_int" << "\t" << "logL" << "\t" <<
                            "index.logL_snp_vector" << "\t" <<
                            "no_of_copy_nos_bf_1st_peak[i]" << "\t" <<
                            "purity_vector[i]" << "\t" <<
                            "ploidy_vector[i]" << "\t" <<
                            "logL_snp_vector[i]" << "\t" <<
                            "lod_snp_vector[i]" << "\t" <<
                            "snp_penalty_vector[i]" << "\t" <<
                            "snp_no_of_parameters_vector[i]" << endl;
        for (unsigned int i = 0; i < best_period_obj.logL_snp_vector.size();
             i++)
        {
            _infer_details_outf << best_period_int << "\t"
                                << best_period_logL << "\t"
                                << i << "\t"
                                << best_period_obj.no_of_copy_nos_bf_1st_peak_vector[i] << "\t"
                                << best_period_obj.purity_vector[i] << "\t"
                                << best_period_obj.ploidy_vector[i] << "\t"
                                << best_period_obj.logL_snp_vector[i] << "\t"
                                << best_period_obj.lod_snp_vector[i] << "\t"
                                << best_period_obj.snp_penalty_vector[i] << "\t"
                                << best_period_obj.snp_no_of_parameters_vector[i] << endl;
        }
        // output likelihoods of all periods tested
        _infer_details_outf << "period_int" << "\t"
                            << "logL" << "\t"
                            << "maxlogL-logL" << "\t"
                            << "logL_rc" << "\t"
                            << "logL_rc_penalty" << "\t"
                            << "best_logL_snp" << "\t"
                            << "best_lod_snp" << "\t"
                            << "best_logL_snp_penalty" << "\t"
                            << "best_logL_snp_no_of_parameters" << "\t"
                            << "best_no_of_copy_nos_bf_1st_peak" << "\t"
                            << "first_peak_int" << "\t"
                            << "best_purity" << "\t"
                            << "best_ploidy" << endl;
        vector<OnePeriod>::iterator it = period_obj_vector.begin();
        for (; it != period_obj_vector.end(); it++)
        {
            OnePeriod period_obj = *it;
            _infer_details_outf
                    << period_obj.period_int << "\t"
                    << period_obj.logL << "\t"
                    << best_period_logL - period_obj.logL << "\t"
                    << period_obj.logL_rc << "\t"
                    << period_obj.logL_rc_penalty << "\t"
                    << period_obj.best_logL_snp << "\t"
                    << period_obj.best_lod_snp << "\t"
                    << period_obj.best_logL_snp_penalty << "\t"
                    << period_obj.best_logL_snp_no_of_parameters << "\t"
                    << period_obj.best_no_of_copy_nos_bf_1st_peak << "\t"
                    << period_obj.first_peak_int << "\t"
                    << period_obj.best_purity << "\t"
                    << period_obj.best_ploidy
                    << endl;
        }
    }
    cerr << "Done." << endl;
    return 0;
}

double Infer::getReadDepthFromRegCoeffFile(string inputFname)
{
    cerr << "Reading depth from " << inputFname << " ...";
    double depth;
    if (!isfile(inputFname)){
        cerr << inputFname << " does not exist. ERROR!" << endl;
        exit(3);
    }
    ifstream reg_inf(inputFname.c_str());
    string line;
    getline(reg_inf, line);
    getline(reg_inf, line);
    reg_inf >> depth;
    reg_inf.close();
    depth = (depth * 100 / _config.window);
    cerr << "Depth=" << depth << endl;
    return depth;
}

double Infer::infer_no_of_copy_nos_bf_1st_peak_for_one_period_by_logL_snp(
        OnePeriod &candidate_period)
{
    double purity, ploidy;
    candidate_period.best_logL_snp = (-1e99);
    int first_peak_int = candidate_period.first_peak_obj.peak_center_int;
    int period_int = candidate_period.period_int;
    //to assign copy number 2 to the peak with the most no_of_windows.
    //sort peak_obj_vector by no_of_windows in descending order
    vector<OnePeak> &peak_obj_vector = candidate_period.peak_obj_vector;
    sort(peak_obj_vector.begin(), peak_obj_vector.end(), peak_greater_no_of_windows());
    OnePeak &tallest_peak = peak_obj_vector[0];

    if (_debug>0) {
        cerr << fmt::format("  Tallest peak index={}, peak_center_int={}, no_of_windows={}.\n",
                            tallest_peak.peak_index, tallest_peak.peak_center_int,
                            tallest_peak.no_of_windows);

    }
    if (tallest_peak.peak_index>2){
        cerr << fmt::format("  WARNING: return now as tallest_peak.peak_index {} is bigger than 2. Not correct.\n",
                            tallest_peak.peak_index);
        //something wrong
        //The tallest peak's copy number is more than 2 due to the order of peaks.
        return candidate_period.best_logL_snp;
    }
    int no_of_copy_nos_bf_1st_peak_prior = max(0, 2 - tallest_peak.peak_index);
    //sort the peak_obj_vector back to its original order by peak_center_int
    sort(peak_obj_vector.begin(), peak_obj_vector.end());
    if (_debug>0) {
        cerr << fmt::format("  First peak's peak_index={}, peak_center_int={}, no_of_windows={}.\n",
                            peak_obj_vector[0].peak_index, peak_obj_vector[0].peak_center_int,
                            peak_obj_vector[0].no_of_windows);
    }
    //allow 20% period deficit in no_of_cps_bf_1st_peak
    int max_no_of_copy_nos_bf_1st_peak = int(floor(double(first_peak_int) / double(period_int) +0.2));
    if (no_of_copy_nos_bf_1st_peak_prior>max_no_of_copy_nos_bf_1st_peak){
        //invalid. no more peaks possible before 1st peak.
        no_of_copy_nos_bf_1st_peak_prior = max_no_of_copy_nos_bf_1st_peak;
    } else if (no_of_copy_nos_bf_1st_peak_prior<max_no_of_copy_nos_bf_1st_peak){
        //only consider one choice. SNP MAF not very good in selecting no_of_copy_nos_bf_1st_peak.
        max_no_of_copy_nos_bf_1st_peak = no_of_copy_nos_bf_1st_peak_prior;
    }
    if (_debug>0) {
        cerr << fmt::format("  no_of_copy_nos_bf_1st_peak_prior={}\n  max_no_of_copy_nos_bf_1st_peak={}\n",
                            no_of_copy_nos_bf_1st_peak_prior, max_no_of_copy_nos_bf_1st_peak);
    }
    OneSegmentSNPs oneSegmentSNPs;

    int cp_no_two_rc_ratio_int = -1;
    for (int no_of_copy_nos_bf_1st_peak = no_of_copy_nos_bf_1st_peak_prior;
         no_of_copy_nos_bf_1st_peak <= max_no_of_copy_nos_bf_1st_peak;
         no_of_copy_nos_bf_1st_peak++)
    {
        int cp_no_two_peak_index = 2 - no_of_copy_nos_bf_1st_peak;
        if (cp_no_two_peak_index>=0 && cp_no_two_peak_index<peak_obj_vector.size()){
            cp_no_two_rc_ratio_int = peak_obj_vector[cp_no_two_peak_index].peak_center_int;
        } else {
            cp_no_two_rc_ratio_int =
                    first_peak_int + (cp_no_two_peak_index) * period_int;
        }
        calc_purity_ploidy_from_period_and_cp_no_two(
                cp_no_two_rc_ratio_int, period_int, purity, ploidy);
        if (ploidy < MIN_PLOIDY || ploidy > MAX_PLOIDY) continue;
        // reset
        double logL_snp = 0.0, lod_snp = 0.0;
        candidate_period.ResetSNPCounters();
        double logL_of_one_maf_peak = 0.0;
        float ssum_sq_diff = 0;
        // calculate the expected MAF for a SNP at any major allele copy number
        for (unsigned int peak_index = 0; peak_index < candidate_period.no_of_peaks_for_logL;
             peak_index++)
        {
            OnePeak &peak_obj = peak_obj_vector[peak_index];
            int cp = no_of_copy_nos_bf_1st_peak + peak_index;
            // double local_ploidy = (2 - 2 * purity) + cp * purity;
            vector<double> maf_expected_vector;
            // calculate the mean maf and its variance, input for maf
            // adjustment,
            if (peak_obj.no_of_snps <= 5) continue;
            for (int major_allele_cp = ceil(cp / 2.0); major_allele_cp <= cp;
                 major_allele_cp++)
            {
                double maf_expected =
                        (1 - purity + major_allele_cp * purity) /
                        (2 - 2 * purity + cp * purity);
                // TODO why skip maf_expected 1?
                if (maf_expected < 0.5 ||
                    maf_expected > 1)
                    continue;
                double maf_exp_adjusted = adjust_maf_expect(maf_expected, peak_obj.snp_coverage_mean,
                                                            peak_obj.snp_coverage_mean*_snp_coverage_var_vs_mean_ratio);
                maf_expected_vector.push_back(maf_exp_adjusted);
                if (_debug > 0)
                {
                    _snp_maf_exp_vs_adj_outf
                            << period_int << "\t"
                            << no_of_copy_nos_bf_1st_peak << "\t"
                            << peak_index << "\t"
                            << cp << "\t"
                            << major_allele_cp << "\t"
                            << first_peak_int << "\t"
                            << purity << "\t"
                            << ploidy << "\t"
                            << maf_expected << "\t"
                            << peak_obj.snp_coverage_mean << "\t"
                            << peak_obj.snp_coverage_var << "\t"
                            << peak_obj.no_of_snps << "\t"
                            << pow(10, maf_exp_adjusted)
                            << endl;
                }
            }  // all snp maf peaks

            // double std_h0 = sqrt(0.5 / 3 /
            // maf_expected_vector.size());
            peak_obj.no_of_maf_peaks = maf_expected_vector.size();
            if (peak_obj.no_of_maf_peaks <= 0) continue;
            double var_of_maf_per_maf_peak[peak_obj.no_of_maf_peaks],
                    sq_diff_per_maf_peak[peak_obj.no_of_maf_peaks],
                    no_of_snps_per_maf_peak[peak_obj.no_of_maf_peaks],
                    std_per_maf_peak[peak_obj.no_of_maf_peaks];
            int *seg_count_per_maf_peak;
            seg_count_per_maf_peak = new int[peak_obj.no_of_maf_peaks];

            for (int i = 0; i < peak_obj.no_of_maf_peaks; i++)
            {
                var_of_maf_per_maf_peak[i] = sq_diff_per_maf_peak[i] =
                no_of_snps_per_maf_peak[i] = 0.0;
                seg_count_per_maf_peak[i] = 0;
            }

            vector<OneSegment>::iterator one_segment_iterator =
                    peak_obj_vector[peak_index].segment_obj_vector.begin();
            for (; one_segment_iterator !=
                   peak_obj_vector[peak_index].segment_obj_vector.end();
                   one_segment_iterator++)
            {
                oneSegmentSNPs = one_segment_iterator->oneSegmentSNPs;
                if (oneSegmentSNPs.no_of_snps <= 5 || oneSegmentSNPs.maf_stddev<=0)
                    continue;
                double min_diff_sq = 1.0e99;
                int best_maf_peak_index = -1;
                for (int i = 0; i < peak_obj.no_of_maf_peaks; i++)
                {
                    double diff_sq = (maf_expected_vector[i] - oneSegmentSNPs.maf_mean) *
                                     (maf_expected_vector[i] - oneSegmentSNPs.maf_mean);
                    if (diff_sq < min_diff_sq)
                    {
                        min_diff_sq = diff_sq;
                        best_maf_peak_index = i;
                    }
                }  // find the nearest expected oneSegmentSNPs.maf_mean
                seg_count_per_maf_peak[best_maf_peak_index]++;

                var_of_maf_per_maf_peak[best_maf_peak_index] +=
                        (min_diff_sq * oneSegmentSNPs.no_of_snps +
                         oneSegmentSNPs.maf_stddev * oneSegmentSNPs.maf_stddev *
                         oneSegmentSNPs.no_of_snps * oneSegmentSNPs.no_of_snps);

                sq_diff_per_maf_peak[best_maf_peak_index] += min_diff_sq * oneSegmentSNPs.no_of_snps;
                no_of_snps_per_maf_peak[best_maf_peak_index] += oneSegmentSNPs.no_of_snps;
                // all maf peaks for one rc peak
            }  // all segments of one rc_peak
            // TODO release memory of  seg_count_per_maf_peak??

            peak_obj.snp_maf_var = 0;
            for (int i = 0; i < peak_obj.no_of_maf_peaks; i++)
            {
                candidate_period.no_of_maf_peaks++;
                if (no_of_snps_per_maf_peak[i] <= 5 || var_of_maf_per_maf_peak[i]<=0) continue;
                peak_obj.snp_maf_var += var_of_maf_per_maf_peak[i];
                std_per_maf_peak[i] = sqrt(var_of_maf_per_maf_peak[i] /
                                           (no_of_snps_per_maf_peak[i] - 1));
                ssum_sq_diff += sq_diff_per_maf_peak[i];
                logL_of_one_maf_peak =
                        -sq_diff_per_maf_peak[i] / (2.0 * std_per_maf_peak[i] * std_per_maf_peak[i]) -
                        (log(std_per_maf_peak[i]) + 0.5 * log(2 * _probInstance.PI)) * no_of_snps_per_maf_peak[i];
                // TODO should use sq_diff_per_maf_peak[i] instead of ssum_sq_diff??

                logL_snp += logL_of_one_maf_peak;
                // double
                // h1=-log(std_per_maf_peak[i])*no_of_snps_per_maf_peak[i];
                // double
                // h1=-log(std_per_maf_peak[i])*no_of_snps_per_maf_peak[i];
                // double
                // snp_logL_penalty=-0.5*peak_obj.no_of_maf_peaks*log(no_of_snps_per_maf_peak[i]);
                // double
                // h1=-log(std_per_maf_peak[i])*seg_count_per_maf_peak[i];

                // logL_snp+=h1+snp_logL_penalty;//-h0);
                //		float
                // maf_stddev=sqrt(var_grand/(peak_obj.no_of_snps-1));
                //		logL_snp-=log(maf_stddev)*peak_obj.no_of_snps;
                //		float
                // snp_logL_penalty=-0.5*num_of_freq_peak*log(peak_obj.no_of_snps*1.0);
                //		logL_snp+=snp_logL_penalty;
                if (_debug)
                {
                    _snp_logL_outf << period_int << "\t"
                                   << no_of_copy_nos_bf_1st_peak << "\t"
                                   << peak_index << "\t"
                                   << peak_obj.no_of_maf_peaks << "\t"
                                   << i << "\t"
                                   << seg_count_per_maf_peak[i] << "\t"
                                   << var_of_maf_per_maf_peak[i] << "\t"
                                   << sq_diff_per_maf_peak[i] << "\t"
                                   << no_of_snps_per_maf_peak[i] << "\t"
                                   << std_per_maf_peak[i] << "\t"
                                   << lod_snp << "\t"
                                   << logL_snp << "\t"
                                   << logL_of_one_maf_peak << "\t"
                                   << ssum_sq_diff << "\t"
                                   << peak_obj.snp_maf_var << "\t"
                                   << peak_obj.no_of_snps << "\t"
                                   << candidate_period.no_of_maf_peaks
                                   << endl;  // add on 2016-12-19
                }
            }  // all maf peaks of one rc_peak
            if (peak_obj.no_of_snps <= 5 || peak_obj.snp_maf_var<=0) continue;
            candidate_period.no_of_snps += peak_obj.no_of_snps;
            double std_of_maf_of_one_rc_peak =
                    sqrt(peak_obj.snp_maf_var / (peak_obj.no_of_snps - 1.0));
            double lod_of_one_rc_peak =
                    -log(std_of_maf_of_one_rc_peak) * peak_obj.no_of_snps -
                    log(peak_obj.no_of_maf_peaks * 2 * sqrt(12.0)) *
                    peak_obj.no_of_snps;
            lod_snp += lod_of_one_rc_peak;
            if (_debug)
            {
                _snp_logL_outf << period_int << "\t"
                               << no_of_copy_nos_bf_1st_peak << "\t"
                               << peak_index << "\t"
                               << -1 << "\t"
                               << -1 << "\t"
                               << -1 << "\t"
                               << -1 << "\t"
                               << -1 << "\t"
                               << -1 << "\t"
                               << -1 << "\t"
                               << lod_snp << "\t"
                               << logL_snp << "\t"
                               << -1 << "\t"
                               << ssum_sq_diff << "\t"
                               << peak_obj.snp_maf_var << "\t"
                               << peak_obj.no_of_snps << "\t"
                               << -1
                               << endl;  // add on 2016-12-19
            }

        }  // each rc peak
        //     logL_snp = (-log(maf_stddev) * candidate_period.no_of_snps);
        if (candidate_period.no_of_snps<=5) {
            continue;
        }
        float snp_logL_penalty =
                -0.5 * candidate_period.no_of_maf_peaks *
                log(candidate_period.no_of_snps * 1.0);
        // ## CHANGE
        // float
        // snp_logL_penalty=-log(candidate_period.no_of_maf_peaks)*candidate_period.no_of_snps;
        // //
        logL_snp += snp_logL_penalty;
        candidate_period.logL_snp_vector.push_back(logL_snp);
        candidate_period.lod_snp_vector.push_back(lod_snp);
        candidate_period.purity_vector.push_back(purity);
        candidate_period.ploidy_vector.push_back(ploidy);
        candidate_period.no_of_copy_nos_bf_1st_peak_vector.push_back(
                no_of_copy_nos_bf_1st_peak);
        candidate_period.snp_penalty_vector.push_back(snp_logL_penalty);
        candidate_period.snp_no_of_parameters_vector.push_back(
                candidate_period.no_of_maf_peaks);

        if (logL_snp > candidate_period.best_logL_snp)
        {
            // TODO use logL_snp or lod_snp?
            candidate_period.best_logL_snp = logL_snp;
            candidate_period.best_lod_snp = lod_snp;
            candidate_period.best_no_of_copy_nos_bf_1st_peak =
                    no_of_copy_nos_bf_1st_peak;
            candidate_period.best_purity = purity;
            candidate_period.best_ploidy = ploidy;
            candidate_period.best_adj_logL_snp =
                    sqrt(ssum_sq_diff / candidate_period.no_of_snps);
            candidate_period.rc_ratio_int_of_cp_2 = cp_no_two_rc_ratio_int;
            candidate_period.best_logL_snp_penalty = snp_logL_penalty;
            candidate_period.best_logL_snp_no_of_parameters =
                    candidate_period.no_of_maf_peaks;
        }
    }  // each possible copy number status (no_of_copy_nos_bf_1st_peak)

    return candidate_period.best_logL_snp;
}

double Infer::calc_one_peak_logL_rc(float peak_center_float, OnePeak &peak_obj,
                                    OnePeriod &period_obj, double &adj_logL)
{
    // cerr<<"begining of each peak "<< peak_center_index <<"
    // "<<peak_center_float<<endl;
    double no_of_windows_in_peak = 0;
    //double rc_ratio_var_per_rc_peak = 0;
    double std_t;
    double ssum_diff = 0.0;
    for (uint i = 0; i < peak_obj.segment_rc_ratio_vector.size(); i++)
    {
        int ratio_int = peak_obj.segment_rc_ratio_vector[i];
        vector<OneSegment>::iterator it = _rc_ratio_segments[ratio_int].begin();
        for (; it != _rc_ratio_segments[ratio_int].end(); it++)
        {
            OneSegment oneSegment = *it;
            float rc_ratio = oneSegment.rc_ratio;
            double stddev = oneSegment.stddev;
            int no_of_windows = oneSegment.no_of_windows;
            double sq_diff = (rc_ratio - peak_center_float) *
                             (rc_ratio - peak_center_float) * no_of_windows;
            ssum_diff += sq_diff;
            no_of_windows_in_peak += no_of_windows;
            //rc_ratio_var_per_rc_peak +=
            //    (sq_diff + stddev * stddev * no_of_windows * no_of_windows);
            period_obj.no_of_segments++;
        }  // all segments for a copy ratio
    }      // all copy ratios corresponding to one peak
    period_obj.no_of_rc_peaks++;
    if (no_of_windows_in_peak < 1) return 0;
    period_obj.no_of_windows += no_of_windows_in_peak;
    // rc_ratio_var_per_rc_peak/=(FRESOLUTION*FRESOLUTION);
    std_t = sqrt(ssum_diff / no_of_windows_in_peak);
    adj_logL = ssum_diff;
    float logL =
            -ssum_diff / 2.0 / std_t / std_t -
            (log(std_t) + 0.5 * log(2 * _probInstance.PI)) * no_of_windows_in_peak;

    return logL;
}

void Infer::calc_purity_ploidy_from_period_and_cp_no_two(
        int cp_no_two_rc_ratio_int, int period_int, double &purity, double &ploidy)
{
    purity = 2.0 * period_int / (float)cp_no_two_rc_ratio_int;
    ploidy = 2 + (FRESOLUTION - cp_no_two_rc_ratio_int) / (float)period_int;
    // ploidy = 2 * PL_C; // a ad-hoc adjustment from DNA index to ploidy
    // cerr<<"first_peak_int in calc_purity_ploidy_from_period_and_cp_no_two()
    // "<<first_peak_int<<"
    // "<<purity<<" "<<ploidy<<endl;
}


inline int Infer::chrStr_to_index(string chrm)
{
    for (int i = 0; i < NUM_AUTO_CHR; i++)
        if (chrm == chromosomeNameArray[i]) return i;
    return -1;
}

double Infer::adjust_maf_expect(double maf_expected,
                                double snp_coverage_mean,
                                double snp_coverage_var)
{
    /*
  double maf_expected, double snp_coverage_mean , double
  snp_coverage_var
    */
    double freq = 0;
    double pdf = 0;
    double cdf = 0;
    double neg_bi_p, neg_bi_r;
    _probInstance.neg_bi_repara(snp_coverage_mean, snp_coverage_var,
                                neg_bi_p, neg_bi_r);
    if (snp_coverage_var <= 1.1 * snp_coverage_mean) {
        // Poisson
        for (int i = _snp_coverage_min; i < snp_coverage_mean*10; i++) {
            pdf = pow(_probInstance.E, -snp_coverage_mean) *
                  pow(snp_coverage_mean, i) / _probInstance.factorial(i);
            freq += (pdf *
                     _probInstance.binomial_max_log10(i, maf_expected));
            cdf += pdf;
        }
    } else {
        for (int i = _snp_coverage_min; i < snp_coverage_mean*10; i++) {
            pdf = _probInstance.neg_bi(neg_bi_p, neg_bi_r, i);
            freq += (pdf * _probInstance.binomial_max_log10(i, maf_expected));
            cdf += pdf;
        }
    }
    freq /= cdf;
    return freq;
}

int Infer::getSNPDataFromFile(string input_file_path)
{
    /*** read in SNP data from inputFname and store data in 3-d array _SNPs
     * ***/
    cerr << "Reading SNPs from " << input_file_path << " ..." << endl;
    if (!isfile(input_file_path)){
        cerr << input_file_path << " does not exist. ERROR!" << endl;
        exit(3);
    }
    ifstream input_file;
    input_file.open(input_file_path.c_str(), std::ios::in | std::ios::binary);
    boost::iostreams::filtering_streambuf<boost::iostreams::input> input_filter_stream_buffer;
    input_filter_stream_buffer.push(boost::iostreams::gzip_decompressor());
    input_filter_stream_buffer.push(input_file);
    std::istream input_stream(&input_filter_stream_buffer);

    string line, chr_string;
    int loc, chr_index, coverage;
    float maf;
    _total_no_of_snps = 0;
    int noOfLines = 0;

    std::getline(input_stream, line);
    while (!line.empty())
    {
        noOfLines++;
        std::vector<std::string> element_vec = string_split(line, "\t");
        chr_string = element_vec[0];
        std::getline(input_stream, line);
        if (chr_string[0]=='#' || element_vec[1]=="pos") {
            //ignore comments and header
            continue;
        } else if (chr_string[0] == 'c') {
            chr_string = chr_string.substr(3);
        }
        chr_index = atoi(chr_string.c_str()) - 1;
        if (chr_index == -1) {
            continue;
        }
        loc = stoi(element_vec[1]);
        maf = stof(element_vec[2]);
        coverage = stoi(element_vec[3]);
        //20171227 take log10
        OneSNP oneSNP(chr_index, loc, maf >= 0.5 ? log10(maf) : log10(1 - maf), coverage);
        _SNPs[chr_index].push_back(oneSNP);
        _total_no_of_snps++;
    }
    input_file.close();
    cerr << _SNPs.size() << " chromosomes, " << _total_no_of_snps << " SNPs, "
         << noOfLines << " lines." << endl;
    return 0;
}

// read in the results from BIC-seq for the read count data
int Infer::getSegmentDataFromFile(string input_file_path)
{
    /*** read in segmentation data from inputFname and store data in 3-d
     * array
     * _SNPs _rc_ratio_segments ***/
    cerr << "Reading in segments from " << input_file_path << " ...\n";
    if (!isfile(input_file_path)){
        cerr << input_file_path << " does not exist. ERROR!" << endl;
        exit(3);
    }
    ifstream input_file;
    input_file.open(input_file_path.c_str(), std::ios::in | std::ios::binary);
    boost::iostreams::filtering_streambuf<boost::iostreams::input> input_filter_stream_buffer;
    input_filter_stream_buffer.push(boost::iostreams::gzip_decompressor());
    input_filter_stream_buffer.push(input_file);
    std::istream input_stream(&input_filter_stream_buffer);

    string line;
    int start, end, no_of_valid_windows;
    float read_count_ratio;
    double ratio_stddev;
    string chr_string;
    _total_no_of_segments = 0;
    _total_no_of_segments_used = 0;

    int **noOfWindowsByRatioAndChr;
    noOfWindowsByRatioAndChr = new int *[MAX_RATIO_HIGH_RES + 1];
    for (int i = 0; i <= MAX_RATIO_HIGH_RES; i++)
    {
        noOfWindowsByRatioAndChr[i] = new int[NUM_AUTO_CHR];
        for (int chr_index = 0; chr_index < NUM_AUTO_CHR; chr_index++)
            noOfWindowsByRatioAndChr[i][chr_index] = 0;
    }
    int noOfLines = 0;
    std::getline(input_stream, line);
    while (!line.empty())
    {
        noOfLines++;
        std::vector<std::string> element_vec = string_split(line, "\t");

        chr_string = element_vec[0];
        std::getline(input_stream, line);

        if (chr_string[0]=='#') {
            //ignore comments
            continue;
        }
        _total_no_of_segments++;
        start = stoi(element_vec[1]);
        end = stoi(element_vec[2]);
        read_count_ratio = stof(element_vec[3]);
        //decrease coverage ratio stddev to enhance signal/noise ratio
        ratio_stddev = stof(element_vec[4])/_segment_stddev_divider;
        no_of_valid_windows = stoi(element_vec[5]);

        if (_total_no_of_segments % 10000 == 0) cerr << _total_no_of_segments << "\n";
        if (read_count_ratio > 0.1 && ratio_stddev > read_count_ratio)
        {
            //TODO why?
            cerr << "Warning: Too much variation at " << chr_string << start << end
                 << ". Skip! " << read_count_ratio << " " << ratio_stddev << " "
                 << no_of_valid_windows << endl;
            continue;
        }

        int ratio_high_res = int(read_count_ratio * RESOLUTION);
        if (read_count_ratio <= MAX_RATIO_RANGE && ratio_stddev > 1e-12)
        {
            int chr_index = chrStr_to_index(chr_string);
            if (chr_index == -1) continue;
            OneSegment oneSegment =
                    OneSegment(chr_index, start, end, read_count_ratio, ratio_stddev,
                               no_of_valid_windows);
            // SNP info
            if (read_count_ratio <= MAX_RATIO)
            {
                noOfWindowsByRatioAndChr[ratio_high_res][chr_index] +=
                        no_of_valid_windows;
                findSNPsWithinSegment(oneSegment);
                kernel_smoothing(read_count_ratio * RESOLUTION, ratio_stddev*RESOLUTION, no_of_valid_windows,
                                 _ratio_int_pdf_vec);
            }
            _rc_ratio_segments[ratio_high_res].push_back(oneSegment);
            _total_no_of_segments_used ++;
        }
    }
    input_file.close();
    if (_debug > 0)
    {
        output_segment_ratio(noOfWindowsByRatioAndChr);
    }
    for (int i = 0; i <= MAX_RATIO_HIGH_RES; i++)
    {
        delete[] noOfWindowsByRatioAndChr[i];
    }
    delete[] noOfWindowsByRatioAndChr;
    cerr << _total_no_of_segments << " segments. " << _total_no_of_segments_used << " segments used. " << _total_no_of_snps_used << " SNPs used." << endl;
    return 0;
}

int Infer::output_segment_ratio(int **noOfWindowsByRatioAndChr)
{
    string file_name1 = _output_dir + "/rc_ratio_window_count_smoothed.tsv";
    cerr << "Outputting segment ratio data to " << file_name1 << "...";
    ofstream tmp(file_name1.c_str());
    tmp << "read_count_ratio"
        << "\t"
        << "window_count_smoothed" << endl;
    for (int rc_ratio_int = 0; rc_ratio_int < _ratio_int_pdf_vec.size();
         rc_ratio_int++)
        tmp << rc_ratio_int * 1.0 / RESOLUTION << "\t"
            << _ratio_int_pdf_vec[rc_ratio_int] << endl;
    tmp.close();
    cerr << "Done.\n";

    string file_name2 = _output_dir + "/rc_ratio_no_of_windows_by_chr.tsv";
    cerr << "Outputting segment ratio data to " << file_name2 << "...";
    //IMPORTANT rc_ratio_by_chr_out_stream_buffer & rc_ratio_by_chr_out_file can't be declared here.
    // otherwise, the program will hung forever by function end.
    // #### gzip output does not work. It simply hung or output an empty gzip file.
    // boost::iostreams::filtering_streambuf<boost::iostreams::output> rc_ratio_by_chr_out_stream_buffer;
    //rc_ratio_by_chr_out_stream_buffer.push(boost::iostreams::gzip_compressor());
    rc_ratio_by_chr_out_file.open(file_name2.c_str(), std::ios::out | std::ios::binary);
    //rc_ratio_by_chr_out_stream_buffer.push(rc_ratio_by_chr_out_file);
    //std::ostream rc_ratio_by_chr_out_stream(&rc_ratio_by_chr_out_stream_buffer);

    rc_ratio_by_chr_out_file << "readCountRatioX1000";
    for (int chr_index = 0; chr_index < NUM_AUTO_CHR; chr_index++)
        rc_ratio_by_chr_out_file << "\t" << setw(8) << "chr" << chr_index << "_noOfWindows";
    rc_ratio_by_chr_out_file << "\n";

    for (int i = 0; i <= MAX_RATIO_HIGH_RES; i++)
    {
        rc_ratio_by_chr_out_file << setw(8) << i;
        for (int chr_index = 0; chr_index < NUM_AUTO_CHR; chr_index++)
            rc_ratio_by_chr_out_file << "\t" << setw(8)
                                     << noOfWindowsByRatioAndChr[i][chr_index];
        rc_ratio_by_chr_out_file << "\n";
    }
    //rc_ratio_by_chr_out_stream.flush();
    //rc_ratio_by_chr_out_file.flush();
    cerr << "Done." << endl;
    return 0;
}

int Infer::output_snp_maf_by_segment()
{
    string tmp_file_path = _output_dir + "/snp_maf_by_segment.tsv";
    cerr << fmt::format("Outputting SNP MAFs by segments to {} ... ", tmp_file_path);
    ofstream snp_maf_by_segment_outf;
    snp_maf_by_segment_outf.open(tmp_file_path.c_str());


    snp_maf_by_segment_outf << "rc_ratio_int" << "\t"
                            << "segment.index" << "\t"
                            << "maf_mean" << "\t"
                            << "maf_stddev" << "\t"
                            << "coverage_mean" << "\t"
                            << "coverage_var" << "\t"
                            << "coverage_squared_sum" << "\t"
                            << "no_of_snps" << endl;
    int counter = 0;
    for (int it = 0; it < MAX_RATIO_HIGH_RES + 1; it++) {
        for (uint seg = 0; seg < _rc_ratio_segments[it].size(); seg++) {
            counter ++;
            OneSegmentSNPs oneSegmentSNPs =
                    _rc_ratio_segments[it][seg].oneSegmentSNPs;
            snp_maf_by_segment_outf << it << "\t"
                                    << seg << "\t"
                                    << pow(10, oneSegmentSNPs.maf_mean) << "\t"
                                    << oneSegmentSNPs.maf_stddev << "\t"
                                    << oneSegmentSNPs.coverage_mean << "\t"
                                    << oneSegmentSNPs.coverage_var << "\t"
                                    << oneSegmentSNPs.coverage_squared_sum << "\t"
                                    << oneSegmentSNPs.no_of_snps << endl;
        }
    }
    snp_maf_by_segment_outf.close();
    cerr << fmt::format("{} segments.\n", counter);
    return 0;
}

int Infer::output_snp_maf_by_peak(vector<OnePeak> &peak_obj_vector)
{
    string tmp_file_path=fmt::format("{}/snp_maf_by_peak.tsv", _output_dir);
    cerr << fmt::format("Outputting SNP MAFs by peaks to {} ... ", tmp_file_path);
    ofstream snp_maf_by_peak_outf;
    snp_maf_by_peak_outf.open(tmp_file_path.c_str());
    snp_maf_by_peak_outf << "peak.index" << "\t"
                         << "peak_center_int" << "\t"
                         << "no_of_snps" << "\t"
                         << "no_of_maf_peaks" << "\t"
                         << "segment.index" << "\t"
                         << "rc_ratio_int" << "\t"
                         << "maf_mean" << "\t"
                         << "maf_stddev" << "\t"
                         << "no_of_snps" << "\t"
                         << "coverage_mean" << "\t"
                         << "coverage_var" << "\t"
                         << "coverage_squared_sum" << endl;
    for (uint i = 0; i < peak_obj_vector.size(); i++) {
        OnePeak& peak_obj = peak_obj_vector[i];
        for (uint seg_index = 0; seg_index < peak_obj.segment_obj_vector.size(); seg_index++) {
            OneSegment oneSegment = peak_obj.segment_obj_vector[seg_index];
            OneSegmentSNPs oneSegmentSNPs = oneSegment.oneSegmentSNPs;
            snp_maf_by_peak_outf << i << "\t"
                                 << peak_obj.peak_center_int << "\t"
                                 << peak_obj.no_of_snps << "\t"
                                 << peak_obj.no_of_maf_peaks << "\t"
                                 << seg_index << "\t"
                                 << oneSegment.get_rc_ratio_high_res() << "\t"
                                 << pow(10, oneSegmentSNPs.maf_mean) << "\t"
                                 << oneSegmentSNPs.maf_stddev << "\t"
                                 << oneSegmentSNPs.no_of_snps << "\t"
                                 << oneSegmentSNPs.coverage_mean << "\t"
                                 << oneSegmentSNPs.coverage_var << "\t"
                                 << oneSegmentSNPs.coverage_squared_sum
                                 << endl;
        }
    }
    snp_maf_by_peak_outf.close();

    int counter = 0;
    for (uint i = 0; i < peak_obj_vector.size(); i++) {
        OnePeak& peak_obj = peak_obj_vector[i];
        if (peak_obj.no_of_snps>0){
            counter ++;
            ofstream outf;
            outf.open(fmt::format("{0}/snp_maf_pdf_of_peak_{1}.tsv", _output_dir, i).c_str());
            outf << "#peak.index=" << i << endl;
            outf << "#peak_center_int=" << peak_obj.peak_center_int << endl;
            outf << "#no_of_maf_peaks=" << peak_obj.no_of_maf_peaks << endl;
            outf << "maf\tcount\n";
            for (uint x_i = 0; x_i < peak_obj.maf_int_pdf_vec.size(); x_i++) {
                outf << fmt::format("{}\t{}\n", pow(10, -float(x_i)/1000.0), peak_obj.maf_int_pdf_vec[x_i]);
            }
            outf.close();
        }
    }
    cerr << fmt::format("{} peaks with valid data.\n", counter);
    return 0;
}

int Infer::output_rc_ratio_of_peaks(vector<OnePeak> &peak_obj_vector)
{
    string tmp_file_path = _output_dir + "/rc_ratios_of_peaks_of_best_period.tsv";
    cerr << fmt::format("Outputting RC ratio of peaks to {} ... ", tmp_file_path);
    ofstream rc_ratios_of_peaks_outf;
    rc_ratios_of_peaks_outf.open(tmp_file_path.c_str(), ios::trunc);
    rc_ratios_of_peaks_outf << "period_int" << "\t" << "peak_index" << "\t" <<
                            "peak_center_int" << "\t" << "ratio_int" << endl;
    int counter = 0;
    for (uint i = 0; i < peak_obj_vector.size(); i++) {
        for (uint j = 0; j < peak_obj_vector[i].segment_rc_ratio_vector.size();
             j++) {
            OnePeak &peak_obj = peak_obj_vector[i];
            counter++;
            rc_ratios_of_peaks_outf
                    << _period_obj_from_logL.period_int << "\t" << i << "\t" <<
                    peak_obj.peak_center_int << "\t" <<
                    peak_obj.segment_rc_ratio_vector[j] << endl;
        }
    }
    cerr << fmt::format(" {} segments.\n", counter);
    rc_ratios_of_peaks_outf.close();
    return 0;
}

int Infer::findSNPsWithinSegment(OneSegment &oneSegment)
{
    if (oneSegment.end_pos <= oneSegment.start_pos) {
        //ToDo report error instead?
        oneSegment.oneSegmentSNPs = OneSegmentSNPs();
        return 0;
    }
    double maf_sum = 0, maf_ssum = 0, weight, sum_weight = 0, sum_cov = 0, ssum_cov = 0;
    int total_no_of_snps = 0;
    vector<OneSNP>::iterator oneSNPIt = _SNPs[oneSegment.chr_index].begin();
    vector<float> maf_vector;
    vector<float> coverage_float_vector;

    for (; oneSNPIt < _SNPs[oneSegment.chr_index].end(); oneSNPIt++) {
        if (oneSNPIt->position >= oneSegment.start_pos &&
            oneSNPIt->position <= oneSegment.end_pos) {
            total_no_of_snps++;
            maf_vector.push_back(oneSNPIt->maf);
            coverage_float_vector.push_back(oneSNPIt->coverage*1.0);
            // use robust mean/maf_stddev, stop weight by coverage
            /*
            weight = sqrt(oneSNPIt->coverage);
            maf_sum += oneSNPIt->maf * weight;
            maf_ssum += oneSNPIt->maf * oneSNPIt->maf * weight;
            sum_weight += weight;
            sum_cov += oneSNPIt->coverage;
            ssum_cov += oneSNPIt->coverage * oneSNPIt->coverage;
            // cerr<<_SNPs[chr_index][1][i]<<"\t"<<_SNPs[chr_index][2][i]<<"\n";
             */
        }
    }
    if (total_no_of_snps <= 10) {
        //not enough SNPs to do robust mean/maf_stddev
        // placeholder to match _rc_ratio_segments, but all values =-1
        oneSegment.oneSegmentSNPs = OneSegmentSNPs();
    } else {
        float maf_mean;
        float maf_stddev;
        double maf_squared_sum =0.0;
        float coverage_mean;
        float coverage_stddev;
        double coverage_squared_sum=0.0;
        int no_of_snps_to_use=0;
        calculate_robust_mean_stddev(maf_vector, 30, maf_mean, maf_stddev, maf_squared_sum, no_of_snps_to_use);
        no_of_snps_to_use=0;
        calculate_robust_mean_stddev(coverage_float_vector, 30, coverage_mean, coverage_stddev,
                                     coverage_squared_sum, no_of_snps_to_use);

        // is there something wrong? the last argument should be
        // ssum_cov/ no_of_snps - sum_cov * sum_cov / no_of_snps / no_of _snps
        //float coverage_mean=sum_cov / no_of_snps;
        //float coverage_var = ssum_cov - sum_cov * sum_cov / no_of_snps;
        //TODO is it proper to divide maf_stddev by _snp_maf_stddev_divider?
        oneSegment.oneSegmentSNPs =
                OneSegmentSNPs(maf_mean, maf_stddev/_snp_maf_stddev_divider, no_of_snps_to_use,
                               coverage_mean, coverage_stddev*coverage_stddev, coverage_squared_sum);
        _total_no_of_snps_used += no_of_snps_to_use;
    }
    return 0;
}

// kernal smoothing of the histogram for segmented read count data
// the kernal bandwidth is determined by the standard deviation of the read
// counts of each segment.
void Infer::kernel_smoothing(double mean_value, double stddev,
                             int sample_size,
                             vector<double> &vec_to_hold_data) {
    int i_start = max(double(0), floor((mean_value - 2 * stddev)));
    int i_end = min(ceil(mean_value + 2 * stddev), double(vec_to_hold_data.size()-1));
    for (int i = i_start; i <= i_end; i++) {
        vec_to_hold_data[i] +=
                sample_size * kGaussianDensityFrontScalar / stddev *
                exp(-(i - mean_value) * (i - mean_value) / (2 * stddev * stddev));
    }
}

int Infer::refine_peak_center(OnePeak &peak_obj, vector<int> segment_rc_ratio_vector,
                              int candidate_period_int,
                              int first_peak_center_int) {
    double mean_rc_ratio_of_one_peak = 0;
    int cnt = 0;
    int no_of_rc_ratios_of_one_peak = segment_rc_ratio_vector.size();
    for (int i = 0; i < no_of_rc_ratios_of_one_peak; i++) {
        int ratio_int = segment_rc_ratio_vector[i];
        vector<OneSegment> all_segs_at_one_rc_ratio =
                _rc_ratio_segments[ratio_int];
        int no_of_segments = all_segs_at_one_rc_ratio.size();
        for (int i = 0; i < no_of_segments; i++) {
            float rc_ratio = all_segs_at_one_rc_ratio[i].rc_ratio;
            int no_of_windows = all_segs_at_one_rc_ratio[i].no_of_windows;
            cnt += no_of_windows;
            mean_rc_ratio_of_one_peak += (rc_ratio * no_of_windows);
        }  // all segments for a copy ratio
    }      // all copy ratios corresponding to one peak

    if (cnt > 0){
        mean_rc_ratio_of_one_peak /= cnt;

        peak_obj.peak_center_int = mean_rc_ratio_of_one_peak*RESOLUTION;
        peak_obj.lower_bound_int = max(peak_obj.peak_center_int - peak_obj.half_width_int, 0);
        peak_obj.upper_bound_int = min(peak_obj.peak_center_int + peak_obj.half_width_int,
                                       MAX_RATIO_HIGH_RES);
        /*
        // round the float to the closest integer
        peak_obj.no_of_periods_since_1st_peak =
            floor((peak_obj.peak_center_int - first_peak_center_int) /
                      period_int +
                  0.5);
                  */
    }
    // cerr << "no_of_periods_since_1st_peak " << no_of_periods_since_1st_peak
    // << endl;
    return peak_obj.no_of_periods_since_1st_peak;
}

vector<double> Infer::call_subclone_peaks(double *a, int size)
{
    cerr << "Calling subclone peaks ...";
    vector<double> peaks;
    // int should_size = 3;
    int clip_size = 5;
    // double con=1/sqrt(2*3.14159)/10;
    for (int i = clip_size; i < size - clip_size; i++)
    {
        double mean1, std1, mean2, std2, mean3, std3, mean4, std4;
        // double mn=0;
        // for(int j=i-clip_size;j<=i+clip_size;j++) mn+=(a[j]/(2*clip_size+1));
        if (a[i] < 2e3) continue;
        _probInstance.moving_average(a, size, mean1, std1, i, 55, 45);
        _probInstance.moving_average(a, size, mean2, std2, i, 27, 23);
        _probInstance.moving_average(a, size, mean3, std3, i, 10, 10);
        _probInstance.moving_average(a, size, mean4, std4, i, 5, 5);
        // if(a[i]<2e3 || a[i]<mean+3*stddev || mn<mean+2*stddev) continue;
        if ((a[i] >= a[i - 1] && a[i] >= a[i - 2] && a[i] >= a[i - 3] &&
             a[i] >= a[i + 1] && a[i] >= a[i + 2] && a[i] >= a[i + 3]) &&
            //	a[i]>mean4+std4 &&
            //( mean2>mean1+std1 || mean3>mean2+std2 || mean4>mean3+std3)) {
            (mean2 > mean1 && mean3 > mean2 && mean4 > mean3 && a[i] > mean4) &&
            ((mean2 - mean1) / std1 + (mean3 - mean2) / std2 +
             (mean4 - mean3) / std3 + (a[i] - mean4) / std4 >
             3) &&
            (a[i] - mean1) / std1 > 5 && 1)
        {
            peaks.push_back(i - size + 1);
            peaks.push_back(a[i]);
            peaks.push_back((a[i] - mean4) / std4);
            peaks.push_back((mean4 - mean3) / std3);
            peaks.push_back((mean3 - mean2) / std2);
            peaks.push_back((mean2 - mean1) / std1);
            peaks.push_back((a[i] - mean1) / std1);
        }
    }
    cerr << "Done.\n";
    return peaks;
}

double Infer::output_copy_number_segments(OnePeriod &best_period_obj,
                                          vector<OnePeak> &peak_obj_vector)
{
    // segmentations with copy number assignment
    string output_file_path = _output_dir + "/cnv.output.tsv";
    //cnv.interval.tsv is only for sub-clonal peaks. In addition to copy number, it contains copy interval info.
    string output_cp_interval = _output_dir + "/cnv.interval.tsv";

    cerr << "Outputting copy number to  " << output_file_path << endl;

    _genome_len_cnv_all = 0;
    _genome_len_clonal = 0;

    double cp_number_multi_len = 0;
    double cp_number_multi_len_clonal = 0;

    OnePeak first_peak_obj = best_period_obj.first_peak_obj;
    bool is_ratio_looked[MAX_RATIO_RANGE_HIGH_RES + 1];
    for (int i = 0; i <= MAX_RATIO_RANGE_HIGH_RES; i++)
    {
        is_ratio_looked[i] = false;
    }
    // string
    // CASE=_segment_data_input_path.substr(0,_segment_data_input_path.find('/'));
    ofstream outf(output_file_path.c_str());
    outf << "chr" << "\t"
         << "cumu_start" << "\t"
         << "cumu_end" << "\t"
         << "cp" << "\t"
         << "major_allele_cp" << "\t"
         << "copy_no_float" << "\t"
         << "oneSegment.stddev" << "\t"
         << "maf_mean" << "\t"
         << "maf_stddev" << "\t"
         << "maf_expected" << "\t"
         << "start" << "\t"
         << "end"
         << endl;
    ofstream out_interval(output_cp_interval.c_str());
    if (_debug > 0)
    {
        out_interval << "chr\t"
                     << "start\t"
                     << "end\t"
                     << "cp\t"
                     << "copy_no_float\t"
                     << "cp_stddev\t"
                     << "interval_left\t"
                     << "interval_right\t"
                     << "seg_stddev\t"
                     << "seg_num_of_window"
                     << endl;
    }

    int best_period_int = best_period_obj.period_int;
    int first_peak_int = best_period_obj.first_peak_int;
    int no_of_copy_nos_bf_1st_peak =
            best_period_obj.best_no_of_copy_nos_bf_1st_peak;
    double purity = best_period_obj.best_purity;
    double ploidy = best_period_obj.best_ploidy;
    // double pl = best_period_obj.ploidy;
    // float pl_all = (1 - purity) * 2 + purity * pl;

    double CHR_ACU[NUM_CHR + 1];
    CHR_ACU[0] = 0;
    for (int i = 0; i < NUM_CHR; i++)
    {
        CHR_ACU[i + 1] = (CHR_ACU[i] + CHR_SIZE[i] * 1e6);
    }

    int no_of_peaks = peak_obj_vector.size();
    OneSegmentSNPs oneSegmentSNPs;
    for (int peak_index = 0; peak_index < no_of_peaks; peak_index++)
    {
        OnePeak &peak_obj = peak_obj_vector[peak_index];
        int cp = no_of_copy_nos_bf_1st_peak + peak_index;
        if (_debug > 0) cerr << "\tcopy number: " << cp << endl;
        vector<int>::iterator it;
        for (it = peak_obj.segment_rc_ratio_vector.begin();
             it < peak_obj.segment_rc_ratio_vector.end(); it++)
        {
            is_ratio_looked[*it] = true;
        }
        if (peak_obj.no_of_snps <= 0) continue;
        // calculate expected maf
        vector<double> maf_expected_vector;
        for (int major_allele_cp = ceil(cp / 2.0); major_allele_cp <= cp;
             major_allele_cp++)
        {
            double maf_expected =
                    (1 - purity + major_allele_cp * purity) /
                    (2 - 2 * purity + cp * purity);
            if (maf_expected < 0.5 ||
                maf_expected == 1)
                continue;
            // logging<<maf_expected<<" "<<purity<<"\n";
            maf_expected_vector.push_back(adjust_maf_expect(
                    maf_expected, peak_obj.snp_coverage_mean,
                    peak_obj.snp_coverage_mean*_snp_coverage_var_vs_mean_ratio));
        }
        peak_obj.no_of_maf_peaks = maf_expected_vector.size();
        if (peak_obj.no_of_maf_peaks <= 0) continue;

        vector<OneSegment>::iterator one_segment_iterator =
                peak_obj.segment_obj_vector.begin();
        for (; one_segment_iterator != peak_obj.segment_obj_vector.end();
               one_segment_iterator++)
        {
            oneSegmentSNPs = one_segment_iterator->oneSegmentSNPs;
            OneSegment oneSegment = *one_segment_iterator;
            int start = oneSegment.start_pos;
            int end = oneSegment.end_pos;
            int segment_length = end - start + 1;
            _genome_len_cnv_all += segment_length;
            _genome_len_clonal += segment_length;
            cp_number_multi_len += segment_length*cp;
            cp_number_multi_len_clonal += segment_length*cp;

            int chr_integer = oneSegment.chr_index + 1;
            if (oneSegmentSNPs.no_of_snps <= 0)
                continue;
            // TODO segments with too few SNPs are skipped in this output??
            double min_diff_sq = 1.0e99;
            int best_maf_peak_index = -1;
            for (int i = 0; i < min(peak_obj.no_of_maf_peaks, 100); i++)
            {
                double diff_sq = (maf_expected_vector[i] - oneSegmentSNPs.maf_mean) *
                                 (maf_expected_vector[i] - oneSegmentSNPs.maf_mean);
                if (diff_sq < min_diff_sq)
                {
                    min_diff_sq = diff_sq;
                    best_maf_peak_index = i;
                }
            }  // find the nearest expected oneSegmentSNPs.maf_mean
            float cp_float = (oneSegment.get_rc_ratio_high_res() -
                              first_peak_obj.peak_center_int) *
                             1.0 / best_period_int +
                             no_of_copy_nos_bf_1st_peak;
            int major_allele_cp = best_maf_peak_index + (1 + cp) / 2;

            outf << chr_integer << "\t"
                 << (long)(start + CHR_ACU[chr_integer - 1]) << "\t"
                 << (long)(end + CHR_ACU[chr_integer - 1]) << "\t"
                 << cp << "\t"
                 << major_allele_cp << "\t"
                 << cp_float << "\t"
                 << oneSegment.stddev << "\t"
                 << pow(10, oneSegmentSNPs.maf_mean) << "\t"
                 << oneSegmentSNPs.maf_stddev << "\t"
                 << pow(10, maf_expected_vector[best_maf_peak_index]) << "\t"
                 << start << "\t"
                 << end
                 << endl;
        }  // each segment
    }      // each rc peak
    // subclonal regions. float copy number
    // 20171213 not sure of segment_stddev_multiplier, maybe because segment MAD was reduced in segmentation.
    for (int ratio_int = 0; ratio_int <= MAX_RATIO_RANGE_HIGH_RES; ratio_int++)
    {
        if (is_ratio_looked[ratio_int])
            continue;
        else
            is_ratio_looked[ratio_int] = true;

        int no_of_segments = _rc_ratio_segments[ratio_int].size();
        for (int k = 0; k < no_of_segments; k++)
        {
            OneSegment oneSegment = _rc_ratio_segments[ratio_int][k];
            int start = oneSegment.start_pos;
            int end = oneSegment.end_pos;
            int segment_length = end - start + 1;
            int chr_integer = oneSegment.chr_index + 1;
            double cp_float =
                    (ratio_int - first_peak_int) * 1.0 / best_period_int +
                    no_of_copy_nos_bf_1st_peak;

            _genome_len_cnv_all += segment_length;
            cp_number_multi_len += segment_length*cp_float;

            double cp_stddev = (purity * ploidy + 2 * (1 - purity)) *
                               (oneSegment.stddev * _segment_stddev_divider) / purity;
            double interval_left = cp_float - cp_stddev;
            double interval_right = cp_float + cp_stddev;
            int interval_left_int = (int)interval_left;
            int interval_right_int = (int)interval_right;
            if (interval_right_int - interval_left_int != 1)  // covers no integer or more than one integer
            {
                outf << chr_integer << "\t"
                     << (long)(start + CHR_ACU[chr_integer - 1]) << "\t"
                     << (long)(end + CHR_ACU[chr_integer - 1]) << "\t"
                     << cp_float << "\t"
                     << "NA" << "\t"
                     << "NA" << "\t"
                     << oneSegment.stddev * _segment_stddev_divider << "\t"
                     << "NA" << "\t"
                     << "NA" << "\t"
                     << "NA" << "\t" << start << "\t" << end NewLineMACRO;
                if (_debug > 0)
                {
                    out_interval << chr_integer << "\t"
                                 << start << "\t"
                                 << end << "\t"
                                 << cp_float << "\t"
                                 << cp_float << "\t"
                                 << cp_stddev << "\t"
                                 << interval_left << "\t"
                                 << interval_right << "\t"
                                 << oneSegment.stddev * _segment_stddev_divider << "\t"
                                 << oneSegment.no_of_windows NewLineMACRO;
                }
            }
            else  // covers one integer
            {
                int cp =
                        int(cp_float) + (cp_float - int(cp_float) > 0.5 ? 1 : 0);
                outf << chr_integer << "\t"
                     << (long)(start + CHR_ACU[chr_integer - 1]) << "\t"
                     << (long)(end + CHR_ACU[chr_integer - 1]) << "\t"
                     << cp << "\t"
                     << "NA" << "\t"
                     << cp_float << "\t"
                     << oneSegment.stddev * _segment_stddev_divider << "\t"
                     << "NA" << "\t"
                     << "NA" << "\t"
                     << "NA" << "\t" << start << "\t" << end NewLineMACRO;
                if (_debug > 0)
                {
                    out_interval << chr_integer << "\t"
                                 << start << "\t"
                                 << end << "\t"
                                 << cp << "\t"
                                 << cp_float << "\t"
                                 << cp_stddev << "\t"
                                 << interval_left << "\t"
                                 << interval_right << "\t"
                                 << oneSegment.stddev*_segment_stddev_divider<< "\t"
                                 << oneSegment.no_of_windows NewLineMACRO;
                }
            }
        }
    }
    _ploidy_cnv_all = cp_number_multi_len/_genome_len_cnv_all;
    _ploidy_clonal = cp_number_multi_len_clonal/_genome_len_clonal;
    outf << "#genome_len_cnv_all=" << _genome_len_cnv_all << endl;
    outf << "#genome_len_clonal=" << _genome_len_clonal << endl;
    outf << "#ploidy_cnv_all=" << _ploidy_cnv_all << endl;
    outf << "#ploidy_clonal=" << _ploidy_clonal << endl;
    outf.close();
    out_interval.close();
    cerr << "CNV output done. ploidy_cnv_all=" << _ploidy_cnv_all << " ploidy_clonal=" << _ploidy_clonal << "\n";
    return _ploidy_clonal;
}

int main(int argc, char **argv)
{
    Infer infInstance(argv[1], argv[2], argv[3], argv[4],
                      atof(argv[5]),
                      atoi(argv[6]), atof(argv[7]),
                      atoi(argv[8]),
                      atoi(argv[9]), atoi(argv[10]));
    int returnCode = infInstance.run();
    exit(returnCode);
}
