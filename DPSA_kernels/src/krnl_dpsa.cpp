/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

/*******************************************************************************
Description:
	The DPSA kernel is implemented on the application layer of the JESD204B. The kernel’s functions obtain the data buffer for the stream, calculate the baseline, filter the signal, detect the peak, and calculate the energy.

*******************************************************************************/

template <class T>
  T REG(T in )
{
  #pragma HLS PIPELINE
  #pragma HLS INLINE off
  #pragma HLS LATENCY min=1 max=1

  return in;
}

// Includes
#include <stdint.h>
#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <hls_vector.h>
#include <hls_math.h>

#define DATA_SIZE 3000
#define FIR_N 20
#define CHANNELS 3
#define DELAY 10

#define SLOPE -1

#define RANGE_FROM 50
#define RANGE_TO 300
#define SIZE 350

#define BS 0
#define OF 1
#define TH 2

#define MAX_PEAKS 10
#define RESULT_SIZE 10

#define PILEUP 0
#define SATURED 1
#define NPEAKS 2
#define BASELINE 3
#define STDBASELINE 4
#define PTIME 5
#define MAX 6
#define EN 7
#define EN1 8
#define EN2 9

typedef ap_uint<128> uint128_t;

// TRIPCOUNT identifier
const int c_size =SIZE;

static void load_h_input(float* h, hls::vector<float,20>& hVector, float & h_sum, int size)
{
mem_h_rd:
    for (int i = 0; i < size; i++) {
        hVector[i] = h[i];
        h_sum += h[i];
    }
}

static void load_input(hls::stream<ap_axis<16, 0, 0, 0> > & in_stream, int * l_in, int baseline, short & npeaks, int * start_index, float threshold, short size)
{
#pragma HLS dataflow
	bool set_index = false;
	for (int i = 0; i < size; i++) {
read_waveform:
		ap_axis<16, 0, 0, 0> v = in_stream.read();
		l_in[i] = v.data - baseline;
set_start_index:
		if (l_in[i] < threshold && !set_index) {
			start_index[npeaks] = i - SIZE - RANGE_FROM;
			set_index = true;
			++npeaks;
		}
		if(set_index and (npeaks >= 1)){
			if ((i - start_index[npeaks - 1] - SIZE - RANGE_FROM) > 3*SIZE){
				set_index = false;
			}
		}
    }
}

static void baseline_calc(float & baseline, float & stdbaseline, float * Signal_out, int * Signal, int bs_start, int bs_end, int pulse_start, int pulse_end)
{
	float s_left = 0;
	float s_right = 0;
	float s2_left = 0;
	float s2_right = 0;

	int lpoints = pulse_start - bs_start;
	int rpoints = bs_end - pulse_end;
	int total_left = 0;
	int total_right = 0;

#pragma BASELINE CALC
	while (lpoints > 0)	{
		float p = Signal[pulse_start - lpoints];
		s_left += p;
		s2_left += p*p;
		lpoints--;
		total_left++;
	}
	while (rpoints > 0) {
		float p = Signal[pulse_end + rpoints];
		s_right += p;
		s2_right += p*p;
		rpoints--;
		total_right++;
	}

	float l_baseline = (s_left + s_right)/(total_left + total_right);
	float l_stdbaseline = sqrt((s2_left + s2_right)/(total_left + total_right) - l_baseline*l_baseline);

	s2_left *= total_left;
	s2_left -= s_left*s_left;

	s2_right *= total_right;
	s2_right -= s_right*s_right;

	if (total_right != 0 && total_left != 0) {
		float var_left = sqrt(s2_left)/total_left;
		float var_right = sqrt(s2_right)/total_right;
		float baseline_left = s_left / total_left;
		float baseline_right = s_right / total_right;

		if (var_left < var_right) {
			if (fabs(baseline_right - baseline_left) < 3 * var_left) {
				baseline = l_baseline;
				stdbaseline = l_stdbaseline;
			}
			else {
				baseline = baseline_left;
				stdbaseline = var_left;
			}
		}
		else {
			if (fabs(baseline_right - baseline_left) < 3 * var_right) {
				baseline = l_baseline;
				stdbaseline = l_stdbaseline;
			}
			else {
				baseline = baseline_right;
				stdbaseline = var_right;
			}
		}
	}
	else if (total_left) {
		stdbaseline = sqrt(s2_left)/total_left;
		baseline = s_left / total_left;
	}
	else if (total_right) {
		stdbaseline = sqrt(s2_right)/total_right;
		baseline = s_right / total_right;
	}
	else {
		stdbaseline = 0;
		baseline = 0;
	}
#pragma LOAD Baseline signal
	for (int i = 0; i < 3*SIZE; i ++)
		Signal_out[i] = Signal[bs_start + i] - baseline;

}

