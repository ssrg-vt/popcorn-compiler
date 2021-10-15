/*******************************************************************************
** Load and Execute Kernel in the FPGA
*******************************************************************************/
#include <sys/time.h>
#include "KNL_Function.h"

// KNL CALL 0 - BEGIN Include original function
// #include "face_detect_sw.c"
// KNL CALL 0 - END Include original function


cl_uint load_file_to_memory(const char *filename, char **result)
{
    cl_uint size = 0;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        *result = NULL;
        return -1; // -1 means file opening fail
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char *)malloc(size+1);
    if (size != fread(*result, sizeof(char), size, f)) {
        free(*result);
        return -2; // -2 means file reading fail
    }
    fclose(f);
    (*result)[size] = 0;
    return size;
}



int KNL_HW_INIT (
  cl_context context,
  cl_command_queue commands,
  cl_program program,
  cl_kernel fpga_kernel
 )

{

  cl_platform_id platform_id;
  cl_device_id device_id;
  char cl_platform_vendor[1001];
  cl_platform_id platforms[16];
  cl_uint platform_count;
  cl_uint platform_found = 0;
  cl_uint num_devices;
  cl_uint device_found = 0;
  cl_device_id devices[16];
  char cl_device_name[1001];
  cl_int err;
  cl_uint i,j;





#ifdef TIMERS_ALL
  struct timeval tv;
  double time_on, time_off, time_on_all, time_off_all;
  gettimeofday(&tv, NULL);
  time_on_all = tv.tv_usec;
  time_on_all += (tv.tv_sec*1.0e6);
  // gettimeofday(&tv, NULL);
  time_on = tv.tv_usec;
  time_on += (tv.tv_sec*1.0e6);
#endif

// ------------------------------------------------------------------------------------
// Step 1: Get All PLATFORMS, then search for Target_Platform_Vendor (CL_PLATFORM_VENDOR)
// ------------------------------------------------------------------------------------

// Get the number of platforms
// ..................................................

err = clGetPlatformIDs(16, platforms, &platform_count);
if (err != CL_SUCCESS) {
    printf("Error: Failed to find an OpenCL platform!\n");
    printf("test failed\n");
    return EXIT_FAILURE;
}



#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("\nTIME--> Get Platform ID in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

#ifdef PRINT_INFO
printf("INFO--> Found %d platforms\n", platform_count);
#endif

  // ....................................................................................
  // step 1:  Search for Platform (ex: Xilinx) using: CL_PLATFORM_VENDOR = Target_Platform_Vendor
  // Check if the current platform matches Target_Platform_Vendor
  // ....................................................................................

for (cl_uint iplat=0; iplat<platform_count; iplat++) {
    err = clGetPlatformInfo(platforms[iplat], CL_PLATFORM_VENDOR, 1000, (void *)cl_platform_vendor,NULL);
    if (err != CL_SUCCESS) {
        printf("Error: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
        printf("test failed\n");
        return EXIT_FAILURE;
    }
    if (strcmp(cl_platform_vendor, "Xilinx") == 0) {
#ifdef PRINT_INFO
        printf("INFO--> Selected platform %d from %s\n", iplat, cl_platform_vendor);
#endif
        platform_id = platforms[iplat];
        platform_found = 1;
    }
}
if (!platform_found) {
    printf("ERROR: Platform Xilinx not found. Exit.\n");
    return EXIT_FAILURE;
}
   // ------------------------------------------------------------------------------------
   // Step 1:  Get All Devices for selected platform Target_Platform_ID
   //            then search for Xilinx platform (CL_DEVICE_TYPE_ACCELERATOR = Target_Device_Name)
// ------------------------------------------------------------------------------------

#ifdef TIMERS_ALL
  gettimeofday(&tv, NULL);
  time_off = tv.tv_usec;
  time_off += (tv.tv_sec*1.0e6);
  printf("TIME--> Get Platform Info in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
  gettimeofday(&tv, NULL);
  time_on = tv.tv_usec;
  time_on += (tv.tv_sec*1.0e6);
#endif




err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, 16, devices, &num_devices);
#ifdef PRINT_INFO
printf("INFO--> Found %d devices\n", num_devices);
#endif
if (err != CL_SUCCESS) {
    printf("ERROR: Failed to create a device group!\n");
    printf("ERROR: Test failed\n");
    return -1;
}


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Get Device IDs in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif


 // ------------------------------------------------------------------------------------
 // Step 1:  Search for CL_DEVICE_NAME = Target_Device_Name
 // ............................................................................

for (cl_uint i=0; i<num_devices; i++) {
    err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 1024, cl_device_name, 0);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to get device name for device %d!\n", i);
        printf("test failed\n");
        return EXIT_FAILURE;
    }
#ifdef PRINT_INFO
    printf("INFO--> CL_DEVICE_NAME = %s\n", cl_device_name);
#endif
   // ............................................................................
  // Step 1: Check if the current device matches Target_Device_Name
  // ............................................................................

   if(strcmp(cl_device_name, "TARGET_PLATFORM") == 0) {
        device_id = devices[i];
        device_found = 1;
#ifdef PRINT_INFO
        printf("INFO--> Selected %s as the target device\n", cl_device_name);
#endif
   }
}

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Search Target Device in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif



