# FusionBlossomProject
- **Team Number:** AOHW-314
- **Project Name:**  Accelerating Quantum Error Correction on FPGA: the Fusion Blossom approach 
- **Link to YouTube Video(s):** https://youtu.be/sgWAbkiQKVA
- **Link to project repository:** https://github.com/BrembillaNiccolo/FusionBlossomProject/
- **University name:** Politecnico di Milano

## Team:

**Participant(s):** Niccolò Brembilla

**Email:** niccolo1.brembilla@mail.polimi.it

**Supervisor name:** Prof. Marco Santambrogio

**Supervisor e-mail:** marco.santambrogio@polimi.it

## General Info

**Board used:** Alveo U55C High Performance Compute Card

**Software Version:** 2023.1

## Brief description of project:
Quantum Computing (QC) represents a novel computing paradigm with the ability to revolutionize the computer science domain, as it paves the way for solving problems that the current generation of classical devices is not able to deal with. Nevertheless, qubits in these systems are not perfect and suffer from noise. Therefore, finding ways to correct errors introduced in the computation becomes of paramount importance to enable QC to scale. In this context, Quantum Error Correction (QEC) represents a valuable approach but is characterized by strict limitations in latency. To satisfy these constraints, hardware acceleration on heterogeneous architectures, such as FPGAs, proved to be an effective strategy. In this context, this project consists of deploying an FPGA-based solution to decode quantum errors leveraging the Fusion Blossom algorithm.
## Description of archive:

**KernelCode folder:** contains the source files for kernel
-	kernel.cpp : source file for the kernel
  
**HostCode folder:** contains the source files for host
-	hostStart.cpp : source file for the host
  
**TestBench folder:** contains the source files for the testbench
-	testBench.cpp : source file for the testbench
  
**Makefile:** makefile to automate design generation, build the host application and run software emulation, hardware emulation and hardware.
- makefile_us_alveo.mk : utility file for Makefile
- utils.mk : utility file for Makefile
- build.sh : script file to build the kernel function.

**buildDim9 folder:** contains the binary of the host and the bitstream of the hardware for Alveo U55C
-	QECBLOSSOM : binary of the host
-	QECBLOSSOM.xclbin : bitstream of the hardware for Alveo U55C

  
## Instructions to build and test project
1. to build and create the kernel (for u55c):
	- ./build.sh  platform  desired_frequency
	
2. to only compile the host (called ./QECBLOSSOM) and run separately:
	- make host
	- ./QECBLOSSOM  path/to/BLOSSOM.xclbin  path/to/test.txt
