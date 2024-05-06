/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

/*******************************************************************************
Description:
	The DPSA kernel is implemented on the application layer of the JESD204B. The kernel’s functions obtain the data buffer for the stream, calculate the baseline, filter the signal, detect the peak, and calculate the energy.

*******************************************************************************/

// Includes
#include <stdint.h>
#include <ap_int.h>
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

static void load_h_input( float* h, hls::vector<float,20>& hVector, float & h_sum, char size)
{
mem_h_rd:
  for (int i = 0; i < size; i++) {
    hVector[i] = h[i];
    h_sum += h[i];
  }
}

static void load_input( hls::stream<ap_axiu<16, 0, 0, 0> > & in_stream, int * l_in, short baseline, short & start_index, float threshold, short size)
{
#pragma HLS dataflow
  bool set_index = false;
  for (short i = 0; i < size; i++) {
read_waveform:
    ap_axiu<16, 0, 0, 0> v = in_stream.read();
    l_in[i] = v.data - baseline;
set_start_index:
    if (l_in[i] <= threshold && !set_index) {
      start_index = i - SIZE - RANGE_FROM;
      set_index = true;
    }
  }
}

static void baseline_calc( float & baseline, float & stdbaseline, float * Signal_out, int * Signal, short bs_start, short bs_end, short signal_start, short signal_end)
{
  float s_left = 0;
  float s_right = 0;
  float s2_left = 0;
  float s2_right = 0;

  short lpoints = signal_start - bs_start;
  short rpoints = bs_end - signal_end;
  short total_left = 0;
  short total_right = 0;

baseline_calc:
  while (lpoints > 0) {
    const short p = Signal[signal_start - lpoints];
    s_left += p;
    s2_left += p*p;
    lpoints--;
    total_left++;
  }
  while (rpoints > 0) {
    const short p = Signal[signal_end + rpoints];
    s_right += p;
    s2_right += p*p;
    rpoints--;
    total_right++;
  }
  const float l_baseline = (s_left + s_right)/(total_left + total_right);
  const float l_stdbaseline = sqrt((s2_left + s2_right)/(total_left + total_right) - l_baseline*l_baseline);

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
load_baseline_signal:
  for (short i = 0; i < 3*SIZE; i ++)
    Signal_out[i] = Signal[bs_start + i] - baseline;
}

static void compute_rc_cfd(float * l_in, hls::vector<float,20> & h, float h_sum, float * rc_vector, float * cfd_vector, float factor, float scale, short start_index) {
execute_fir:
  for (short n = start_index; n < start_index + 3*SIZE; n++) {
    float lsignal_sum = 0;
    float lhxin[20];
    for (char i = 0; i < FIR_N; i++) {
#pragma HLS unroll factor=20
//#pragma HLS ARRAY_PARTITION variable=l_in dim=0 cyclic complete
      lhxin[i] = l_in[n-i]*h[i];
    }
    for (char i = 0; i < FIR_N; i++)
      lsignal_sum += lhxin[i];
    float l_result = lsignal_sum/h_sum;
    rc_vector[n - start_index] = l_result * scale;
  }
execute_cfd:
  for (short i=start_index + DELAY; i < start_index + 3*SIZE; i++ )	{
    cfd_vector[i-start_index-DELAY] = rc_vector[i-start_index-DELAY] - factor*rc_vector[i-start_index];
  }
}

