#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <CL/cl.h>


//Function prototypes
cl_context CreateContext(void);
cl_command_queue CreateCommandQueue(cl_context context, cl_device_id *device);
void Cleanup(cl_context context, cl_command_queue commandQueue, cl_program program);
cl_program CreateProgramFromBinary(cl_context context, cl_device_id device, const char* fileName);
cl_program CreateProgram(cl_context context, cl_device_id device, const char* fileName);
bool SaveProgramBinary(cl_program program, cl_device_id device, const char* fileName);


//Global variables

int main(int argc, char** argv)
{
    cl_context context = 0;
    cl_command_queue commandQueue = 0;
    cl_program program = 0;
    cl_device_id device = 0;

    // Create an OpenCL context on first available platform
    context = CreateContext();
    if (context == NULL)
    {
        printf("Failed to create OpenCL context.\n");
        return 1;
    }

    // Create a command-queue on the first device available
    // on the created context
    commandQueue = CreateCommandQueue(context, &device);
    if (commandQueue == NULL)
    {
        Cleanup(context, commandQueue, program);
        return 1;
    }

    // Create OpenCL program - first attempt to load cached binary.
    //  If that is not available, then create the program from source
    //  and store the binary for future use.
    printf("Attempting to create kernel binary from source...\n");

    //program = CreateProgramFromBinary(context, device, "HelloWorld.cl.bin");
    //if (program == NULL)
    //{
        //printf("Binary not loaded, create from source...\n");
    program = CreateProgram(context, device, "/home/stardica/Desktop/KernelPrecompiler/Release/MatrixMultiplication_Kernels.cl");
    if (program == NULL)
    {
         Cleanup(context, commandQueue, program);
         return 1;
    }

    printf("Save program binary for future run...\n");
    if (SaveProgramBinary(program, device, "/home/stardica/Desktop/KernelPrecompiler/Release/MatrixMultiplication_Kernels.cl.bin") == false)
    {
        printf("Failed to write program binary.\n");
        Cleanup(context, commandQueue, program);
        return 1;
     }

    return 0;
}

cl_context CreateContext() {
    cl_int errNum;
    cl_uint numPlatforms;
    cl_platform_id firstPlatformId;
    cl_context context = NULL;

    // First, select an OpenCL platform to run on.  For this example, we
    // simply choose the first available platform.  Normally, you would
    // query for all available platforms and select the most appropriate one.
    errNum = clGetPlatformIDs(1, &firstPlatformId, &numPlatforms);
    if (errNum != CL_SUCCESS || numPlatforms <= 0)
    {
        printf("Failed to find any OpenCL platforms.\n");
        return NULL;
    }

    // Next, create an OpenCL context on the platform.  Attempt to
    // create a GPU-based context, and if that fails, try to create
    // a CPU-based context.
    cl_context_properties contextProperties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)firstPlatformId, 0 };

    context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_GPU, NULL, NULL, &errNum);

    if (errNum != CL_SUCCESS)
    {
        printf("Could not create GPU context, trying CPU...\n");
        context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_CPU, NULL, NULL, &errNum);
        if (errNum != CL_SUCCESS)
        {
            printf("Failed to create an OpenCL GPU or CPU context.\n");
            return NULL;
        }
    }

    return context;
}

cl_command_queue CreateCommandQueue(cl_context context, cl_device_id *device)
{
    cl_int errNum;
    cl_device_id *devices;
    cl_command_queue commandQueue = NULL;
    size_t deviceBufferSize = -1;

    // First get the size of the devices buffer
    errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &deviceBufferSize);
    if (errNum != CL_SUCCESS)
    {
        printf("Failed call to clGetContextInfo(...,GL_CONTEXT_DEVICES,...)\n");
        return NULL;
    }

    if (deviceBufferSize <= 0)
    {
        printf("No devices available.\n");
        return NULL;
    }

    // Allocate memory for the devices buffer
    devices = new cl_device_id[deviceBufferSize / sizeof(cl_device_id)];
    errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceBufferSize, devices, NULL);
    if (errNum != CL_SUCCESS)
    {
        delete [] devices;
        printf("Failed to get device IDs");
        return NULL;
    }

    // In this example, we just choose the first available device.  In a
    // real program, you would likely use all available devices or choose
    // the highest performance device based on OpenCL device queries
    commandQueue = clCreateCommandQueue(context, devices[0], 0, NULL);
    if (commandQueue == NULL)
    {
        delete [] devices;
        printf("Failed to create commandQueue for device 0");
        return NULL;
    }

    *device = devices[0];
    delete [] devices;
    return commandQueue;
}

void Cleanup(cl_context context, cl_command_queue commandQueue, cl_program program){

    if (commandQueue != 0)
        clReleaseCommandQueue(commandQueue);

    if (program != 0)
        clReleaseProgram(program);

    if (context != 0)
        clReleaseContext(context);
}

