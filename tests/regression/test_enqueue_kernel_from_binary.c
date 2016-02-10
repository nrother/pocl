#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <CL/opencl.h>

#define BUFFER_SIZE 1024

// OpenCL kernel. Each work item takes care of one element of c
const char *kernelSource =                                                "\n" \
"__kernel void vecAdd( __constant int *a,                               \n" \
"                      __constant int *b,                               \n" \
"                      __global int *c)                                 \n" \
"{                                                                         \n" \
"unsigned int i = get_global_id(0) * get_global_size(1) + get_global_id(1);\n" \
"c[i] = a[i] + b[i];                                                       \n" \
"}                                                                         \n" \
                                                                          "\n" ;

int main(void)
{
        int *h_a;
	int *h_b;
	int *h_c1;
	int *h_c2;
	int *h_c3;

	cl_mem d_a;
	cl_mem d_b;
	cl_mem d_c1;
	cl_mem d_c2;
	cl_mem d_c3;

	cl_platform_id cpPlatform;	// OpenCL platform
	cl_device_id device_id;		// device ID
	cl_context context;		// context
	cl_command_queue queue;		// command queue
	cl_program program1, program2;		// program
	cl_kernel kernel1, kernel2, kernel3;	// kernel

	size_t globalSize[2], localSize[2];
	cl_int err;

//###################################################################
//###################################################################
// Initialize variables

        unsigned bytes = BUFFER_SIZE * sizeof(int);

	h_a = (int*)malloc(bytes);
	h_b = (int*)malloc(bytes);
	h_c1 = (int*)malloc(bytes);
	h_c2 = (int*)malloc(bytes);
	h_c3 = (int*)malloc(bytes);

	unsigned int i;
	for( i = 0; i < BUFFER_SIZE; i++ )
	{
		h_a[i] = 2*i-1;
		h_b[i] = -i;
	}
        memset(h_c1, 0, bytes);
        memset(h_c2, 0, bytes);
        memset(h_c3, 0, bytes);

//###################################################################
//###################################################################
// Initialize cl_programS

	err = clGetPlatformIDs(1, &cpPlatform, NULL);
	assert(!err);

	err = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, NULL);
	assert(!err);

	context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
	assert(context);

	queue = clCreateCommandQueue(context, device_id, 0, &err);
	assert(queue);

	program1 = clCreateProgramWithSource(context, 1,
				(const char **) & kernelSource, NULL, &err);
	assert(program1);

 	clBuildProgram(program1, 0, NULL, NULL, NULL, NULL);

        void *program_buffer;
        cl_uint program_buffer_size;
        err = clExportBinaryFormat(program1, &program_buffer, &program_buffer_size);
        assert(!err);

        size_t *binaries_sizes;
        unsigned char **binaries;
        err = clExtractBinaryFormat(program_buffer, program_buffer_size, &binaries_sizes, &binaries);

        program2 = clCreateProgramWithBinary(
          context, 1, &device_id, binaries_sizes, (const unsigned char **)binaries, NULL, &err);
        assert(!err);

 	clBuildProgram(program2, 0, NULL, NULL, NULL, NULL);

//###################################################################
//###################################################################
// Set up buffers in memory

	d_a = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, NULL);
	assert(d_a);
	d_b = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, NULL);
	assert(d_b);
	d_c1 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, NULL);
	assert(d_c1);
	d_c2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, NULL);
	assert(d_c2);
	d_c3 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, NULL);
	assert(d_c3);

	err = clEnqueueWriteBuffer(queue, d_a, CL_TRUE, 0,
						bytes, h_a, 0, NULL, NULL);
	assert(!err);
	err |= clEnqueueWriteBuffer(queue, d_b, CL_TRUE, 0,
						bytes, h_b, 0, NULL, NULL);
	assert(!err);

//###################################################################
//###################################################################
// Create kernels

	kernel1 = clCreateKernel(program1, "vecAdd", &err);
	assert(kernel1);

	err = clSetKernelArg(kernel1, 0, sizeof(cl_mem), &d_a);
	err |= clSetKernelArg(kernel1, 1, sizeof(cl_mem), &d_b);
	err |= clSetKernelArg(kernel1, 2, sizeof(cl_mem), &d_c1);
	assert(!err);

	kernel2 = clCreateKernel(program2, "vecAdd", &err);
	assert(kernel2);

	err = clSetKernelArg(kernel2, 0, sizeof(cl_mem), &d_a);
	err |= clSetKernelArg(kernel2, 1, sizeof(cl_mem), &d_b);
	err |= clSetKernelArg(kernel2, 2, sizeof(cl_mem), &d_c2);
	assert(!err);

	kernel3 = clCreateKernel(program2, "vecAdd", &err);
	assert(kernel3);

	err = clSetKernelArg(kernel3, 0, sizeof(cl_mem), &d_a);
	err |= clSetKernelArg(kernel3, 1, sizeof(cl_mem), &d_b);
	err |= clSetKernelArg(kernel3, 2, sizeof(cl_mem), &d_c3);
	assert(!err);

//###################################################################
//###################################################################
// Enqueue kernels

	localSize[0] = 4;
	localSize[1] = 8;        
	globalSize[0] = 16;
	globalSize[1] = 64;
        err = clEnqueueNDRangeKernel(queue, kernel1, 2, NULL, globalSize, localSize,
                                     0, NULL, NULL);
        assert(!err);

        err = clEnqueueNDRangeKernel(queue, kernel2, 2, NULL, globalSize, localSize,
                                     0, NULL, NULL);
        assert(!err);

	localSize[0] = 4;
	localSize[1] = 8;
	globalSize[0] = 16;
	globalSize[1] = 16;
        err = clEnqueueNDRangeKernel(queue, kernel3, 2, NULL, globalSize, localSize,
                                     0, NULL, NULL);
        assert(!err);

//###################################################################
//###################################################################
// Read output buffers

        clFinish(queue);

        clEnqueueReadBuffer(queue, d_c1, CL_TRUE, 0,
                            bytes, h_c1, 0, NULL, NULL);
        clEnqueueReadBuffer(queue, d_c2, CL_TRUE, 0,
                            bytes, h_c2, 0, NULL, NULL);
        clEnqueueReadBuffer(queue, d_c3, CL_TRUE, 0,
                            bytes, h_c3, 0, NULL, NULL);

//###################################################################
//###################################################################
// Check output of each kernels

        for(i = 0; i < BUFFER_SIZE; i++) {
                if(h_c1[i] != h_c2[i]) {
                        printf("Check failed at offset %d, %i instead of %i\n", i, h_c2[i], h_c1[i]);
                        exit(1);
                }
                printf("%i - %i\n", h_c1[i], h_c3[i]); 
        }

//###################################################################
//###################################################################
// Release everythings

	clReleaseMemObject(d_a);
	clReleaseMemObject(d_b);
	clReleaseMemObject(d_c1);
	clReleaseMemObject(d_c2);
	clReleaseMemObject(d_c3);
	clReleaseProgram(program1);
	clReleaseProgram(program2);
	clReleaseKernel(kernel1);
	clReleaseKernel(kernel2);
 	clReleaseKernel(kernel3);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

	free(h_a);
	free(h_b);
	free(h_c1);
	free(h_c2);
	free(h_c3);
        free(binaries);
        free(binaries_sizes);
        free(program_buffer);

	return 0;
}
