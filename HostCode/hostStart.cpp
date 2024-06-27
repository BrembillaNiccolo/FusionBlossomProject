#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <unistd.h>
#include <assert.h>
#include <chrono>
#include <math.h>
#include <vector>

#include "common/xcl2.hpp"

#include "ap_int.h"
#define BITACCURACY 9 //2^(BITACCURACY-1) > 2* MAXELEMENTS^2
#define hlsboolACCURACY 1

typedef ap_int<BITACCURACY> hlsint;
typedef ap_uint<hlsboolACCURACY> hlsbool;

#define MAXELEMENTS 9
#define NORTH 0
#define EAST 2
#define SOUTH 3
#define WEST 1
#define MAXWEIGHT 1000
#define STRONG 1
#define WEAK 0
#define MAXTOUCHES 2
#define MAX_QUEUE_SIZE 8 //4
#define DEBUG 0
#define MAXDEFECTEDNODES 2*MAXELEMENTS //3*MAXELEMENTS
#define MAXBLOSSOMS (MAXELEMENTS+1)/2 //MAXELEMENTS
#define MAXBLOSSOMSDIMENSION 3 //5
#define MAXCANTOUCHES 3 //5
#define MAXNODESCOUNT 2*MAXELEMENTS
#define MAXEDGES 2*(MAXELEMENTS*MAXELEMENTS)

typedef struct pairDefectedNode
{
	short node1;
	short node2;
} PairDefectedNode;
float startExecution(cl::CommandQueue& q, cl::Kernel& decorderQEC, hlsint numNodes, hlsint dim, cl::Buffer& defNodes,cl::Buffer& pairs_out ,cl::Buffer& pairsCount_out, double totalClocks[])
{
	cl_int err;
	
    OCL_CHECK(err, err = decorderQEC.setArg(2, defNodes));
    OCL_CHECK(err, err = decorderQEC.setArg(3, pairs_out));
    OCL_CHECK(err, err = decorderQEC.setArg(4, pairsCount_out));
	OCL_CHECK(err, err = decorderQEC.setArg(0, numNodes));
	OCL_CHECK(err, err = decorderQEC.setArg(1, dim));


    // Data will be migrated to kernel space
    q.enqueueMigrateMemObjects({defNodes}, 0); /*0 means from host*/
	q.finish();

	auto start=std::chrono::high_resolution_clock::now();

	//Launch the Kernel
	q.enqueueTask(decorderQEC);
	q.finish();
	
	auto stop=std::chrono::high_resolution_clock::now();

    auto duration=std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start);

    printf("Operation concluded in %f nanoseconds\n", (float)duration.count());
	totalClocks[0]=	(double)duration.count();
	//Data from Kernel to Host
	q.enqueueMigrateMemObjects({pairs_out, pairsCount_out},CL_MIGRATE_MEM_OBJECT_HOST);
	q.finish();
	
	return (float)duration.count();
}

int main(int argc, char* argv[]){
	
	//TARGET_DEVICE macro needs to be passed from gcc command line
	if(argc != 3) {
		std::cout << "Usage: " << argv[0] <<" <xclbin> <dataset path>" << std::endl;
		return EXIT_FAILURE;
	}

    FILE* inputFile = fopen(argv[2], "r");
    //Frequency in MegaHZ
    double frequency = 154;


	double decodeAVG = 0;
	double clocksAVG = 0;
	double totalClocks[1];
	totalClocks[0]=0;
	std::vector<hlsint> nDefNodes(1);
	std::vector<hlsint> dimension(1);
	std::vector<hlsint> defectedNodes(MAXDEFECTEDNODES);
	std::vector<PairDefectedNode> pairs(MAXDEFECTEDNODES);
	std::vector<hlsint> pairsCount(1);

    //size_t nDefNodes_size = sizeof(hlsint);
    //size_t dimension_size = sizeof(hlsint);
	size_t defectedNodes_size = sizeof(hlsint) * MAXDEFECTEDNODES;
	size_t pairs_out_size = sizeof(PairDefectedNode) * MAXDEFECTEDNODES;
    size_t pairsCount_out_size = sizeof(hlsint);


	
    

/*
================================================================================================================================
	OPENCL STUFF
================================================================================================================================
*/

   	std::string binaryFile = argv[1];

    
    cl_int err;
    cl::Context context;
    cl::Kernel decorderQEC;
    cl::CommandQueue q;

    auto devices = xcl::get_xil_devices();

    auto fileBuf = xcl::read_binary_file(binaryFile);

    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};

    bool valid_device = false;

    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
        OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, nullptr, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            std::cout << "Device[" << i << "]: program successful!\n";
            OCL_CHECK(err, decorderQEC= cl::Kernel(program, "resolve", &err));
            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }
    //to Modify
    // cl::Buffer buffer_nDefNodes(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, nDefNodes_size, nDefNodes.data());
    // cl::Buffer buffer_dimension(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, dimension_size, dimension.data());
    cl::Buffer buffer_defectedNodes(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, defectedNodes_size, defectedNodes.data());
    cl::Buffer pairs_out_buf(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, pairs_out_size, pairs.data());
    cl::Buffer pairsCount_out_buf(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, pairsCount_out_size, pairsCount.data());