static void compute_rc_cfd(float * l_in, hls::vector<float,20> & h, float h_sum, float * rc_vector, float * cfd_vector, float factor, float scale)
{
execute_fir_cfd:
	for (int n = 0; n < 3*SIZE; ++n) {
#pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size

		float lsignal_sum = 0;
		float lhxin[20];
		for (int i = 0; i < FIR_N; i++) {
#pragma HLS unroll factor=20
#pragma HLS ARRAY_PARTITION variable=l_in dim=1 complete
			lhxin[i] = l_in[n-i]*h[i];
		}
		for (int i = 0; i < FIR_N; i++){
#pragma HLS unroll factor=20
#pragma HLS ARRAY_PARTITION variable=lhxin dim=1 complete
			lsignal_sum += lhxin[i];
		}
		float l_result = lsignal_sum/h_sum;
		rc_vector[n] = l_result*scale;
	}
execute_cfd:
	for (int i=DELAY; i < 3*SIZE; i++ )	{
		cfd_vector[i-DELAY] = rc_vector[i-DELAY] - factor*rc_vector[i];
	}
}

static void peak_detection(short &pileup, float &time, float * rc_peak_signal, float * no_shape_signal, float * rc_vector, float * cfd_vector, int start_index, float threshold)
{
	short bthresholds[3*SIZE];
	short cfdsigns[3*SIZE];
	bool peak_detected = false;
	unsigned int index[MAX_PEAKS];
	short numberpulses = 0;

peak_detection:
	for(int i = 0; i < 3*SIZE; i++) {

		bool belowth = false;
		float tmprc=rc_vector[i];
		float tmpcfd=cfd_vector[i];

		if (tmprc < threshold) {
			bthresholds[i]=-1;
			belowth = true;
		} else {
			bthresholds[i] = 1;
			belowth = false;
			peak_detected = false;
		}
		if (tmpcfd < 0) {
			cfdsigns[i] = -1;
		} else {
			cfdsigns[i] = 1;
		}

		if (belowth && !peak_detected) 	{

			peak_detected = true;

			index[numberpulses]=i;
			++numberpulses;

			if (numberpulses >= 2) {
				if((index[numberpulses - 1]-index[numberpulses - 2]) <= SIZE) {
					//PILEUP LEFT
					--numberpulses;
					pileup |= 2;
				}
				if(numberpulses >= 2 and (index[numberpulses]-index[numberpulses-1]) <= SIZE){
					//PILEUP RIGHT
					--numberpulses;
					pileup |= 1;
				}
			} else {
				pileup = 0;
			}
		}
	}

zero_cross_detection:
	float zero_cross[MAX_PEAKS];
	if (numberpulses >= 1) {

		for (int pulse = 0; pulse < numberpulses; pulse++ ) {

			if((cfdsigns[index[pulse]] == -1)){
				do {
					--index[pulse];
				} while(cfdsigns[index[pulse]] == -1);
			} else {
				while (cfdsigns[index[pulse]] == 1)
					++index[pulse];
				--index[pulse];
			}
			// X = -b(x2-x1)/(y2-y1) = -Y/(y2-y1) = -y1/(y2-y1)
			zero_cross[pulse] = index[pulse] - (cfd_vector[index[pulse]])/(cfd_vector[index[pulse]+1]-cfd_vector[index[pulse]]);

			time = zero_cross[pulse] + start_index;

			for(int i = index[pulse] - RANGE_FROM; i <= index[pulse] + RANGE_TO; ++i) {
				rc_peak_signal[i - (index[pulse] - RANGE_FROM)] = no_shape_signal[i];
			}
		}
	}
}