// ------------------------------------------------------------------------------------
// Step 1: Create Context
// ------------------------------------------------------------------------------------

context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
if (!context) {
    printf("Error: Failed to create a compute context!\n");
    printf("test failed\n");
    return EXIT_FAILURE;
}


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Create Context in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

    // ------------------------------------------------------------------------------------
// Step 1: Create Command Queue
// ------------------------------------------------------------------------------------
commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
if (!commands) {
    printf("Error: Failed to create a command queue!\n");
    printf("Error: code %i\n",err);
    printf("test failed\n");
    return EXIT_FAILURE;
}

cl_int status;

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Create Command Queue in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif



unsigned char *kernelbinary;
char *xclbin = "XCLBIN_FILE";
// ------------------------------------------------------------------
// Step 1: Load Binary File from a disk to Memory
// ------------------------------------------------------------------

#ifdef PRINT_INFO
printf("INFO--> Loading %s (XCLBIN File)\n", xclbin);
#endif

cl_uint n_i0 = load_file_to_memory(xclbin, (char **) &kernelbinary);

if (n_i0 < 0) {
    printf("failed to load kernel from xclbin: %s\n", xclbin);
    printf("test failed\n");
    return EXIT_FAILURE;
}

size_t n0 = n_i0;


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Load File to Memory in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif


// ------------------------------------------------------------
// Step 1: Create a program using a Binary File
// ------------------------------------------------------------

program = clCreateProgramWithBinary(context, 1, &device_id, &n0,
                                    (const unsigned char **) &kernelbinary, &status, &err);
free(kernelbinary);


#ifdef TIMERS_ALL
gettimeofday(&tv, NULL);
time_off = tv.tv_usec;
time_off += (tv.tv_sec*1.0e6);
printf("TIME--> Create Program From Binary in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off_all = tv.tv_usec;
    time_off_all += (tv.tv_sec*1.0e6);
    printf("TIME--> FPGA Init in %12.8f ms\n", (time_off_all - time_on_all)/1.0e3);
#endif


#ifdef PRINT_INFO
printf("INFO--> FPGA INIT DONE\n\n");
#endif

}


// KNL CALL 1 - BEGIN Header

// KNL CALL 1 - END  Header