/*
================================================================================================================================
	MY STUFF
================================================================================================================================
*/
	float clocksAVGArr[200] = {0}; 

	int totalDecodes = 0;

	if (inputFile == NULL) {
		printf("Failed to open the input file.\n");
		return 1;
	}
	printf("File opened successfully.\n");

	int rounds;
	int nNodes=0;
	fscanf(inputFile, "%d", &rounds);
	for (int i = 0; i < rounds; i++) {
		int d;
		fscanf(inputFile, "%d", &d);
		nDefNodes[0] = 0;
		nNodes=0;
		char c;
		int val = 0;
		//read until the end of the line
		//the line is number space number \n read all the numbers until the end of the line
		do{
		c = fgetc(inputFile);
		}while(c!='\n' && c!='\r');
		c = fgetc(inputFile);
		while (c != '\n' && c!='\n')
		{
			if (c == ' ' &&  val!=0)
			{
				defectedNodes[nNodes] = (hlsint)val;
				val = 0;
				nDefNodes[0]++;
				nNodes++;
			}
			else 
			{
				val = val * 10 + (c - '0');
				
			}
			c = fgetc(inputFile);
		}
		printf("actualRound %d\n", i + 1);


		printf("d: %d\n", d);
		printf("Defective nodes: ");
		dimension[0]=(hlsint)d; 

		int dNodes= (int) nDefNodes[0];
		for (int j = 0; j < dNodes; j++)
		{
			printf("%d ", (int)defectedNodes[j]);
		}

		printf("\n");
		decodeAVG += startExecution(q, decorderQEC, nDefNodes[0], dimension[0], buffer_defectedNodes, pairs_out_buf, pairsCount_out_buf,totalClocks);
		clocksAVG += totalClocks[0];
		clocksAVGArr[rounds] = totalClocks[0];
		printf("Pairs count: %d\n",(int)pairsCount[0]);
		int pCount=(int) pairsCount[0];
		//print all pairs
		for (int h = 0; h < pCount; h++)
		{
			printf("Pair %d %d\n", pairs[h].node1, pairs[h].node2);
			bool found = false;
			for (int l = 0; l < dNodes; l++)
			{
				if (pairs[h].node1 == defectedNodes[l])
				{
					defectedNodes[l] = -1;
					found = true;
					break;
				}
			}
			//if not found or the number correspond to a virtual node
			if (!found && pairs[h].node1 % (d + 1) != d && pairs[h].node1 % (d + 1) != 0)
			{
				printf("ERROR first node not found and is not virtual\n");
			}

			found = false;
			for (int m = 0; m < dNodes; m++)
			{
				if (pairs[h].node2 == defectedNodes[m])
				{
					defectedNodes[m] = -1;
					found = true;
					break;
				}
			}
			if (!found && pairs[h].node2 % (d + 1) != d && pairs[h].node2 % (d + 1) != 0)
			{
				printf("ERROR second node not found and is not virtual\n");
			}
		}
		for (int k = 0; k < dNodes; k++)
		{
			if (defectedNodes[k] != -1)
			{
				printf("ERROR missing value %d\n",(int)defectedNodes[k]);
			}
		}

		val = 0;

	}
    
	totalDecodes = rounds;
	float distanceSum = 0;
	float mean = (float)clocksAVG/totalDecodes;
//Standard Deviation Computation
/*
	for(int i = 0; i < totalDecodes; i++)
	{
		distanceSum += (mean - clocksAVGArr[i])*(mean - clocksAVGArr[i]);
	}
	*/
	//float SD = sqrt(distanceSum / totalDecodes);
	double nanoseconds= (double)1000/frequency;
	//printf("Decode AVG: %lf\n", (double)(clocksAVG*1000000.0)/(totalDecodes*frequency));
    //printf("Clock Cycles AVG: %f\n", (double)clocksAVG/totalDecodes);
	printf("Decode Total: %lf\n", (double)clocksAVG);
	printf("Decode AVG: %lf\n", (double)mean);
    printf("Clock Cycles AVG: %f\n", (double)mean/nanoseconds);
	//printf("Standard Deviation: %f\n", SD);
	//printf("Correct/Total_Decodes: %d / %d\n", accuracy, totalDecodes); 

	return 0;
}