cl_program CreateProgramFromBinary(cl_context context, cl_device_id device, const char* fileName){

	FILE *fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
    	printf("CreateProgramFromBinary(): Can't open binary file.\n");
        return NULL;
    }

    // Determine the size of the binary
    size_t binarySize;
    fseek(fp, 0, SEEK_END);
    binarySize = ftell(fp);
    rewind(fp);

    unsigned char *programBinary = new unsigned char[binarySize];
    fread(programBinary, 1, binarySize, fp);
    fclose(fp);

    cl_int errNum = 0;
    cl_program program;
    cl_int binaryStatus;

    program = clCreateProgramWithBinary(context, 1, &device, &binarySize, (const unsigned char**)&programBinary, &binaryStatus,&errNum);

    delete [] programBinary;
    if (errNum != CL_SUCCESS)
    {
        printf("Error loading program binary.\n");
        return NULL;
    }

    if (binaryStatus != CL_SUCCESS)
    {
        printf("Invalid binary for device.\n");
        return NULL;
    }

    errNum = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        // Determine the reason for the error
        char buildLog[16384];
        char *buildlog_ptr = NULL;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, NULL);

        buildlog_ptr = (char *)&buildLog;
        printf("Error in program: %s", buildlog_ptr);
        clReleaseProgram(program);
        return NULL;
    }

    return program;
}

cl_program CreateProgram(cl_context context, cl_device_id device, const char* fileName) {

	cl_int errNum;
    cl_program program;

    std::ifstream kernelFile(fileName, std::ios::in);
    if (!kernelFile.is_open())
    {
        printf("Failed to open file for reading: \n");
        return NULL;
    }

    std::ostringstream oss;
    oss << kernelFile.rdbuf();

    std::string srcStdStr = oss.str();
    const char *srcStr = srcStdStr.c_str();
    program = clCreateProgramWithSource(context, 1,
                                        (const char**)&srcStr,
                                        NULL, NULL);
    if (program == NULL)
    {
        std::cerr << "Failed to create CL program from source." << std::endl;
        return NULL;
    }

    errNum = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        // Determine the reason for the error
        char buildLog[16384];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                              sizeof(buildLog), buildLog, NULL);

        std::cerr << "Error in kernel: " << std::endl;
        std::cerr << buildLog;
        clReleaseProgram(program);
        return NULL;
    }

    return program;
}

bool SaveProgramBinary(cl_program program, cl_device_id device, const char* fileName){

	cl_uint numDevices = 0;
    cl_int errNum;

    // 1 - Query for number of devices attached to program
    errNum = clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof(cl_uint), &numDevices, NULL);
    if (errNum != CL_SUCCESS)
    {
        printf("Error querying for number of devices.\n");
        return false;
    }

    // 2 - Get all of the Device IDs
    cl_device_id *devices = new cl_device_id[numDevices];
    errNum = clGetProgramInfo(program, CL_PROGRAM_DEVICES, sizeof(cl_device_id) * numDevices, devices, NULL);

    if (errNum != CL_SUCCESS)
    {
    	printf("Error querying for devices.\n");
        delete [] devices;
        return false;
    }

    // 3 - Determine the size of each program binary
    size_t *programBinarySizes = new size_t [numDevices];
    errNum = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t) * numDevices, programBinarySizes, NULL);
    if (errNum != CL_SUCCESS)
    {
    	printf("Error querying for program binary sizes.\n");
        delete [] devices;
        delete [] programBinarySizes;
        return false;
    }

    unsigned char **programBinaries = new unsigned char*[numDevices];
    for (cl_uint i = 0; i < numDevices; i++)
    {
        programBinaries[i] = new unsigned char[programBinarySizes[i]];
    }

    // 4 - Get all of the program binaries
    errNum = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(unsigned char*) * numDevices, programBinaries, NULL);
    if (errNum != CL_SUCCESS)
    {
    	printf("Error querying for program binaries\n");
        delete [] devices;
        delete [] programBinarySizes;
        for (cl_uint i = 0; i < numDevices; i++)
        {
            delete [] programBinaries[i];
        }
        delete [] programBinaries;
        return false;
    }

    // 5 - Finally store the binaries for the device requested out to disk for future reading.
    for (cl_uint i = 0; i < numDevices; i++)
    {
        // Store the binary just for the device requested.  In a scenario where
        // multiple devices were being used you would save all of the binaries out here.
        if (devices[i] == device)
        {
            FILE *fp = fopen(fileName, "wb");
            fwrite(programBinaries[i], 1, programBinarySizes[i], fp);
            fclose(fp);
            break;
        }
    }

    // Cleanup
    delete [] devices;
    delete [] programBinarySizes;
    for (cl_uint i = 0; i < numDevices; i++)
    {
        delete [] programBinaries[i];
    }
    delete [] programBinaries;
    return true;
}
