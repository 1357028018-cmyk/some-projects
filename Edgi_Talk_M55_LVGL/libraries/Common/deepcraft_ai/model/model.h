/*
* ImagiNet Compiler 5.13.5729.0+9fdc76d9f295d32de8260f13ae53677c59192b10
* Copyright © 2023- Imagimob AB, All Rights Reserved.
* 
* Generated at 07/06/2026 05:44:49 UTC. Any changes will be lost.
* 
* Model ID  97e32e38-02d8-47e6-84db-217ceaf09165
* 
* Memory    Size                      Efficiency
* Buffers   261 bytes (RAM)           100 %
* State     24592 bytes (RAM)         100 %
* Readonly  24928 bytes (Flash)       100 %
* 
* Exported functions:
* 
*  @param data_in Input features. Input float[256].
*  @param data_out Output features. Output float[5].
*  @return IPWIN_RET_SUCCESS (0) or IPWIN_RET_NODATA (-1), IPWIN_RET_ERROR (-2), IPWIN_RET_STREAMEND (-3)
*  int IMPressure_compute(const float *data_in, float *data_out);
* 
*  @description: Closes and flushes streams, free any heap allocated memory.
*  void IMPressure_finalize(void);
* 
*  @description: Resets windows and neural networks(i.e. RNNs) to initial state.
*  @return IPWIN_RET_SUCCESS (0) or IPWIN_RET_NODATA (-1), IPWIN_RET_ERROR (-2), IPWIN_RET_STREAMEND (-3)
*  int IMPressure_soft_reset(void);
* 
*  @description: Initializes buffers to initial state.
*  @return IPWIN_RET_SUCCESS (0) or IPWIN_RET_NODATA (-1), IPWIN_RET_ERROR (-2), IPWIN_RET_STREAMEND (-3)
*  int IMPressure_init(void);
* 
* 
* Disclaimer:
*   The generated code relies on the optimizations done by the C compiler.
*   For example many for-loops of length 1 must be removed by the optimizer.
*   This can only be done if the functions are inlined and simplified.
*   Check disassembly if unsure.
*   tl;dr Compile using gcc with -O3 or -Ofast
* 
* Notes:
* 	-> This code was generated with DEEPCRAFT Studio using:
* 		ml-coretools 3.2.0.9646.
* 		tensorflow 2.19.0.
* 		ethos-u-vela 4.5.0.
* 	-> This code requires the following Modus Toolbox libraries (add them to your
* 	project using the Library Manager):
* 		ml-middleware 3.2.0.
* 		ml-tflite-micro 3.2.0.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mtb_ml_model.h"
#define IMPRESSURE_API_FUNCTION

typedef int8_t q7_t;         // 8-bit fractional data type in Q1.7 format.
typedef int16_t q15_t;       // 16-bit fractional data type in Q1.15 format.
typedef int32_t q31_t;       // 32-bit fractional data type in Q1.31 format.
typedef int64_t q63_t;       // 64-bit fractional data type in Q1.63 format.
typedef float timestamp_t;

// Model GUID (16 bytes)
#define IMPRESSURE_MODEL_ID {0x38, 0x2e, 0xe3, 0x97, 0xd8, 0x02, 0xe6, 0x47, 0x84, 0xdb, 0x21, 0x7c, 0xea, 0xf0, 0x91, 0x65}


// First nibble is bit encoding, second nibble is number of bytes
#define IMAGINET_TYPES_NONE	(0x0)
#define IMAGINET_TYPES_FLOAT32	(0x14)
#define IMAGINET_TYPES_FLOAT64	(0x18)
#define IMAGINET_TYPES_INT8	(0x21)
#define IMAGINET_TYPES_INT16	(0x22)
#define IMAGINET_TYPES_INT32	(0x24)
#define IMAGINET_TYPES_INT64	(0x28)
#define IMAGINET_TYPES_Q7	(0x31)
#define IMAGINET_TYPES_Q15	(0x32)
#define IMAGINET_TYPES_Q31	(0x34)
#define IMAGINET_TYPES_BOOL	(0x41)
#define IMAGINET_TYPES_STRING	(0x54)
#define IMAGINET_TYPES_D8	(0x61)
#define IMAGINET_TYPES_D16	(0x62)
#define IMAGINET_TYPES_D32	(0x64)
#define IMAGINET_TYPES_UINT8	(0x71)
#define IMAGINET_TYPES_UINT16	(0x72)
#define IMAGINET_TYPES_UINT32	(0x74)
#define IMAGINET_TYPES_UINT64	(0x78)


#define IMPRESSURE_COMPUTE_INPUTS (1)
#define IMPRESSURE_COMPUTE_OUTPUTS (1)
#define IMPRESSURE_COMPUTE_IN_TYPE float
#define IMPRESSURE_COMPUTE_IN_TYPE_ID IMAGINET_TYPES_FLOAT32
#define IMPRESSURE_COMPUTE_OUT_TYPE float
#define IMPRESSURE_COMPUTE_OUT_TYPE_ID IMAGINET_TYPES_FLOAT32
#define IMPRESSURE_COMPUTE_OUT_NO_COPY false

// data_in [256] (1024 bytes)
#define IMPRESSURE_DATA_IN_RANK (1)
#define IMPRESSURE_DATA_IN_SHAPE ((int[]){256})
#define IMPRESSURE_DATA_IN_COUNT (256)
#define IMPRESSURE_DATA_IN_BYTES (1024)
#define IMPRESSURE_DATA_IN_TYPE float
#define IMPRESSURE_DATA_IN_TYPE_ID IMAGINET_TYPES_FLOAT32
#define IMPRESSURE_DATA_IN_SHIFT 0
#define IMPRESSURE_DATA_IN_OFFSET 0
#define IMPRESSURE_DATA_IN_SCALE 1
#define IMPRESSURE_DATA_IN_SYMBOLS { }

// data_out [5] (20 bytes)
#define IMPRESSURE_DATA_OUT_RANK (1)
#define IMPRESSURE_DATA_OUT_SHAPE ((int[]){5})
#define IMPRESSURE_DATA_OUT_COUNT (5)
#define IMPRESSURE_DATA_OUT_BYTES (20)
#define IMPRESSURE_DATA_OUT_TYPE float
#define IMPRESSURE_DATA_OUT_TYPE_ID IMAGINET_TYPES_FLOAT32
#define IMPRESSURE_DATA_OUT_SHIFT 0
#define IMPRESSURE_DATA_OUT_OFFSET 0
#define IMPRESSURE_DATA_OUT_SCALE 1
#define IMPRESSURE_DATA_OUT_SYMBOLS {"unlabeled", "supine", "left_lateral", "right_lateral", "noise"}

#define IMPRESSURE_KEY_MAX (6)

// Return codes
#define IMPRESSURE_RET_SUCCESS 0
#define IMPRESSURE_RET_NODATA -1
#define IMPRESSURE_RET_ERROR -2
#define IMPRESSURE_RET_STREAMEND -3

#define IPWIN_RET_SUCCESS 0
#define IPWIN_RET_NODATA -1
#define IPWIN_RET_ERROR -2
#define IPWIN_RET_STREAMEND -3

// Exported methods
int IMPressure_compute(const float *restrict data_in, float *restrict data_out);
void IMPressure_finalize(void);
int IMPressure_soft_reset(void);
int IMPressure_init(void);

// Symbol IMPRESSURE_PROFILING must be defined to enable profiling of models
// Symbol IMPRESSURE_PROFILING_LOG will enable printing the raw outputs of neural networks
/// @brief This method will print the region profiling results
void IMPRESSURE_print_region_profiling(void);
/// @brief Point this function pointer to your custom tick count function. This should populate the value pointed to by val with the current tick count.
extern int (*IMPRESSURE_get_ticks_ptr)(uint64_t*);

/// @brief This method will print neural network inference profiling results
void IMPRESSURE_mtb_models_profile_log();
/// @brief This method will print neural network information
void IMPRESSURE_mtb_models_print_info();
extern uint8_t IMPRESSURE_mtb_models_count;
extern mtb_ml_model_t* IMPRESSURE_mtb_models[];

// Profiling regions
#ifdef IMPRESSURE_PROFILING
    #define IMPRESSURE_REGIONS_COUNT 2
    #define IMPRESSURE_REGIONS_NAMES {\
    	"PREPROCESSOR",\
    	"NETWORK",\
    }
    #define IMPRESSURE_REGIONS_NOTES {\
    	NULL,\
    	NULL,\
    }
#else
    #define IMPRESSURE_REGIONS_COUNT 0
    #define IMPRESSURE_REGIONS_NAMES {}
    #define IMPRESSURE_REGIONS_NOTES {}
#endif

// Call macros — invoke any exported function via a void* array of arguments
#define IMPRESSURE_COMPUTE_PTR(a) IMPressure_compute((const float *)(a)[0], (float *)(a)[1])
#define IMPRESSURE_FINALIZE_PTR(a) IMPressure_finalize()
#define IMPRESSURE_SOFT_RESET_PTR(a) IMPressure_soft_reset()
#define IMPRESSURE_INIT_PTR(a) IMPressure_init()

typedef enum {
    IMPRESSURE_PARAM_UNDEFINED = 0,
    IMPRESSURE_PARAM_INPUT = 1,
    IMPRESSURE_PARAM_OUTPUT = 2,
    IMPRESSURE_PARAM_REFERENCE = 3,
    IMPRESSURE_PARAM_HANDLE = 7,
    IMPRESSURE_PARAM_CALLBACK = 8,
    IMPRESSURE_PARAM_OUTPUT_REF = 18,
} IMPRESSURE_param_attrib;

typedef char *label_text_t;

typedef struct {
    char* name;
    int size;
    label_text_t *labels;
} IMPRESSURE_shape_dim;

typedef struct {
    char* name;
    IMPRESSURE_param_attrib attrib;
    int32_t rank;
    IMPRESSURE_shape_dim *shape;
    int32_t count;
    int32_t bytes;
    int32_t type_id;
    float frequency;
    int shift;
    float scale;
    long offset;
} IMPRESSURE_param_def;

typedef enum {
    IMPRESSURE_FUNC_ATTRIB_NONE = 0,
    IMPRESSURE_FUNC_ATTRIB_CAN_FAIL = 1,
    IMPRESSURE_FUNC_ATTRIB_PUBLIC = 2,
    IMPRESSURE_FUNC_ATTRIB_INIT = 4,
    IMPRESSURE_FUNC_ATTRIB_DESTRUCTOR = 8,
} IMPRESSURE_func_attrib;

typedef struct {
    char* name;
    char* description;
    void* fn_ptr;
    IMPRESSURE_func_attrib attrib;
    int32_t param_count;
    IMPRESSURE_param_def *param_list;
} IMPRESSURE_func_def;

typedef struct {
    uint32_t size;
    uint32_t peak_usage;
} IMPRESSURE_mem_usage;

typedef enum {
    IMPRESSURE_API_TYPE_UNDEFINED = 0,
    IMPRESSURE_API_TYPE_FUNCTION = 1,
    IMPRESSURE_API_TYPE_QUEUE = 2,
    IMPRESSURE_API_TYPE_QUEUE_TIME = 3,
    IMPRESSURE_API_TYPE_CALLBACK = 4,
    IMPRESSURE_API_TYPE_CALLBACK_TIME = 5,
} IMPRESSURE_api_type;

typedef struct {
    uint32_t api_ver;
    uint8_t id[16];
    IMPRESSURE_api_type api_type;
    char* prefix;
    IMPRESSURE_mem_usage buffer_mem;
    IMPRESSURE_mem_usage static_mem;
    IMPRESSURE_mem_usage readonly_mem;
    int32_t func_count;
    IMPRESSURE_func_def *func_list;
} IMPRESSURE_api_def;

IMPRESSURE_api_def *IMPRESSURE_api(void);

#define IMPRESSURE_INPUT_META_COUNT 1
#define IMPRESSURE_OUTPUT_META_COUNT 1
#define IMPRESSURE_INPUT_META(i) ((IMPRESSURE_param_def*)(&IMPRESSURE_api()->func_list[0].param_list[(i)]))
#define IMPRESSURE_OUTPUT_META(i) ((IMPRESSURE_param_def*)(&IMPRESSURE_api()->func_list[0].param_list[(i) + 1]))

