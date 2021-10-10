#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

// #define L1 8 

//L1
// #define BATCHS 16 
// #define L2 64 //L2
// #define L3 4 //L3

#include <vector>
#include <CL/cl2.hpp>


#include <iostream>
#include <fstream>
#include <CL/cl_ext_xilinx.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ap_int.h>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <string>
#include <cfloat>
#include <CL/cl_ext.h>


#include "./topQ_new.h"
#include "./rmm.h"

using namespace std;



// function for aligning the address


template <typename Tour>
struct aligned_allocator
{
  using value_type = Tour;
  Tour* allocate(std::size_t num)
  {
    void* ptr = nullptr;
    if (posix_memalign(&ptr,4096,num*sizeof(Tour)))
      throw std::bad_alloc();
    return reinterpret_cast<Tour*>(ptr);
  }
  void deallocate(Tour* p, std::size_t num)
  {
    free(p);
  }
};


#define OCL_CHECK(error,call)                                       \
    call;                                                           \
    if (error != CL_SUCCESS) {                                      \
      printf("%s:%d Error calling " #call ", error code is: %d\n",  \
              __FILE__,__LINE__, error);                            \
      exit(EXIT_FAILURE);                                           \
    }                                   







  


namespace xcl {


std::vector<cl::Device> get_devices(const std::string& vendor_name) {

    size_t i;
    cl_int err;
    std::vector<cl::Platform> platforms;
    OCL_CHECK(err, err = cl::Platform::get(&platforms));
    cl::Platform platform;
    for (i  = 0 ; i < platforms.size(); i++){
        platform = platforms[i];
        OCL_CHECK(err, std::string platformName = platform.getInfo<CL_PLATFORM_NAME>(&err));
        if (platformName == vendor_name){
            std::cout << "Found Platform" << std::endl;
            std::cout << "Platform Name: " << platformName.c_str() << std::endl;
            break;
        }
    }
    if (i == platforms.size()) {
        std::cout << "Error: Failed to find Xilinx platform" << std::endl;
        exit(EXIT_FAILURE);
    }
   
    //Getting ACCELERATOR Devices and selecting 1st such device 
    std::vector<cl::Device> devices;
    OCL_CHECK(err, err = platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices));
    return devices;
}


std::vector<cl::Device> get_xilinx_devices();

std::vector<cl::Device> get_xil_devices() {return get_devices("Xilinx");}

char* read_binary_file(const std::string &xclbin_file_name, unsigned &nb);
}