static void peak_detection(short & nsignals, short * pileup, float * time, float * rc_peak_signal, float * rc_vector, float * cfd_vector, int start_index, float threshold)
{
  short bthresholds[3*SIZE];
  short cfdsigns[3*SIZE];
  bool peak_detected = false;
  unsigned short index[4];
  short numberpulses = 0;

peak_detection:
  for(int i = 0; i < 3*SIZE; i++) {

    unsigned char belowth=0;
    unsigned char belowcfd=0;
    float tmprc=rc_vector[i];
    float tmpcfd=cfd_vector[i];

    if (tmprc < threshold) {
      bthresholds[i]=-1;
      belowth=1;
    }
    else {
      bthresholds[i]=1;
      belowth=0;
      peak_detected = false;
    }
    if (tmpcfd < 0) {
      cfdsigns[i]=-1;
      belowcfd=1;
    }
    else {
      cfdsigns[i]=1;
      belowcfd=0;
    }

    if (belowth && belowcfd && !peak_detected) {

      peak_detected = true;

      index[numberpulses]=i;
      numberpulses++;

      if (numberpulses > 1) {
        if(index[numberpulses-1]-index[numberpulses-2] < SIZE) { //index[numberpulses -1] is current index 'cause numberpulses increase after index assigment
          numberpulses--;
          pileup[numberpulses] |= 2;
        }
      }
    }
  }

  nsignals = numberpulses;
zero_cross_detection:
  float zero_cross[10];
  if (numberpulses >= 1) {

    for (short pulse = 0; pulse < numberpulses; pulse++ ) {

      do{
        --index[pulse];
      } while (cfdsigns[index[pulse]] < 0); //index will point to a positive value of cfdsignal

      zero_cross[pulse] = index[pulse] + (cfd_vector[index[pulse]])/(cfd_vector[index[pulse]+1]-cfd_vector[index[pulse]]);
      time[pulse] = zero_cross[pulse] + start_index - 2;

      for(short i = index[pulse] - RANGE_FROM; i <= index[pulse] + RANGE_TO;i++ ) {
        rc_peak_signal[i - (index[pulse] - RANGE_FROM)] = rc_vector[i];
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
  float energ_3 = 0;

  for(short i = 0; i < SIZE; i++ ) {
#pragma HLS unroll
    ampli = SLOPE*rc_peak_signal[i];
    emax = ampli > emax ? ampli : emax;
  }

  for (short i = 27; i < 347; i++) {
#pragma HLS unroll
    energ_1 += SLOPE*rc_peak_signal[i];
  }

  for (short i = 27; i < 67; i++) {
#pragma HLS unroll
    energ_2 += SLOPE*rc_peak_signal[i];
  }

  for (short i = 67; i < SIZE; i++) {
#pragma HLS unroll
    energ_3 += SLOPE*rc_peak_signal[i];
  }

  energies_buf[0] = emax;
  energies_buf[1] = energ_1;
  energies_buf[2] = energ_2;
  energies_buf[3] = energ_3;
}

static void store_results( float* results, short * pileup, short nsignals, float baseline_0, float stdbaseline_0, float * time, float * energies_0 )
{
mem_record_result_wr:
	results[0] = pileup[0];
	results[1] = 1;
	results[2] = nsignals;
	results[3] = baseline_0;
	results[4] = stdbaseline_0;
	results[5] = time[0];
	results[6] = energies_0[0];
	results[7] = energies_0[1];
	results[8] = energies_0[2];
	results[9] = energies_0[3];

}

extern "C" {
  void krnl_dpsa( hls::stream<ap_axiu<16, 0, 0, 0> > &ln0, short * baseline, float * h, float factor, float threshold, float * result, float scale, short size) {

#pragma HLS INTERFACE m_axi port = h bundle = gmem1
#pragma HLS INTERFACE m_axi port = result bundle = gmem0

    // vectors
    float rc_vector[3*SIZE];
    float cfd_vector[3*SIZE];

    // AREA OF INTEREST
    float rc_peak_signal[SIZE];

    hls::vector<float,20> h_vector;
    float h_sum = 0;
    int l_in_0[DATA_SIZE];
    float l_float_0[3*SIZE];

    // Record_Results
    short nsignals;

    // Signal_Results
    short pileup[4];
    float time[4];
    float energy[4];
    short start_index;
    float baseline_calculated;
    float stdbaseline;

#pragma HLS dataflow

    load_h_input(h, h_vector, h_sum, FIR_N);

    load_input(ln0, l_in_0,baseline[0], start_index, threshold, size);

    const short bs_end = start_index + 3*SIZE;
    const short sig_start = start_index + SIZE;
    const short sig_end = start_index + 2*SIZE;

    baseline_calc(baseline_calculated, stdbaseline, l_float_0, l_in_0, start_index, bs_end, sig_start, sig_end);

    compute_rc_cfd(l_float_0, h_vector, h_sum, rc_vector, cfd_vector, factor, scale, 0);

    peak_detection(nsignals, pileup, time, rc_peak_signal, rc_vector, cfd_vector, start_index, threshold);

    energies_calculation(rc_peak_signal, energy);

    store_results(result, pileup, nsignals, baseline_calculated, stdbaseline, time, energy);
  }
}
