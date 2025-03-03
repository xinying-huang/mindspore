project(nnacl)

set(NNACL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${NNACL_DIR}/..)

set(NNACL_SIMD_DIR ${CMAKE_BINARY_DIR}/src/nnacl)
include_directories(${NNACL_SIMD_DIR}/..)
file(GLOB SIMD_CONFIG_HEADER
    ${NNACL_DIR}/base/*_simd.h.in
    ${NNACL_DIR}/fp32/*_simd.h.in
    ${NNACL_DIR}/fp32_grad/*_simd.h.in
)
function(generate_simd_header_code)
    foreach(simd_config_file ${SIMD_CONFIG_HEADER})
        string(REGEX MATCHALL "[0-9A-Za-z_]*_simd.h.in" tmp1 ${simd_config_file})
        string(REGEX REPLACE "_simd.h.in" "_${SIMD_INSTRUCTION_LOWER}.h" tmp2 ${tmp1})
        string(REGEX REPLACE "_simd.h.in" "" tmp3 ${tmp1})
        string(TOLOWER ${tmp3} OP_NAME_LOWER)
        string(TOUPPER ${tmp3} OP_NAME_UPPER)
        configure_file(${NNACL_DIR}/op_simd_header_file.h.in ${NNACL_SIMD_DIR}/${tmp3}_simd.h)
    endforeach()
endfunction()

function(generate_simd_code SIMD_INSTRUCTION SIMD_BLOCK_NUM SIMD_TARGET)
    string(TOLOWER ${SIMD_INSTRUCTION} SIMD_INSTRUCTION_LOWER)
    set(SIMD_DEFINE "#define MS_SIMD_${SIMD_INSTRUCTION}")
    set(SIMD_UNDEF "#undef MS_SIMD_${SIMD_INSTRUCTION}")
    set(SIMD_DEF_INSTRUCTION "#define MS_SIMD_INSTRUCTION MS_SIMD_${SIMD_INSTRUCTION}_INSTRUCTION")
    set(SIMD_UNDEF_INSTRUCTION "#undef MS_SIMD_INSTRUCTION")
    set(SIMD_DEF_BLOCK_NUM "#define BLOCK_NUM ${SIMD_BLOCK_NUM}")
    set(SIMD_UNDEF_BLOCK_NUM "#undef BLOCK_NUM")
    if(SIMD_INSTRUCTION_LOWER STREQUAL "neon")
        set(SIMD_TARGET_BEGIN "")
        set(SIMD_TARGET_END "")
    else()
        set(SIMD_TARGET_BEGIN "#pragma GCC push_options\n#pragma GCC target(${SIMD_TARGET})")
        set(SIMD_TARGET_END "#pragma GCC pop_options")
    endif()

    set(SIMD_INSTRUCTION_BEGIN "${SIMD_TARGET_BEGIN}\n${SIMD_DEF_INSTRUCTION}\n${SIMD_DEF_BLOCK_NUM}\n${SIMD_DEFINE}")
    set(SIMD_INSTRUCTION_END "${SIMD_UNDEF_INSTRUCTION}\n${SIMD_UNDEF_BLOCK_NUM}\n${SIMD_TARGET_END}\n${SIMD_UNDEF}")
    foreach(simd_config_file ${SIMD_CONFIG_HEADER})
        string(REGEX MATCHALL "[0-9A-Za-z_]*_simd.h.in" tmp1 ${simd_config_file})
        string(REGEX REPLACE "_simd.h.in" "_${SIMD_INSTRUCTION_LOWER}.h" tmp2 ${tmp1})
        configure_file(${simd_config_file} ${NNACL_SIMD_DIR}/${SIMD_INSTRUCTION_LOWER}/${tmp2})
    endforeach()
endfunction()
generate_simd_code(NEON 4 \"\")
generate_simd_code(SSE 4 \"sse4.1\")
generate_simd_code(AVX 8 "\"avx\", \"avx2\"")
generate_simd_code(AVX512 16 \"avx512f\")
generate_simd_header_code()

if(ENABLE_CPU)
    set(CMAKE_C_FLAGS "-Wno-attributes ${CMAKE_C_FLAGS}")
endif()

if(APPLE OR PLATFORM_ARM32 OR PLATFORM_ARM64 OR PLATFORM_MCU)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND DEFINED ARCHS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -fstrict-aliasing \
        -ffunction-sections -fdata-sections -ffast-math -Wno-shorten-64-to-32")
    endif()
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND NOT DEFINED ARCHS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -fomit-frame-pointer -fstrict-aliasing \
        -ffunction-sections -fdata-sections -ffast-math")
    endif()
endif()

if(NOT MSVC)
    if("${X86_64_SIMD}" STREQUAL "sse" OR "${X86_64_SIMD}" STREQUAL "avx" OR "${X86_64_SIMD}" STREQUAL "avx512")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1")
    endif()
    if("${X86_64_SIMD}" STREQUAL "avx" OR "${X86_64_SIMD}" STREQUAL "avx512")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx -mavx2 -mfma")
    endif()
    if(WIN32)
        if("${X86_64_SIMD}" STREQUAL "avx512")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx512f -fno-asynchronous-unwind-tables")
        endif()
    endif()
endif()


########################### files ###########################
file(GLOB KERNEL_SRC
    ${NNACL_DIR}/*.c
    ${NNACL_DIR}/fp32/*.c
    ${NNACL_DIR}/infer/*.c
    ${NNACL_DIR}/base/*.c
    ${NNACL_DIR}/fp32_grad/*.c
    ${NNACL_DIR}/kernel/*.c
    ${NNACL_DIR}/experimental/*.c
)

if(NOT MSLITE_ENABLE_RUNTIME_PASS)
  list(REMOVE_ITEM KERNEL_SRC ${NNACL_DIR}/infer/shape_fusion_infer.c)
endif()
if((NOT DEFINED MSLITE_ENABLE_INT8) OR MSLITE_ENABLE_INT8)
    file(GLOB KERNEL_SRC_INT8
            ${NNACL_DIR}/int8/*.c
            )
    set(KERNEL_SRC
            ${KERNEL_SRC}
            ${KERNEL_SRC_INT8}
            )
else()
    set(KERNEL_SRC
            ${KERNEL_SRC}
            ${NNACL_DIR}/int8/pack_int8.c
            ${NNACL_DIR}/int8/quantize.c
            )
endif()

if(MSLITE_ENABLE_SPARSE_COMPUTE)
    file(GLOB KERNEL_SRC_SPARSE
            ${NNACL_DIR}/fp32_sparse/*.c
            )
    set(KERNEL_SRC
            ${KERNEL_SRC}
            ${KERNEL_SRC_SPARSE}
            )
endif()

if(MSLITE_ENABLE_STRING_KERNEL)
    file(GLOB KERNEL_SRC_INFER_STRING
            ${NNACL_DIR}/infer/string/*.c
            )
    set(KERNEL_SRC
            ${KERNEL_SRC}
            ${KERNEL_SRC_INFER_STRING}
            )
endif()
if(MSLITE_ENABLE_CONTROLFLOW)
    file(GLOB KERNEL_SRC_INFER_CONTROL_TENSORLIST
            ${NNACL_DIR}/infer/control/*.c
            )
    set(KERNEL_SRC
            ${KERNEL_SRC}
            ${KERNEL_SRC_INFER_CONTROL_TENSORLIST}
            )
endif()
if(PLATFORM_ARM64)
    file(GLOB ASSEMBLY_SRC ${NNACL_DIR}/assembly/arm64/*.S)
    set_property(SOURCE ${ASSEMBLY_SRC} PROPERTY LANGUAGE C)
endif()

if(PLATFORM_ARM32)
    file(GLOB ASSEMBLY_SRC ${NNACL_DIR}/assembly/arm32/*.S)
    set_property(SOURCE ${ASSEMBLY_SRC} PROPERTY LANGUAGE C)
endif()

if("${X86_64_SIMD}" STREQUAL "sse")
    file(GLOB ASSEMBLY_SRC ${NNACL_DIR}/intrinsics/sse/*.c)
    set_property(SOURCE ${ASSEMBLY_SRC} PROPERTY LANGUAGE C)
endif()

if("${X86_64_SIMD}" STREQUAL "avx")
    file(GLOB ASSEMBLY_SRC ${NNACL_DIR}/intrinsics/sse/*.c
            ${NNACL_DIR}/intrinsics/avx/*.c
            ${NNACL_DIR}/assembly/avx/*.S
            ${NNACL_DIR}/intrinsics/ms_simd_cpu_info.c)
    set_property(SOURCE ${ASSEMBLY_SRC} PROPERTY LANGUAGE C)
endif()

if("${X86_64_SIMD}" STREQUAL "avx512")
    file(GLOB ASSEMBLY_SRC ${NNACL_DIR}/intrinsics/sse/*.c
            ${NNACL_DIR}/intrinsics/avx/*.c
            ${NNACL_DIR}/assembly/avx/*.S
            ${NNACL_DIR}/intrinsics/ms_simd_cpu_info.c)
    set_property(SOURCE ${ASSEMBLY_SRC} PROPERTY LANGUAGE C)

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
        file(GLOB HPC_SRC ${NNACL_DIR}/experimental/HPC-generator/gemm_avx512/*.c)
        set_property(SOURCE ${HPC_SRC} PROPERTY LANGUAGE C)
    endif()

    set(MS_X86_AVX512_SRC ${HPC_SRC}
                          ${NNACL_DIR}/fp32/matmul_avx512_fp32.c)

    set_source_files_properties(${MS_X86_AVX512_SRC} PROPERTIES LANGUAGE C
        COMPILE_FLAGS "${CMAKE_C_FLAGS} -mavx512f -fPIC")

endif()

if(APPLE)
    set_source_files_properties(${ASSEMBLY_SRC} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
endif()

########################### build nnacl library ########################
string(REPLACE "-fvisibility=hidden" "-fvisibility=default" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

add_library(nnacl_mid OBJECT ${KERNEL_SRC} ${TRAIN_SRC} ${ASSEMBLY_SRC} ${MS_X86_AVX512_SRC})

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(nnacl_mid PRIVATE ENABLE_DEBUG)
endif()

if(ENABLE_CPU)
    if(${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64")
        target_compile_definitions(nnacl_mid PRIVATE ENABLE_ARM ENABLE_ARM64 ENABLE_NEON)
        target_compile_options(nnacl_mid PRIVATE -ffast-math -flax-vector-conversions)
    elseif("${X86_64_SIMD}" STREQUAL "sse")
        target_compile_definitions(nnacl_mid PRIVATE ENABLE_SSE)
    elseif("${X86_64_SIMD}" STREQUAL "avx")
        target_compile_definitions(nnacl_mid PRIVATE ENABLE_SSE ENABLE_AVX)
    elseif("${X86_64_SIMD}" STREQUAL "avx512")
        target_compile_definitions(nnacl_mid PRIVATE ENABLE_SSE ENABLE_AVX ENABLE_AVX512)
    endif()
    target_compile_options(nnacl_mid PRIVATE -fPIC)
    add_library(nnacl SHARED $<TARGET_OBJECTS:nnacl_mid>)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
        if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
            target_link_options(nnacl PRIVATE -Wl,-z,relro,-z,now,-z,noexecstack)
            target_link_libraries(nnacl PRIVATE m)
        endif()
        if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
            target_link_options(nnacl PRIVATE -s)
        endif()
    endif()
endif()

set(nnacl_static_obj $<TARGET_OBJECTS:nnacl_mid>)
########################### arm fp16 build optimize library ########################
if(PLATFORM_ARM)
    add_subdirectory(${NNACL_DIR}/optimize)
    if(TARGET nnacl_optimize_mid)
        set(nnacl_static_obj ${nnacl_static_obj} $<TARGET_OBJECTS:nnacl_optimize_mid>)
    endif()
    if(TARGET nnacl_fp16_mid)
        set(nnacl_static_obj ${nnacl_static_obj} $<TARGET_OBJECTS:nnacl_fp16_mid>)
    endif()
endif()
add_library(nnacl_static STATIC ${nnacl_static_obj})
set_target_properties(nnacl_static PROPERTIES OUTPUT_NAME "nnacl")
set_target_properties(nnacl_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