int main(int argc, char ** argv){
    int BATCHS=16;
      cl_int err;
    std::string binaryFile = (argc != 2) ? "./top.xclbin" : argv[1];
    unsigned fileBufSize;
    std::vector<cl::Device> devices = xcl::get_xilinx_devices();
    //devices.resize(1);
    cl::Device device = devices[0];

    OCL_CHECK(err, std::string device_name = device.getInfo<CL_DEVICE_NAME>(&err));


    // std::cout << "the device info is" << device_name;
    devices.resize(1);




    OCL_CHECK(err, cl::Context context(device, NULL, NULL, NULL, &err));

    char *fileBuf = xcl::read_binary_file(binaryFile, fileBufSize);

    cout << "the size of buff is" << *fileBuf  << endl;

    cl::Program::Binaries bins{{fileBuf, fileBufSize}};

    OCL_CHECK(err, cl::Program program(context, devices, bins, NULL, &err));
    
    // OCL_CHECK(err, cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err));
    OCL_CHECK(err, cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
    



    // cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE , &err);

    // ===============================================Q==============================================

    std::vector<float, aligned_allocator<float>> In_rows;
    In_rows.resize(L1_pi*BATCHS);

    std::vector<float, aligned_allocator<float>> In_rows_snt;
    In_rows_snt.resize(L1_pi*BATCHS);


    std::vector<float, aligned_allocator<float>> In_actions;
    In_actions.resize(L3_pi*BATCHS);

    std::vector<float, aligned_allocator<float>> In_rewards;
    In_rewards.resize(BATCHS);

    std::vector<int, aligned_allocator<int>> In_dones;
    In_dones.resize(BATCHS);

    std::vector<float, aligned_allocator<float>> Out_Q;
    Out_Q.resize(BATCHS);

    std::vector<float, aligned_allocator<float>> Out_Loss;
    Out_Loss.resize(BATCHS);

    // ===============================================Pi==============================================

    std::vector<float, aligned_allocator<float>> In_rows_pi;
    In_rows_pi.resize(L1_pi*BATCHS);

    std::vector<w1blockvec, aligned_allocator<w1blockvec>> Out_w1bram;
    Out_w1bram.resize(L1_pi);

    std::vector<Piw2blockvec, aligned_allocator<Piw2blockvec>> Out_w2bram;
    Out_w2bram.resize(L2);


    std::vector<float, aligned_allocator<float>> Out_bias1;
    Out_bias1.resize(L2);

    std::vector<float, aligned_allocator<float>> Out_bias2;
    Out_bias2.resize(L3_pi);



    int i, j, jj;


    std::cout << "Host: init input states/actions..." << std::endl;
    printf("\nHost: input snt content:\n");
    
    for (jj = 0; jj < BATCHS; jj++) {   
        for (j = 0; j < L1_pi; j++) {
            // for (i = 0; i < BSIZE; i++) {
            In_rows[L1_pi*jj+j] = float(-j)/float(4.0)+jj;
            
            In_rows_snt[L1_pi*jj+j] = float(jj)/float(4.0);
            In_rows_pi[L1_pi*jj+j] = float(jj)/float(4.0);
            printf("%f ",In_rows_pi[L1_pi*jj+j]);
            // printf("%f ",In_rows_snt[j].a[i]);
            // }
        }
        printf("\n");
    }

    printf("\nHost: input acts content:\n");
    for (jj = 0; jj < BATCHS; jj++) {   
        for (j = 0; j < L3_pi; j++) {
            // for (i = 0; i < BSIZE; i++) {
            if(jj%2==0)In_actions[L3_pi*jj+j] = float(2)+j;
            else In_actions[L3_pi*jj+j] = float(1)-j;
            printf("%f ",In_actions[L3_pi*jj+j]);
            // printf("%f ",In_rows_snt[j].a[i]);
            // }
        }
        printf("\n");
    }

    printf("\nHost: Init input reward/done content...\n");

    for (jj = 0; jj < BATCHS; jj++) {   
        // for (i = 0; i < BSIZE; i++) {
        // printf("\njj,i:%d,%d\n",jj,i);

            In_rewards[jj] = float(1);
            In_dones[jj] = int(0);
            
        // }
    }
    
    std::vector<int, aligned_allocator<int>> insert_ind;
    insert_ind.resize(BATCHS);
    std::vector<float, aligned_allocator<float>> init_priority;
    init_priority.resize(BATCHS);
    std::vector<int, aligned_allocator<int>> ind_o_out;
    ind_o_out.resize(BATCHS);

    printf("\nHost: Init replay insert inputs...\n");
    for (i = 0; i < BATCHS; i++) {
        // printf("\njj,i:%d,%d\n",jj,i);
        insert_ind[i] = i;
        init_priority[i] = i+1;
        // printf("%f ",In_actions[jj].a[i]);
    }
    
    cl_mem_ext_ptr_t InqExt1;
    cl_mem_ext_ptr_t InqExt2;
    cl_mem_ext_ptr_t InqExt3;
    cl_mem_ext_ptr_t InqExt4;
    cl_mem_ext_ptr_t InqExt5;
    cl_mem_ext_ptr_t OutqExt1;
    cl_mem_ext_ptr_t OutqExt2;

    InqExt1.obj = In_rows.data();
    InqExt1.param = 0;
    InqExt1.banks = XCL_MEM_DDR_BANK0;
    InqExt1.flags = XCL_MEM_DDR_BANK0;

    InqExt2.obj = In_rows_snt.data();
    InqExt2.param = 0;
    InqExt2.banks = XCL_MEM_DDR_BANK0;
    InqExt2.flags = XCL_MEM_DDR_BANK0;

    InqExt3.obj = In_actions.data();
    InqExt3.param = 0;
    InqExt3.banks = XCL_MEM_DDR_BANK0;
    InqExt3.flags = XCL_MEM_DDR_BANK0;

    InqExt4.obj = In_rewards.data();
    InqExt4.param = 0;
    InqExt4.banks = XCL_MEM_DDR_BANK0;
    InqExt4.flags = XCL_MEM_DDR_BANK0;

    InqExt5.obj = In_dones.data();
    InqExt5.param = 0;
    InqExt5.banks = XCL_MEM_DDR_BANK0;
    InqExt5.flags = XCL_MEM_DDR_BANK0;

    OutqExt1.obj = Out_Q.data();
    OutqExt1.param = 0;
    OutqExt1.banks = XCL_MEM_DDR_BANK0;
    OutqExt1.flags = XCL_MEM_DDR_BANK0;

    OutqExt2.obj = Out_Loss.data();
    OutqExt2.param = 0;
    OutqExt2.banks = XCL_MEM_DDR_BANK0;
    OutqExt2.flags = XCL_MEM_DDR_BANK0;


    cl_mem_ext_ptr_t InpiExt1;
    cl_mem_ext_ptr_t OutpiExt1;
    cl_mem_ext_ptr_t OutpiExt2;
    cl_mem_ext_ptr_t OutpiExt3;
    cl_mem_ext_ptr_t OutpiExt4;


    InpiExt1.obj = In_rows_pi.data();
    InpiExt1.param = 0;
    InpiExt1.banks = XCL_MEM_DDR_BANK3;
    InpiExt1.flags = XCL_MEM_DDR_BANK3;

    OutpiExt1.obj = Out_w1bram.data();
    OutpiExt1.param = 0;
    OutpiExt1.banks = XCL_MEM_DDR_BANK3;
    OutpiExt1.flags = XCL_MEM_DDR_BANK3;

    OutpiExt2.obj = Out_w2bram.data();
    OutpiExt2.param = 0;
    OutpiExt2.banks = XCL_MEM_DDR_BANK3;
    OutpiExt2.flags = XCL_MEM_DDR_BANK3;

    OutpiExt3.obj = Out_bias1.data();
    OutpiExt3.param = 0;
    OutpiExt3.banks = XCL_MEM_DDR_BANK3;
    OutpiExt3.flags = XCL_MEM_DDR_BANK3;

    OutpiExt4.obj = Out_bias2.data();
    OutpiExt4.param = 0;
    OutpiExt4.banks = XCL_MEM_DDR_BANK3;
    OutpiExt4.flags = XCL_MEM_DDR_BANK3;




    // ========================replay=====================
    
    cl_mem_ext_ptr_t RepInExt1;
    cl_mem_ext_ptr_t RepInExt2;
    cl_mem_ext_ptr_t RepoutExt; //for output
    

    RepInExt1.obj = insert_ind.data();
    RepInExt1.param = 0;
    RepInExt1.flags = 1|XCL_MEM_TOPOLOGY;

    RepInExt2.obj = init_priority.data();
    RepInExt2.param = 0;
    RepInExt2.flags = 1|XCL_MEM_TOPOLOGY;

    RepoutExt.obj = ind_o_out.data();
    RepoutExt.param = 0;
    RepoutExt.flags = 1|XCL_MEM_TOPOLOGY;

    printf("flags set\n");



  
    // Create the buffers and allocate memory for Q net (SLR0)
    cl::Buffer inq1_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * L1_pi * BATCHS, &InqExt1, &err);
    cl::Buffer inq2_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * L1_pi * BATCHS, &InqExt2, &err);
    cl::Buffer inq3_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * BATCHS, &InqExt3, &err);
    cl::Buffer inq4_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * BATCHS, &InqExt4, &err);
    cl::Buffer inq5_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(int) * BATCHS, &InqExt5, &err);
    cl::Buffer outq1_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * BATCHS, &OutqExt1, &err);
    cl::Buffer outq2_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * BATCHS, &OutqExt2, &err);
