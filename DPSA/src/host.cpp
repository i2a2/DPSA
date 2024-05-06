/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#define OCL_CHECK(error, call)                                                                   \
    call;                                                                                        \
    if (error != CL_SUCCESS) {                                                                   \
        printf("%s:%d Error calling " #call ", error code is: %d\n", __FILE__, __LINE__, error); \
        exit(EXIT_FAILURE);                                                                      \
    }

#include "host.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <ap_int.h>
#include <ap_axi_sdata.h>

#include "reader.h"

#define SAMPLES_W 375		// n samples = 3000. 8 samples are sent at the same time (JSD204) = 600
#define SAMPLES_P 3000
#define FIR_N 20
#define SAMPLES_R 3000
#define CHANNELS 1
#define RESULTS_SIZE 10

#define BITS16 65536 //ADC units

#define IN_FILE "/home/resources/monster0.bin"

static const int DATA_SIZE = 4096;

static const std::string error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

typedef ap_int<128> uint128_t;
typedef ap_int<256> uint256_t;

int main(int argc, char* argv[]) {
    // TARGET_DEVICE macro needs to be passed from gcc command line
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <xclbin>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string xclbinFilename = argv[1];

    /*Signal headers*/
    SP_Devices_DataBlock_Information card_header;
    SP_Devices_Monster_Data_Header record_header[4];

    // Creates a vector of DATA_SIZE elements with an initial value of 10 and 32
    // using customized allocator for getting buffer alignment to 4k boundary

    std::vector<cl::Device> devices;
    cl_int err;
    cl::Context context;
    cl::CommandQueue q_tx, q_rx, q_dpsa;
    cl::Kernel krnl_JESD204B_tx, krnl_JESD204B_rx, krnl_dpsa;
    cl::Program program;
    std::vector<cl::Platform> platforms;
    bool found_device = false;

    // traversing all Platforms To find Xilinx Platform and targeted
    // Device in Xilinx Platform
    cl::Platform::get(&platforms);
    for (size_t i = 0; (i < platforms.size()) & (found_device == false); i++) {
        cl::Platform platform = platforms[i];
        std::string platformName = platform.getInfo<CL_PLATFORM_NAME>();
        if (platformName == "Xilinx") {
            devices.clear();
            platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
            if (devices.size()) {
                found_device = true;
                break;
            }
        }
    }
    if (found_device == false) {
        std::cout << "Error: Unable to find Target Device " << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "INFO: Reading " << xclbinFilename << std::endl;
    FILE* fp;
    if ((fp = fopen(xclbinFilename.c_str(), "r")) == nullptr) {
        printf("ERROR: %s xclbin not available please build\n", xclbinFilename.c_str());
        exit(EXIT_FAILURE);
    }
    // Load xclbin
    std::cout << "Loading: '" << xclbinFilename << std::endl;;
    std::ifstream bin_file(xclbinFilename, std::ifstream::binary);
    bin_file.seekg(0, bin_file.end);
    unsigned nb = bin_file.tellg();
    bin_file.seekg(0, bin_file.beg);
    char* buf = new char[nb];
    bin_file.read(buf, nb);

    // Creating Program from Binary File
    cl::Program::Binaries bins;
    bins.push_back({buf, nb});
    bool valid_device = false;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
        OCL_CHECK(err, q_tx = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
        OCL_CHECK(err, q_rx = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
        OCL_CHECK(err, q_dpsa = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, nullptr, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            OCL_CHECK(err, krnl_JESD204B_tx = cl::Kernel(program, "krnl_JESD204B_tx", &err));
            OCL_CHECK(err, krnl_JESD204B_rx = cl::Kernel(program, "krnl_JESD204B_rx", &err));
            OCL_CHECK(err, krnl_dpsa = cl::Kernel(program, "krnl_dpsa", &err));
            valid_device = true;
            std::cout << "Device[" << i << "]: program successful!" << std::endl;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!"  << std::endl;
        exit(EXIT_FAILURE);
    }

    /*Allocate host memory*/
	uint128_t * host_ptr_w;
	float * host_h_ptr_w;
	float * host_ptr_r;
	short * host_baseline_ptr_w;

	// These commands will allocate memory on the Device. The cl::Buffer objects can
    // be used to reference the memory locations on the device.
	// Signal
    OCL_CHECK(err, cl::Buffer d_buffer_w(context, CL_MEM_READ_ONLY, SAMPLES_W*sizeof(uint128_t), NULL, &err));

    // FIR H
    OCL_CHECK(err, cl::Buffer d_h(context, CL_MEM_READ_ONLY, FIR_N*sizeof(float), NULL, &err));

    // BASELINEE
    OCL_CHECK(err, cl::Buffer d_baseline(context, CL_MEM_READ_ONLY, CHANNELS*sizeof(short), NULL, &err));

    // Result
    OCL_CHECK(err, cl::Buffer d_buffer_r(context, CL_MEM_WRITE_ONLY, CHANNELS*RESULTS_SIZE*sizeof(float), NULL, &err));

    // set the kernel Arguments

    OCL_CHECK(err, err = krnl_JESD204B_tx.setArg(1, d_buffer_w));
    OCL_CHECK(err, err = krnl_JESD204B_tx.setArg(2, SAMPLES_W));

    OCL_CHECK(err, err = krnl_JESD204B_rx.setArg(2, SAMPLES_W));

    // FIXME
    //TODO check scale
    float factor = 0.492000014;
    float threshold = -211.34653103643751;
    float scale = 1.64;

    short size_p = SAMPLES_P;

    /*Signal variables*/
	uint32_t index = 0;
	uint32_t nsamples[CHANNELS];

	std::vector<short> waveform[CHANNELS];

	/*Reader */
	Reader reader;

	reader_response res = reader.ReadCardHeader(IN_FILE, card_header);
	if(res > 0)
		std::cout << "ERROR" << std::endl;

	// We then need to map our OpenCL buffers to get the pointers
	OCL_CHECK(err, host_ptr_w = (uint128_t*)q_tx.enqueueMapBuffer(d_buffer_w, CL_TRUE, CL_MAP_WRITE, 0, SAMPLES_W*sizeof(uint128_t), NULL, NULL, &err));

	OCL_CHECK(err, host_baseline_ptr_w = (short*)q_dpsa.enqueueMapBuffer(d_baseline, CL_TRUE, CL_MAP_WRITE, 0, CHANNELS*sizeof(short), NULL, NULL, &err));
	OCL_CHECK(err, host_h_ptr_w = (float*)q_dpsa.enqueueMapBuffer(d_h, CL_TRUE, CL_MAP_WRITE, 0, FIR_N*sizeof(float), NULL, NULL, &err));

	std::cout << "Mapped input signal buffer to host memory" << std::endl;

	/*TODO: READ DEVICE PARAMETERS*/
	float rc = 6.0;

	/* FIR coefficients */
	const float b = exp(-1/rc);
	const float a = 1 - b;

	float * h = (float *) host_h_ptr_w;

	for (unsigned int i=0; i<20; i++)
	{
		h[i] = a*pow(b,i);
	}

	/*Get waveforms from file and fill host buffer*/
	short * data = (short *) host_ptr_w;
	short * baseline = (short *) host_baseline_ptr_w;;

	// HERE: Read more than one signal
	for(long unsigned int i=0; i<CHANNELS; i++) {
		res = reader.ReadRecordHeader(IN_FILE, nsamples[i], index, sizeof(card_header), sizeof(record_header[0]), record_header[i]);
		res = reader.ReadWaveform(IN_FILE, index, waveform[i], nsamples[i], sizeof(card_header), sizeof(record_header[0]));

		baseline[i] = record_header[i].moving_average;

		//index data
		int read_i = 0;//i*5;

		for (long unsigned int j = 0; j < SAMPLES_P; j++){
			//if (j%5 == 0 && j>0) read_i+=11;
			data[read_i] = waveform[i][j];
			read_i++;
		}
	}

	OCL_CHECK(err, err = krnl_dpsa.setArg(1, d_baseline));
	OCL_CHECK(err, err = krnl_dpsa.setArg(2, d_h));
	OCL_CHECK(err, err = krnl_dpsa.setArg(3, factor));
	OCL_CHECK(err, err = krnl_dpsa.setArg(4, threshold));
	OCL_CHECK(err, err = krnl_dpsa.setArg(5, d_buffer_r));
	OCL_CHECK(err, err = krnl_dpsa.setArg(6, scale));
	OCL_CHECK(err, err = krnl_dpsa.setArg(7, size_p));

	// Launch the Kernel
	OCL_CHECK(err, err = q_tx.enqueueTask(krnl_JESD204B_tx));
	OCL_CHECK(err, err = q_rx.enqueueTask(krnl_JESD204B_rx));
	OCL_CHECK(err, err = q_dpsa.enqueueTask(krnl_dpsa));

	std::cout << "Launched kernels!" << std::endl;

	// The result of the previous kernel execution will need to be retrieved in
	// order to view the results. This call will transfer the data from FPGA to
	// source_results vector

	OCL_CHECK(err, host_ptr_r = (float*)q_dpsa.enqueueMapBuffer(d_buffer_r, CL_TRUE, CL_MAP_READ, 0, CHANNELS*RESULTS_SIZE*sizeof(float), NULL, NULL, &err));

	std::cout << "Mapped output signal buffer to host memory" << std::endl;

	float * data_r = (float *) host_ptr_r;

	//SIMPLE_Signal_Results signal_results;

	const double SampleRate = card_header.i64Frequency*1e-9;
	const double SampleRate_1 = 1/SampleRate;

	/*Write data to files*/
	for(long unsigned int j = 0; j < CHANNELS; j++){

		double FS = card_header.FullVerticalScale[j]/BITS16;
		double offset = card_header.iOffset[j];

		const int PreTrigger_Delay = card_header.firmware[2] == 'D' ?
				-card_header.iFWDAQ_Delay :
				card_header.iFWPD_LEW[j]*SampleRate;


		for(long unsigned int i = 0; i < RESULTS_SIZE; i++){
				if (i == 3) //TODO: ENUM
					data_r[j*RESULTS_SIZE + i] = (data_r[j*RESULTS_SIZE + i] + baseline[j]) * FS - offset;
				else if (i == 4)
					data_r[j*RESULTS_SIZE + i] = (data_r[j*RESULTS_SIZE + i])* FS;
				else if (i == 5)
					data_r[j*RESULTS_SIZE + i] = data_r[j*RESULTS_SIZE + i] * SampleRate_1 - PreTrigger_Delay;
				else if (i >= 6)
					data_r[j*RESULTS_SIZE + i] *= FS;
				std::cout << data_r[j*RESULTS_SIZE + i] << "-";
			}
			std::cout << std::endl;
	}

	OCL_CHECK(err, q_tx.finish());
	OCL_CHECK(err, q_rx.finish());
	OCL_CHECK(err, q_dpsa.finish());

    std::cout << "done" << std::endl;
    return (EXIT_SUCCESS);
 }