{

      cl_platform_id platform_id;
      cl_device_id device_id;

      char cl_platform_vendor[1001];
      cl_platform_id platforms[16];
      cl_uint platform_count;
      cl_uint platform_found = 0;
      cl_uint num_devices;
      cl_uint device_found = 0;
      cl_device_id devices[16];
      char cl_device_name[1001];
      cl_int err;
      cl_uint i,j;





#ifdef TIMERS_ALL
      struct timeval tv;
      double time_on, time_off, time_on_all, time_off_all;
      gettimeofday(&tv, NULL);
      time_on_all = tv.tv_usec;
      time_on_all += (tv.tv_sec*1.0e6);
      time_on = tv.tv_usec;
      time_on += (tv.tv_sec*1.0e6);
#endif

   	// ------------------------------------------------------------------------------------
	// Step 1: Get All PLATFORMS, then search for Target_Platform_Vendor (CL_PLATFORM_VENDOR)
	// ------------------------------------------------------------------------------------

	// Get the number of platforms
	// ..................................................

   err = clGetPlatformIDs(16, platforms, &platform_count);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to find an OpenCL platform!\n");
        printf("test failed\n");
        return EXIT_FAILURE;
    }



    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("\nTIME--> Get Platform ID in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif

#ifdef PRINT_INFO
    printf("INFO--> Found %d platforms\n", platform_count);
#endif

      // ....................................................................................
      // step 1:  Search for Platform (ex: Xilinx) using: CL_PLATFORM_VENDOR = Target_Platform_Vendor
      // Check if the current platform matches Target_Platform_Vendor
      // ....................................................................................

    for (cl_uint iplat=0; iplat<platform_count; iplat++) {
        err = clGetPlatformInfo(platforms[iplat], CL_PLATFORM_VENDOR, 1000, (void *)cl_platform_vendor,NULL);
        if (err != CL_SUCCESS) {
            printf("Error: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
            printf("test failed\n");
            return EXIT_FAILURE;
        }
        if (strcmp(cl_platform_vendor, "Xilinx") == 0) {
#ifdef PRINT_INFO
            printf("INFO--> Selected platform %d from %s\n", iplat, cl_platform_vendor);
#endif
            platform_id = platforms[iplat];
            platform_found = 1;
        }
    }
    if (!platform_found) {
        printf("ERROR: Platform Xilinx not found. Exit.\n");
        return EXIT_FAILURE;
    }
       // ------------------------------------------------------------------------------------
       // Step 1:  Get All Devices for selected platform Target_Platform_ID
       //            then search for Xilinx platform (CL_DEVICE_TYPE_ACCELERATOR = Target_Device_Name)
	// ------------------------------------------------------------------------------------

  #ifdef TIMERS_ALL
      gettimeofday(&tv, NULL);
      time_off = tv.tv_usec;
      time_off += (tv.tv_sec*1.0e6);
      printf("TIME--> Get Platform Info in %12.8f ms\n", (time_off - time_on)/1.0e3);
  #endif


  #ifdef TIMERS_ALL
      gettimeofday(&tv, NULL);
      time_on = tv.tv_usec;
      time_on += (tv.tv_sec*1.0e6);
  #endif




   err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, 16, devices, &num_devices);
#ifdef PRINT_INFO
    printf("INFO--> Found %d devices\n", num_devices);
#endif
    if (err != CL_SUCCESS) {
        printf("ERROR: Failed to create a device group!\n");
        printf("ERROR: Test failed\n");
        return -1;
    }


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("TIME--> Get Device IDs in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif


     // ------------------------------------------------------------------------------------
     // Step 1:  Search for CL_DEVICE_NAME = Target_Device_Name
     // ............................................................................

   for (cl_uint i=0; i<num_devices; i++) {
        err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 1024, cl_device_name, 0);
        if (err != CL_SUCCESS) {
            printf("Error: Failed to get device name for device %d!\n", i);
            printf("test failed\n");
            return EXIT_FAILURE;
        }
#ifdef PRINT_INFO
        printf("INFO--> CL_DEVICE_NAME = %s\n", cl_device_name);
#endif
       // ............................................................................
      // Step 1: Check if the current device matches Target_Device_Name
      // ............................................................................

       if(strcmp(cl_device_name, "TARGET_PLATFORM") == 0) {
            device_id = devices[i];
            device_found = 1;
#ifdef PRINT_INFO
        printf("INFO--> Selected %s as the target device\n", cl_device_name);
#endif
       }
    }

    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("TIME--> Search Target Device in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif



	// ------------------------------------------------------------------------------------
	// Step 1: Create Context
	// ------------------------------------------------------------------------------------

    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!context) {
        printf("Error: Failed to create a compute context!\n");
        printf("test failed\n");
        return EXIT_FAILURE;
    }


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("TIME--> Create Context in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif

        // ------------------------------------------------------------------------------------
	// Step 1: Create Command Queue
	// ------------------------------------------------------------------------------------
    commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
    if (!commands) {
        printf("Error: Failed to create a command queue!\n");
        printf("Error: code %i\n",err);
        printf("test failed\n");
        return EXIT_FAILURE;
    }

    cl_int status;

    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("TIME--> Create Command Queue in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif



   unsigned char *kernelbinary;
    char *xclbin = "XCLBIN_FILE";
	// ------------------------------------------------------------------
	// Step 1: Load Binary File from a disk to Memory
	// ------------------------------------------------------------------

#ifdef PRINT_INFO
   printf("INFO--> Loading %s (XCLBIN File)\n", xclbin);
#endif

    cl_uint n_i0 = load_file_to_memory(xclbin, (char **) &kernelbinary);

    if (n_i0 < 0) {
        printf("failed to load kernel from xclbin: %s\n", xclbin);
        printf("test failed\n");
        return EXIT_FAILURE;
    }

    size_t n0 = n_i0;


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("TIME--> Load File to Memory in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif


	// ------------------------------------------------------------
	// Step 1: Create a program using a Binary File
	// ------------------------------------------------------------

   program = clCreateProgramWithBinary(context, 1, &device_id, &n0,
                                        (const unsigned char **) &kernelbinary, &status, &err);
    free(kernelbinary);

	// ============================================================================
	// Step 2: Create Program and Kernels
	// ============================================================================
	//   o) Build a Program from a Binary File
	//   o) Create Kernels
	// ============================================================================

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Create Program From Binary in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif




    if ((!program) || (err!=CL_SUCCESS)) {
        printf("Error: Failed to create compute program from binary %d!\n", err);
        printf("test failed\n");
        return EXIT_FAILURE;
    }

	// -------------------------------------------------------------
	// Step 2: Build (compiles and links) a program executable from binary
	// -------------------------------------------------------------

    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t len;
        char buffer[2048];

        printf("Error: Failed to build program executable!\n");
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        printf("%s\n", buffer);
        printf("test failed\n");
        return EXIT_FAILURE;
    }


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_off = tv.tv_usec;
        time_off += (tv.tv_sec*1.0e6);
        printf("TIME--> Build Program in %12.8f ms\n", (time_off - time_on)/1.0e3);
    #endif


    #ifdef TIMERS_ALL
        gettimeofday(&tv, NULL);
        time_on = tv.tv_usec;
        time_on += (tv.tv_sec*1.0e6);
    #endif


	// -------------------------------------------------------------
	// Step 2: Create Kernels
	// -------------------------------------------------------------

// KNL CALL 2 - BEGIN Create Compute Kernel

// KNL CALL 2 - END Create Compute Kernel


#ifdef CLFINISH_INTERNAL
    clFinish(commands);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Create Kernel in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif



// KNL CALL 3 - BEGIN Create host data pointers and copy kernel data

// KNL CALL 3 - END  Create host data pointers and copy kernel data

#ifdef CLFINISH_INTERNAL
    clFinish(commands);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Copy Function  Data to Host Data in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif


// KNL CALL 4 - BEGIN Create host buffers

// KNL CALL 4 - END  Create host buffers

#ifdef CLFINISH_INTERNAL
    clFinish(commands);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Host --> FPGA in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

// KNL CALL 5 - BEGIN Create kernel arguments

// KNL CALL 5 - END  Create kernel arguments

#ifdef CLFINISH_INTERNAL
    clFinish(commands);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Create Kernel Arguments in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

err = clEnqueueTask(commands, fpga_kernel, 0, NULL, NULL);
  if (err) {
            printf("Error: Failed to execute kernel! %d\n", err);
            printf("test failed\n");
            return EXIT_FAILURE;
            }

#ifdef CLFINISH_INTERNAL
    clFinish(commands);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Kernel Execution in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif

err = 0;

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

// KNL CALL 6 - BEGIN copy Global Memory to Host

// KNL CALL 6 - END  copy Global Memory to Host

    clFinish(commands);

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> FPGA --> Host in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

// KNL CALL 7 - BEGIN Copy host data to function output

// KNL CALL 7 - END  Copy host data to function output

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Copy Host Data to Function Data in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

// KNL CALL 8 - BEGIN Free Data/Buff

// KNL CALL 8 - END  Free Data/Buff

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Free Buffers in%12.8f ms\n", (time_off - time_on)/1.0e3);
#endif



#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_on = tv.tv_usec;
    time_on += (tv.tv_sec*1.0e6);
#endif

    clReleaseProgram(program);
    clReleaseKernel(fpga_kernel);
    clReleaseCommandQueue(commands);
    clReleaseContext(context);

#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off = tv.tv_usec;
    time_off += (tv.tv_sec*1.0e6);
    printf("TIME--> Release Objects in %12.8f ms\n", (time_off - time_on)/1.0e3);
#endif


#ifdef TIMERS_ALL
    gettimeofday(&tv, NULL);
    time_off_all = tv.tv_usec;
    time_off_all += (tv.tv_sec*1.0e6);
    printf("TIME--> FPGA Exec in %12.8f ms\n", (time_off_all - time_on_all)/1.0e3);
#endif


#ifdef PRINT_INFO
printf("INFO--> HW Kernel Execution DONE\n\n");
#endif

return 0;
}