// std::cout << sizeof(actvec) * BATCHS << std::endl;
    // Create the buffers and allocate memory for PI net (SLR3)
    cl::Buffer inpi1_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * L1_pi * BATCHS, &InpiExt1, &err);
    cl::Buffer outpi1_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(w1blockvec) * L1_pi, &OutpiExt1, &err);
    cl::Buffer outpi2_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(Piw2blockvec) * L2, &OutpiExt2, &err);
    cl::Buffer outpi3_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * L2, &OutpiExt3, &err);
    cl::Buffer outpi4_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * L3_pi, &OutpiExt4, &err);

    printf("Learners buffers allocated\n");

    // Top_tree(int insert_signal,int *insert_ind,float *init_priority, int update_signal, hls::stream<ap_axiu<32,0,0,0>> &pn_in,int sample_signal,int *ind_o)
    int insert_signal_in;
    int update_signal;
    int sample_signal;
    cl::Buffer insind_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(int) * BATCHS, &RepInExt1, &err);
    cl::Buffer inpn_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(float) * BATCHS, &RepInExt2, &err);
    cl::Buffer out_buf(context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX, sizeof(int) * BATCHS, &RepoutExt, &err);
    printf("Replay buffers allocated\n");


/* Tree, uncomment later

    OCL_CHECK(err,cl::Kernel krnl_tree(program, "Top_tree:{Top_tree_1}", &err));

    // ===================Replay Insert Initialze:
    // ===================and Replay sampling: (insert only needed once at first. In all later insertion, use insert_signal_in = 2;)
    insert_signal_in = 1;
    update_signal=0;
    sample_signal = 0;
    krnl_tree.setArg(0, insert_signal_in);
    krnl_tree.setArg(1, insind_buf);
    krnl_tree.setArg(2, inpn_buf);
    krnl_tree.setArg(3, update_signal);
    krnl_tree.setArg(5, sample_signal);
    krnl_tree.setArg(6, out_buf);
    // q.enqueueMigrateMemObjects({insind_buf}, 0);
    // q.enqueueMigrateMemObjects({inpn_buf}, 0);
    q.enqueueTask(krnl_tree);
    // q.enqueueMigrateMemObjects({out_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();
    printf("\nFrom Host: Tree init done.\n");
    

    cl::Kernel krnl_tree2(program, "Top_tree", &err);
    insert_signal_in = 2;
    update_signal=0;
    sample_signal = 1;

    printf("\n Host: priorities to insert:\n");
    for (int i=0;i<BATCHS;i++){
        printf("%f ",init_priority[i]);
    }
    printf("\n");
    printf("\n Host: indices to insert:\n");
    for (int i=0;i<BATCHS;i++){
        printf("%d: ",insert_ind[i]);
    }
    printf("\n");

    krnl_tree2.setArg(0, insert_signal_in);
    krnl_tree2.setArg(1, insind_buf);
    krnl_tree2.setArg(2, inpn_buf);
    krnl_tree2.setArg(3, update_signal);
    krnl_tree2.setArg(5, sample_signal);
    krnl_tree2.setArg(6, out_buf);
    q.enqueueMigrateMemObjects({insind_buf,inpn_buf}, 0);
    q.finish();
    q.enqueueTask(krnl_tree2);
    q.finish();
    q.enqueueMigrateMemObjects({out_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();
    // printf("===============================================\n");
    // printf("========= Sampling: DATA BACK TO HOST =========\n");
    // printf("===============================================\n");
    printf("\nSample indices content:\n");
    for(int i = 0; i < BATCHS; i++) {
        printf("%d ",ind_o_out[i]);
    }
    printf("\n");
    q.finish(); //(??? Sequential after Insert or need barrier ???):
    printf("q.finish\n");

    */

