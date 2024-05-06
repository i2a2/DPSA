/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

#define DATA_SIZE 375

typedef ap_uint<128> uint128_t;

// TRIPCOUNT identifier
const int c_size = DATA_SIZE;

extern "C" {
	void krnl_JESD204B_rx(
			hls::stream<uint128_t> &inStream,
			hls::stream<ap_axiu<16, 0, 0, 0> > &outStream_ln0,
			int size
			){

#pragma HLS INTERFACE axis port=inStream depth=512
#pragma HLS dataflow

		for (int i = 0; i < size; i++){

#pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size

			uint128_t v = inStream.read();

			ap_axiu<16, 0, 0, 0> v_0_ln0;
			ap_axiu<16, 0, 0, 0> v_1_ln0;
			ap_axiu<16, 0, 0, 0> v_2_ln0;
			ap_axiu<16, 0, 0, 0> v_3_ln0;
			ap_axiu<16, 0, 0, 0> v_4_ln0;
			ap_axiu<16, 0, 0, 0> v_5_ln0;
			ap_axiu<16, 0, 0, 0> v_6_ln0;
			ap_axiu<16, 0, 0, 0> v_7_ln0;

			v_0_ln0.data = v.range(15,0);
			v_1_ln0.data = v.range(31,16);
			v_2_ln0.data = v.range(47,32);
			v_3_ln0.data = v.range(63,48);
			v_4_ln0.data = v.range(79,64);
			v_5_ln0.data = v.range(95,80);
			v_6_ln0.data = v.range(111,96);
			v_7_ln0.data = v.range(127,112);

			outStream_ln0 << v_0_ln0;
			outStream_ln0 << v_1_ln0;
			outStream_ln0 << v_2_ln0;
			outStream_ln0 << v_3_ln0;
			outStream_ln0 << v_4_ln0;
			outStream_ln0 << v_5_ln0;
			outStream_ln0 << v_6_ln0;
			outStream_ln0 << v_7_ln0;


		}
	}
}
