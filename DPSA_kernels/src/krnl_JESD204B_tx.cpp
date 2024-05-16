/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

/*******************************************************************************
Description:
          The JESD20B_TX kernel, which is a component of the JESDB204B transport layer, is utilized to stream data and emulate a DAC

*******************************************************************************/
#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

typedef ap_uint<128> uint128_t;

#define DATA_SIZE 375

// TRIPCOUNT identifier
const int c_size = DATA_SIZE;

extern "C" {
	void krnl_JESD204B_tx(
			hls::stream<uint128_t> &outStream,
			uint128_t * in,
			short size
			){

#pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
#pragma HLS INTERFACE axis port=outStream

		/*Write to AXIS*/
		for(int i = 0; i < size; i++){
#pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
			outStream << in[i];
		}
	}
}