// void learnersQ_top(float *S, float *Snt, float *acts, float *r, float gamma, float alpha, int *done, 
// hls::stream<ap_axiu<32,0,0,0>> &Qw1_axiout, hls::stream<ap_axiu<32,0,0,0>> &Qw2_axiout, hls::stream<ap_axiu<32,0,0,0>> &Qbias1_axiout, hls::stream<ap_axiu<32,0,0,0>> &Qbias2_axiout,
// hls::stream<ap_axiu<32,0,0,0>> &Pitw1_axiin, hls::stream<ap_axiu<32,0,0,0>> &Pitw2_axiin, hls::stream<ap_axiu<32,0,0,0>> &Pitbias1_axiin, hls::stream<ap_axiu<32,0,0,0>> &Pitbias2_axiin,
// int Qwsync,int Piwsync, /*Learners args*/
// float *Qs,float *Loss_sqrt,/*Logging args*/
// hls::stream<ap_axiu<32,0,0,0>> &pn_out/*Replay args*/);

// void learnersPi_top(float *S, float alpha, int Piwsync,
// hls::stream<ap_axiu<32,0,0,0>> &Qw1_axiin, hls::stream<ap_axiu<32,0,0,0>> &Qw2_axiin, hls::stream<ap_axiu<32,0,0,0>> &Qbias1_axiin, hls::stream<ap_axiu<32,0,0,0>> &Qbias2_axiin,
// hls::stream<ap_axiu<32,0,0,0>> &Pitw1_axiout, hls::stream<ap_axiu<32,0,0,0>> &Pitw2_axiout, hls::stream<ap_axiu<32,0,0,0>> &Pitbias1_axiout, hls::stream<ap_axiu<32,0,0,0>> &Pitbias2_axiout,
// w1blockvec policyw1_out[L1_pi], Piw2blockvec policyw2_out[L2], float *bias1_out, float *bias2_out
// )

    cl::Kernel krnl_topQ(program, "learnersQ_top:{topq_1}");
    cl::Kernel krnl_tree3(program, "Top_tree", &err); 
    cl::Kernel krnl_topPi(program, "learnersPi_top:{toppi_1}");
    float gamma=0.5;
    float alpha=0.1;
    int Qwsync = 0;
    int Piwsync = 0;
    
    int seed=0;
    OCL_CHECK(err, err =krnl_topQ.setArg(0, inq1_buf));
    OCL_CHECK(err, err =krnl_topQ.setArg(1, inq2_buf));
    OCL_CHECK(err, err =krnl_topQ.setArg(2, inq3_buf));
    OCL_CHECK(err, err =krnl_topQ.setArg(3, inq4_buf));
    OCL_CHECK(err, err =krnl_topQ.setArg(4, gamma));
    OCL_CHECK(err, err =krnl_topQ.setArg(5, alpha));
    OCL_CHECK(err, err =krnl_topQ.setArg(6, inq5_buf));
    OCL_CHECK(err, err =krnl_topQ.setArg(15, Qwsync));
    OCL_CHECK(err, err =krnl_topQ.setArg(16, Piwsync));
    OCL_CHECK(err, err =krnl_topQ.setArg(17, outq1_buf)); //Logging Qs  float*BATCHS*BSIZE
    OCL_CHECK(err, err =krnl_topQ.setArg(18, outq2_buf)); //Logging Loss  float*BATCHS*BSIZE
    OCL_CHECK(err, err =krnl_topQ.setArg(20, BATCHS)); 

    
    OCL_CHECK(err, err =krnl_topPi.setArg(0, inpi1_buf));
    OCL_CHECK(err, err =krnl_topPi.setArg(1, alpha));
    OCL_CHECK(err, err =krnl_topPi.setArg(2, Piwsync));
    OCL_CHECK(err, err =krnl_topPi.setArg(11, outpi1_buf));
    OCL_CHECK(err, err =krnl_topPi.setArg(12, outpi2_buf));
    OCL_CHECK(err, err =krnl_topPi.setArg(13, outpi3_buf));
    OCL_CHECK(err, err =krnl_topPi.setArg(14, outpi4_buf));
    OCL_CHECK(err, err =krnl_topPi.setArg(15, BATCHS));


    insert_signal_in = 0;
    update_signal=1;
    sample_signal = 0;
    krnl_tree3.setArg(0, insert_signal_in);
    krnl_tree3.setArg(1, insind_buf);
    krnl_tree3.setArg(2, inpn_buf);
    krnl_tree3.setArg(3, update_signal);
    krnl_tree3.setArg(5, sample_signal);
    krnl_tree3.setArg(6, seed);
    krnl_tree3.setArg(7, out_buf);
    krnl_tree3.setArg(8, BATCHS);
    krnl_tree3.setArg(9, BATCHS);
    
    q.enqueueMigrateMemObjects({inq1_buf,inq2_buf,inq3_buf,inq4_buf,inq5_buf}, 0 /* 0 means from host*/);
    // q.enqueueMigrateMemObjects({insind_buf, inpn_buf}, 0 /* 0 means from host*/);
    q.enqueueMigrateMemObjects({inpi1_buf}, 0 /* 0 means from host*/);
    q.finish();
    printf("migrated\n");
    q.enqueueTask(krnl_topQ);
    printf("Hi1\n");
    q.enqueueTask(krnl_tree3);
    printf("Hi2\n");
    q.finish();
    q.enqueueTask(krnl_topPi);
    printf("Hi3\n");
    q.finish();
    q.enqueueMigrateMemObjects({outq1_buf,outq2_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.enqueueMigrateMemObjects({outpi1_buf,outpi2_buf,outpi3_buf,outpi4_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish(); 
    printf("q.finish\n");

    printf("============================================================================\n");
    printf("================= DATA BACK TO HOST: Learners Pi Train round 1 ================\n");
    printf("============================================================================\n");
    

    // Print weights, bias of Pi
    printf("\nW1 content:\n");
    for(int i = 0; i < L1_pi; i++) {
        for(int j = 0; j < L2; j++) {
                printf("%.8f ",Out_w1bram[j].a[i]);  //L1 rows, L2 cols                
        }
        printf("\n");        
    }

    printf("\nW2 content:\n");
    for(int i = 0; i < L2; i++) {
        for(int j = 0; j < L3_pi; j++) {
                printf("%.8f ",Out_w2bram[j].a[i]);  //L2 rows, L3 cols
        }
        printf("\n");
    }

    printf("\nBias1 content:\n");
    for(int i = 0; i < L2; i++) {
        printf("%.8f ",Out_bias1[i]);                 
    }
    printf("\n"); 


    printf("\nBias2 content:\n");
    for(int i = 0; i < L3_pi; i++) {
        printf("%.8f ",Out_bias2[i]);           
    }
    printf("\n");  
    

    //Print Qs, Loss
    printf("=======================================================================================\n");
    printf("================= Logging DATA BACK TO HOST: Learners Q Train round 1 =================\n");
    printf("=======================================================================================\n");
    printf("\nQs content:\n");
    for(int i = 0; i < BATCHS; i++) {
        printf("%.8f ",Out_Q[i]);
    }
    printf("\n"); 
    printf("\nLoss content:\n");
    for(int i = 0; i < BATCHS; i++) {
        printf("%.8f ",Out_Loss[i]);
    }
    printf("\n"); 



    // // ===================Learner train with static weight:


    // cl::Kernel krnl_top1(program, "learners_top", &err);
    // gamma=0.5;
    // alpha=0.1;
    // wsync = 1;
    // krnl_top1.setArg(0, in1_buf);
    // krnl_top1.setArg(1, in2_buf);
    // krnl_top1.setArg(2, in3_buf);
    // krnl_top1.setArg(3, in4_buf);
    // krnl_top1.setArg(4, gamma);
    // krnl_top1.setArg(5, alpha);
    // krnl_top1.setArg(6, in5_buf);
    // krnl_top1.setArg(7, out1_buf);
    // krnl_top1.setArg(8, out2_buf);
    // krnl_top1.setArg(9, out3_buf);
    // krnl_top1.setArg(10, out4_buf); //bias2, float*L3
    // krnl_top1.setArg(11, wsync);
    // krnl_top1.setArg(12, out5_buf); //Logging Qs  float*BATCHS*BSIZE
    // krnl_top1.setArg(13, out6_buf); //Logging Loss  float*BATCHS*BSIZE
    // // Schedule transfer of inputs to device memory, execution of kernel, and transfer of outputs back to host memory
    // q.enqueueMigrateMemObjects({in1_buf}, 0 /* 0 means from host*/);
    // q.enqueueMigrateMemObjects({in2_buf}, 0 /* 0 means from host*/);
    // q.enqueueMigrateMemObjects({in3_buf}, 0 /* 0 means from host*/);
    // q.enqueueMigrateMemObjects({in4_buf}, 0 /* 0 means from host*/);
    // q.enqueueMigrateMemObjects({in5_buf}, 0 /* 0 means from host*/);
    // // q.finish();
    // // printf("sent data\n");
    // q.enqueueTask(krnl_top1);
    // // q.finish();
    // // printf("enqueue\n");


    // q.enqueueMigrateMemObjects({out1_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // q.enqueueMigrateMemObjects({out2_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // q.enqueueMigrateMemObjects({out3_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // q.enqueueMigrateMemObjects({out4_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // q.enqueueMigrateMemObjects({out5_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // q.enqueueMigrateMemObjects({out6_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // printf("executed learner kernel with weight static\n");
    // // ===================Replay Update (??? Parallel with train ???):


    // cl::Kernel krnl_tree4(program, "Top_tree", &err);
    // krnl_tree4.setArg(0, insert_signal_in);
    // krnl_tree4.setArg(1, insind_buf);
    // krnl_tree4.setArg(2, inpn_buf);
    // krnl_tree4.setArg(3, update_signal);
    // krnl_tree4.setArg(5, sample_signal);
    // krnl_tree4.setArg(6, out_buf);
    // insert_signal_in = 0;
    // update_signal=1;
    // sample_signal = 0;
    // q.enqueueMigrateMemObjects({insind_buf}, 0 /* 0 means from host*/);
    // q.enqueueMigrateMemObjects({inpn_buf}, 0 /* 0 means from host*/);
    // q.enqueueTask(krnl_tree4);
    // q.enqueueMigrateMemObjects({out_buf}, CL_MIGRATE_MEM_OBJECT_HOST);
    // // ------------------------------------------------------------------------------------
    // // Step 4: Check Results and Release Allocated Resources
    // // ------------------------------------------------------------------------------------
    // // bool match = true;
    // printf("======================================================================================================\n");
    // printf("================================== DATA BACK TO HOST: Train round 2 ==================================\n");
    // printf("======================================================================================================\n");
    // // Print weights, bias

    // printf("\nW1 content:\n");
    // for(int i = 0; i < L1; i++) {
    //     for(int j = 0; j < L2; j++) {
    //             printf("%.8f ",Out_w1bram[j].a[i]);  //L1 rows, L2 cols                
    //     }
    //     printf("\n");        
    // }
    // printf("\nBias1 content:\n");
    // for(int i = 0; i < L2; i++) {
    //     printf("%.8f ",Out_bias1[i]);                 
    // }
    // printf("\n"); 

    // printf("\nW2 content:\n");
    // for(int i = 0; i < L2; i++) {
    //     for(int j = 0; j < L3; j++) {
    //             printf("%.8f ",Out_w2bram[j].a[i]);  //L2 rows, L3 cols
    //     }
    //     printf("\n");
    // }
    // printf("\nBias2 content:\n");
    // for(int i = 0; i < L3; i++) {
    //     printf("%.8f ",Out_bias2[i]);           
    // }
    // printf("\n");  

    // //Print Qs, Loss

    // printf("\nQs content:\n");
    // for(int i = 0; i < BATCHS*BSIZE; i++) {
    //     printf("%.8f ",Out_Q[i]);
    // }
    // printf("\n"); 
    // printf("\nLoss content:\n");
    // for(int i = 0; i < BATCHS*BSIZE; i++) {
    //     printf("%.8f ",Out_Loss[i]);
    // }
    // printf("\n"); 


  return 0;


}






namespace xcl {

std::vector<cl::Device> get_xilinx_devices() 
{
    size_t i;
    cl_int err;
    std::vector<cl::Platform> platforms;
    err = cl::Platform::get(&platforms);
    cl::Platform platform;
    for (i  = 0 ; i < platforms.size(); i++){
        platform = platforms[i];
        std::string platformName = platform.getInfo<CL_PLATFORM_NAME>(&err);
        if (platformName == "Xilinx"){
            std::cout << "INFO: Found Xilinx Platform" << std::endl;
            break;
        }
    }
    if (i == platforms.size()) {
        std::cout << "ERROR: Failed to find Xilinx platform" << std::endl;
        exit(EXIT_FAILURE);
    }
   
    //Getting ACCELERATOR Devices and selecting 1st such device 
    std::vector<cl::Device> devices;
    err = platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
    return devices;
}
   
char* read_binary_file(const std::string &xclbin_file_name, unsigned &nb) 
{
    if(access(xclbin_file_name.c_str(), R_OK) != 0) {
        printf("ERROR: %s xclbin not available please build\n", xclbin_file_name.c_str());
        exit(EXIT_FAILURE);
    }
    //Loading XCL Bin into char buffer 
    std::cout << "INFO: Loading '" << xclbin_file_name << "'\n";
    std::ifstream bin_file(xclbin_file_name.c_str(), std::ifstream::binary);
    bin_file.seekg (0, bin_file.end);
    nb = bin_file.tellg();
    bin_file.seekg (0, bin_file.beg);
    char *buf = new char [nb];
    bin_file.read(buf, nb);
    return buf;
}


}