static void energies_calculation(float * rc_peak_signal, float * energies_buf)
{
	float ampli = 0;
	float emax = 0;
	float energ_1 = 0;
	float energ_2 = 0;

	// MAX VALUE
	for(short i = 0; i < RANGE_TO; i++ ) {
#pragma HLS unroll
		ampli = rc_peak_signal[i];
		emax = ampli < emax ? ampli : emax;
	}

	// PROMPR CHARGE
	for (short i = 30; i <= (RANGE_FROM + 20); i++) {
#pragma HLS unroll
		energ_1 += SLOPE*rc_peak_signal[i];
	}

	// DELAY CHARGE
	for (int i = RANGE_FROM + 20; i <= RANGE_TO; i++) {
#pragma HLS unroll
		energ_2 += SLOPE*rc_peak_signal[i];
	}

	energies_buf[0] = SLOPE*emax;
	energies_buf[1] = energ_1 + energ_2;
	energies_buf[2] = energ_1;
	energies_buf[3] = energ_2;

}

static void store_results(float* results, short * pileup, short npeaks, float * baseline, float * stdbaseline, float * time, float energies[MAX_PEAKS][4])
{
mem_record_result_wr:
	for(short i = 0; i < npeaks; ++i ){
		results[PILEUP + RESULT_SIZE*i] = pileup[i];
		results[SATURED + RESULT_SIZE*i] = 0;
		results[NPEAKS + RESULT_SIZE*i] = npeaks;
		results[BASELINE + RESULT_SIZE*i] = baseline[i];
		results[STDBASELINE + RESULT_SIZE*i] = stdbaseline[i];
		results[PTIME + RESULT_SIZE*i] = time[i];
		results[MAX + RESULT_SIZE*i] = energies[i][0];
		results[EN + RESULT_SIZE*i] = energies[i][1];
		results[EN1 + RESULT_SIZE*i] = energies[i][2];
		results[EN2 + RESULT_SIZE*i] = energies[i][3];
	}
	for(short i = npeaks; i < MAX_PEAKS; ++i ){
		results[PILEUP + RESULT_SIZE*i] = 0;
		results[SATURED + RESULT_SIZE*i] = 0;
		results[NPEAKS + RESULT_SIZE*i] = 0;
		results[BASELINE + RESULT_SIZE*i] = 0;
		results[STDBASELINE + RESULT_SIZE*i] = 0;
		results[PTIME + RESULT_SIZE*i] = 0;
		results[MAX + RESULT_SIZE*i] = 0;
		results[EN + RESULT_SIZE*i] = 0;
		results[EN1 + RESULT_SIZE*i] = 0;
		results[EN2 + RESULT_SIZE*i] = 0;
	}
}

extern "C" {
void krnl_dpsa(hls::stream<ap_axis<16, 0, 0, 0> > &ln0, int * waveform_args, float * h, float factor, float threshold, float * result, float scale, short size)
{
#pragma HLS INTERFACE m_axi port = result bundle = gmem0
#pragma HLS INTERFACE m_axi port = waveform_args bundle = gmem2
#pragma HLS INTERFACE m_axi port = h bundle = gmem1

	float rc_vector[MAX_PEAKS][3*SIZE], cfd_vector[MAX_PEAKS][3*SIZE], rc_peak_signal[MAX_PEAKS][SIZE], l_float[MAX_PEAKS][3*SIZE];
    hls::vector<float,20> h_vector;
    float h_sum = 0;
    int l_in[DATA_SIZE];
    short npeaks = 0;
    short pileup[MAX_PEAKS];
    float time[MAX_PEAKS];
    float energy[MAX_PEAKS][4];
    int start_index[MAX_PEAKS];
    float baseline_calculated[MAX_PEAKS], stdbaseline[MAX_PEAKS];
    int lbaseline = waveform_args[BS], loffset = RESULT_SIZE*waveform_args[OF];

#pragma HLS dataflow

    load_h_input(h, h_vector, h_sum, FIR_N);

    load_input(ln0, l_in, lbaseline, npeaks, start_index, threshold, size);

	for(short i = 0; i < npeaks; i++){
		int bs_end = start_index[i] +3*SIZE;
		int pulse_start = start_index[i] + SIZE;
		int pulse_end = start_index[i] + 2*SIZE;

		baseline_calc( baseline_calculated[i], stdbaseline[i], l_float[i], l_in, start_index[i], bs_end, pulse_start, pulse_end);

		compute_rc_cfd(	l_float[i], h_vector, h_sum, rc_vector[i], cfd_vector[i], factor, scale);

		peak_detection(	pileup[i], time[i], rc_peak_signal[i], l_float[i], rc_vector[i], cfd_vector[i], start_index[i],threshold);

		energies_calculation( rc_peak_signal[i], energy[i]);

	}

	store_results(result + loffset, pileup, npeaks, baseline_calculated, stdbaseline, time, energy);
	}
}
