#include <stdio.h>
#include <stdlib.h>
#include <CL/cl.h>


#define KERNELPATHIN	"/home/stardica/Desktop/Kernels/MatrixMultiplication_Kernels.cl"
#define KERNELPATHOUT	"/home/stardica/Desktop/Kernels/MatrixMultiplication_Kernels.cl.bin"


//Function prototypes
cl_context CreateContext(void);
cl_command_queue CreateCommandQueue(cl_context context, cl_device_id *device);
void Cleanup(cl_context context, cl_command_queue commandQueue, cl_program program);
cl_program CreateProgram(cl_context context, cl_device_id device, const char* fileName);
bool SaveProgramBinary(cl_program program, cl_device_id device, const char* fileName);




int main(int argc, char** argv){

    cl_context context = 0;
    cl_command_queue commandQueue = 0;
    cl_program program = 0;
    cl_device_id device = 0;

    //Create an OpenCL context on first available platform
    context = CreateContext();
    if (context == NULL)
    {
        printf("Failed to create OpenCL context.\n");
        return 1;
    }

    //Create a command-queue on the first device available on the created context
    commandQueue = CreateCommandQueue(context, &device);
    if (commandQueue == NULL)
    {
    	printf("Failed to create commandQueue.\n");
    	Cleanup(context, commandQueue, program);
    	return 1;
    }

    // Create OpenCL program and store the binary for future use.
    printf("Attempting to create kernel binary from source.\n");
    program = CreateProgram(context, device, KERNELPATHIN);
    if (program == NULL)
    {
    	printf("Failed to create Program");
    	Cleanup(context, commandQueue, program);
    	return 1;
    }

    printf("Kernel is saved.\n");
    if (SaveProgramBinary(program, device, KERNELPATHOUT) == false)
    {
        printf("Failed to write program binary.\n");
        Cleanup(context, commandQueue, program);
        return 1;
     }

    printf("---Done---");
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

cl_program CreateProgram(cl_context context, cl_device_id device, const char* fileName) {

	cl_int errNum;
    cl_program program;

    char *buffer;
    long length = 0;
    char temp;
    FILE *fp;

    fp = fopen(fileName, "r");
    if (fp == NULL)
    {
        printf("Failed to open input file.\n");
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    //apparently using ftell to get file size of a text file is bad.
    length = ftell(fp);
    if (length < 0){
    	printf("Error getting file size.\n");
    	return NULL;
    }
    fseek(fp, 0L, SEEK_SET);

    buffer = (char *) malloc(length + 1);
    fread(buffer, 1, length, fp);
    //ftell is bad. Sometimes you get garbage at the end of the string.
    //add 0 to the end to terminate the string correctly.
    buffer[length] = 0;
    printf("%s\n", buffer);
    fclose(fp);

    program = clCreateProgramWithSource(context, 1, (const char**)&buffer, NULL, NULL);
    if (program == NULL)
    {
        printf("Failed to create CL program from source.\n");
        return NULL;
    }

    errNum = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        // Determine the reason for the error
        char buildLog[16384];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, NULL);

        printf("Error in kernel\n");
        //printf("%s\n", buildLog);
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
