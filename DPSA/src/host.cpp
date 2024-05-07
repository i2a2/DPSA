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
#include <unistd.h>
#include "reader.h"

#include "SimpleDataProcess.h"

static const short SAMPLES_W = 375;			// n samples = 3000. 8 samples are sent at the same time (JSD204) = 600
static const short SAMPLES_P = 3000;		// SAMPLES PROCCESS
static const short SAMPLES_R = 3000;		// SAMPLES READ
static const short RESULTS_SIZE = 10;

static const short FIR_N = 20;
static const short CHANNEL = 0;
static const int BITS16 = 65536; //ADC units

const int MAX_PEAKS = 10;

static const std::string error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

typedef ap_int<128> uint128_t;

int main(int argc, char* argv[]) {
    // TARGET_DEVICE macro needs to be passed from gcc command line
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <xclbin>" << " config.ini" << std::endl;
        return EXIT_FAILURE;
    }

    std::string xclbinFilename = argv[2];

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
	int * host_baseline_ptr_w;

	// Allocate memory on the Device
    OCL_CHECK(err, cl::Buffer d_buffer_w(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, SAMPLES_W*sizeof(uint128_t), NULL, &err));
    OCL_CHECK(err, cl::Buffer d_h(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, FIR_N*sizeof(float), NULL, &err));
    OCL_CHECK(err, cl::Buffer d_baseline(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, 2*sizeof(int), NULL, &err));
    OCL_CHECK(err, cl::Buffer d_buffer_r(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, MAX_PEAKS*RESULTS_SIZE*sizeof(float), NULL, &err));

    // Set the kernel Arguments

    // Signal headers
	SP_Devices_DataBlock_Information card_header;
	SP_Devices_Monster_Data_Header record_header;

	Simple_Data_Process *sdp = new Simple_Data_Process();
	if (sdp->configure(argv[1]) != 0){
		std::cout << "Failed to configure, exit!"  << std::endl;
		exit(EXIT_FAILURE);
	}

	const SIMPLE_Channel_Analysis_Struct CA = sdp->CA[CHANNEL];

	std::string IN_FILE = sdp->input_file;
	std::string OUT_FILE = sdp->output_file;

	sdp->~Simple_Data_Process();

	/*Reader */
	 std::unique_ptr<Reader> reader(new Reader(IN_FILE));

	reader_response res = reader->ReadCardHeader(card_header);
	if(res > 0)
		std::cout << "ERROR" << std::endl;

	const float SampleRate = card_header.i64Frequency*1e-9;
	const float SampleRate_1 = 1/SampleRate;

	float offset = card_header.iOffset[0];

	const int PreTrigger_Delay = card_header.firmware[2] == 'D' ?
			-card_header.iFWDAQ_Delay :
			card_header.iFWPD_LEW[0]*SampleRate;

	float FS = card_header.FullVerticalScale[CHANNEL]/BITS16;

	float scale = CA.detection.shaping.rc_scale;
	float factor = CA.detection.cfd.factor*scale;
	float threshold = (float)CA.detection.threshold / FS;

	float rc = CA.detection.shaping.rc;

    OCL_CHECK(err, err = krnl_JESD204B_tx.setArg(1, d_buffer_w));
    OCL_CHECK(err, err = krnl_JESD204B_tx.setArg(2, SAMPLES_W));
    OCL_CHECK(err, err = krnl_JESD204B_rx.setArg(2, SAMPLES_W));

	OCL_CHECK(err, err = krnl_dpsa.setArg(1, d_baseline));
	OCL_CHECK(err, err = krnl_dpsa.setArg(2, d_h));
	OCL_CHECK(err, err = krnl_dpsa.setArg(3, factor));
	OCL_CHECK(err, err = krnl_dpsa.setArg(4, threshold));
	OCL_CHECK(err, err = krnl_dpsa.setArg(5, d_buffer_r));
	OCL_CHECK(err, err = krnl_dpsa.setArg(6, scale));
	OCL_CHECK(err, err = krnl_dpsa.setArg(7, SAMPLES_P));

	// Map OpenCL buffers to get the pointers
	OCL_CHECK(err, host_ptr_w = (uint128_t*)q_tx.enqueueMapBuffer(d_buffer_w, CL_TRUE, CL_MAP_WRITE, 0, SAMPLES_W*sizeof(uint128_t), NULL, NULL, &err));
	OCL_CHECK(err, host_h_ptr_w = (float*)q_dpsa.enqueueMapBuffer(d_h, CL_TRUE, CL_MAP_WRITE, 0, FIR_N*sizeof(float), NULL, NULL, &err));
	OCL_CHECK(err, host_baseline_ptr_w = (int*)q_dpsa.enqueueMapBuffer(d_baseline, CL_TRUE, CL_MAP_WRITE, 0, 2*sizeof(int), NULL, NULL, &err));
	OCL_CHECK(err, host_ptr_r = (float*)q_dpsa.enqueueMapBuffer(d_buffer_r, CL_TRUE, CL_MAP_READ, 0, RESULTS_SIZE*MAX_PEAKS*sizeof(float), NULL, NULL, &err));

	std::cout << "Mapped input signal buffer to host memory" << std::endl;

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
	int * baseline = (int *) host_baseline_ptr_w;
	float * data_r = (float *) host_ptr_r;

	/*Signal variables*/
	uint32_t index = 0;
	uint32_t nsamples;

	std::vector<int16_t> waveform;

	int satured;

	long unsigned int card_header_size = sizeof(card_header);
	long unsigned int record_header_size = sizeof(record_header);

	FILE *Output_fp= freopen(OUT_FILE.c_str(),"w",stdout);

	do{
		res = reader->ReadRecordHeader(nsamples, index, card_header_size, record_header_size, record_header);
		res = reader->ReadWaveform(index, waveform, nsamples, card_header_size, record_header_size);

		baseline[0] = record_header.moving_average;
		baseline[1] = 0;

		for (int i = 0; i < SAMPLES_P; i++){
			data[i] = waveform[i];
		}

		// Data will be migrated to kernel space
		OCL_CHECK(err, err = q_tx.enqueueMigrateMemObjects({d_buffer_w}, 0));
		OCL_CHECK(err, err = q_dpsa.enqueueMigrateMemObjects({d_baseline, d_h }, 0));

		// Launch the Kernel
		OCL_CHECK(err, err = q_tx.enqueueTask(krnl_JESD204B_tx));
		OCL_CHECK(err, err = q_rx.enqueueTask(krnl_JESD204B_rx));
		OCL_CHECK(err, err = q_dpsa.enqueueTask(krnl_dpsa));

		satured = record_header.status % 2;

		OCL_CHECK(err, q_dpsa.enqueueMigrateMemObjects({d_buffer_r}, CL_MIGRATE_MEM_OBJECT_HOST));

		OCL_CHECK(err, q_tx.finish());
		OCL_CHECK(err, q_rx.finish());
		OCL_CHECK(err, q_dpsa.finish());

		/*Write data to files*/

		int npeaks = data_r[NPEAKS];
		std::cout << index << ",";
		for (int peak = 0; peak < npeaks; ++peak ){
			for(int i = 0; i < RESULTS_SIZE; i++){
				if (i == SATURED)
					data_r[peak*RESULTS_SIZE + i] = satured;
				else if (i == BASELINE)
					data_r[peak*RESULTS_SIZE + i] = (data_r[peak*RESULTS_SIZE + i] + baseline[0]) * FS - offset;
				else if (i == STDBASELINE)
					data_r[peak*RESULTS_SIZE + i] = (data_r[peak*RESULTS_SIZE + i])* FS;
				else if (i == PTIME)
					data_r[peak*RESULTS_SIZE + i] = data_r[peak*RESULTS_SIZE + i] * SampleRate_1 - PreTrigger_Delay;
				else if (i >= MAX)
					data_r[peak*RESULTS_SIZE + i] *= FS;
				std::cout << data_r[peak*RESULTS_SIZE + i] << ",";
			}
		}
		std::cout << std::endl;

	} while (res == NO_ERROR);

	reader->~Reader();

	fclose(Output_fp);
	OCL_CHECK(err, err = q_tx.enqueueUnmapMemObject(d_buffer_w, host_ptr_w));
	OCL_CHECK(err, err = q_dpsa.enqueueUnmapMemObject(d_h, host_h_ptr_w));
	OCL_CHECK(err, err = q_dpsa.enqueueUnmapMemObject(d_buffer_r,host_ptr_r));

	OCL_CHECK(err, q_tx.finish());
	OCL_CHECK(err, q_rx.finish());
	OCL_CHECK(err, q_dpsa.finish());

    std::cout << "done" << std::endl;
    return (EXIT_SUCCESS);
 }
