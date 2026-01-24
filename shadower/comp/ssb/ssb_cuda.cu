#define THREADS_PER_BLOCK 256
#include "shadower/comp/ssb/ssb_cuda.cuh"
#include "srsran/phy/ch_estimation/dmrs_pbch.h"
#include "srsran/phy/utils/vector.h"
#include <chrono>
#include <complex>
#include <cuda_runtime_api.h>
#include <cufft.h>
#include <thrust/device_vector.h>
#include <thrust/extrema.h>
#include <thrust/reduce.h>
#include <vector>

// Kernel for complex conjugate multiplication in the frequency domain
__global__ void
complex_conj_mult_slide_window(cufftComplex* input, cufftComplex* pss_seq, cufftComplex* output, uint32_t pss_size)
{
  int seg_idx     = blockIdx.y;
  int element_idx = blockIdx.x * blockDim.x + threadIdx.x;
  int idx         = seg_idx * pss_size + element_idx;

  if (element_idx < pss_size) {
    cufftComplex e   = input[idx];
    cufftComplex pss = pss_seq[element_idx];
    pss.y            = -pss.y;
    output[idx].x    = e.x * pss.x - e.y * pss.y;
    output[idx].y    = e.x * pss.y + e.y * pss.x;
  }
}

// Kernel to compute the absolute squared magnitude (correlation power)
__global__ void compute_power(cufftComplex* input, float* power, int N)
{
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < N) {
    power[idx] = input[idx].x * input[idx].x + input[idx].y * input[idx].y; // |z|^2
  }
}

__global__ void find_max_kernel(float* d_data, int size, float* d_max_val, int* d_max_idx)
{
  __shared__ float shared_data[1024];
  __shared__ float shared_idx[1024];

  int tid = threadIdx.x;
  int idx = blockIdx.x * blockDim.x + tid;

  // Load data into shared memory
  if (idx < size) {
    shared_data[tid] = d_data[idx];
    shared_idx[tid]  = idx;
  } else {
    shared_data[tid] = -1e10f; // Very small value for comparison
    shared_idx[tid]  = -1;
  }
  __syncthreads();

  // Perform reduction to find max value
  for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
    if (tid < stride) {
      if (shared_data[tid] < shared_data[tid + stride]) {
        shared_data[tid] = shared_data[tid + stride];
        shared_idx[tid]  = shared_idx[tid + stride];
      }
    }
    __syncthreads();
  }

  // Write the result of this block to global memory
  if (tid == 0) {
    d_max_val[blockIdx.x] = shared_data[0];
    d_max_idx[blockIdx.x] = shared_idx[0];
  }
}

void SSBCuda::find_max(float* d_data, int size, float* max_val, int* max_idx)
{
  // clang-format off
  find_max_kernel<<<compareBlocksPerGrid, THREADS_PER_BLOCK, THREADS_PER_BLOCK * (sizeof(float) + sizeof(int))>>>(d_data, size, d_block_max_vals, d_block_max_idxs);
  // clang-format on
  cudaMemcpyAsync(
      h_block_max_vals, d_block_max_vals, compareBlocksPerGrid * sizeof(float), cudaMemcpyDeviceToHost, *stream);
  cudaMemcpyAsync(
      h_block_max_idxs, d_block_max_idxs, compareBlocksPerGrid * sizeof(int), cudaMemcpyDeviceToHost, *stream);
  // Final reduction on CPU
  *max_val = h_block_max_vals[0];
  *max_idx = h_block_max_idxs[0];
  for (int i = 1; i < compareBlocksPerGrid; i++) {
    if (h_block_max_vals[i] > *max_val) {
      *max_val = h_block_max_vals[i];
      *max_idx = h_block_max_idxs[i];
    }
  }
}

SSBCuda::SSBCuda(double                      srate_,
                 double                      dl_freq_,
                 double                      ssb_freq_,
                 srsran_subcarrier_spacing_t scs_,
                 srsran_ssb_pattern_t        pattern_,
                 srsran_duplex_mode_t        duplex_mode_) :
  srate(srate_), dl_freq(dl_freq_), ssb_freq(ssb_freq_), scs(scs_), pattern(pattern_), duplex_mode(duplex_mode_)
{
}

SSBCuda::~SSBCuda() {}

void SSBCuda::cleanup()
{
  if (h_pin_time) {
    cudaFreeHost(h_pin_time);
  }
  if (d_time) {
    cudaFree(d_time);
  }
  if (d_freq) {
    cudaFree(d_freq);
  }
  if (d_corr) {
    cudaFree(d_corr);
  }
  if (d_pss_seq) {
    cudaFree(d_pss_seq);
  }
  if (d_corr_mag) {
    cudaFree(d_corr_mag);
  }
  if (fft_plan) {
    cufftDestroy(fft_plan);
  }
  if (d_block_max_idxs) {
    cudaFree(d_block_max_idxs);
  }
  if (d_block_max_vals) {
    cudaFree(d_block_max_vals);
  }
  if (h_block_max_idxs) {
    free(h_block_max_idxs);
  }
  if (h_block_max_vals) {
    free(h_block_max_vals);
  }
  if (stream) {
    cudaStreamDestroy(*stream);
  }
  srsran_ssb_free(&ssb);
}

bool SSBCuda::init(uint32_t N_id_2_)
{
  N_id_2                     = N_id_2_;
  srsran_ssb_args_t ssb_args = {};
  ssb_args.max_srate_hz      = srate;
  ssb_args.min_scs           = scs;
  ssb_args.enable_search     = true;
  ssb_args.enable_measure    = true;
  ssb_args.enable_decode     = true;
  if (srsran_ssb_init(&ssb, &ssb_args) != 0) {
    printf("Error initialize ssb\n");
    return false;
  }
  srsran_ssb_cfg_t ssb_cfg = {};
  ssb_cfg.srate_hz         = srate;
  ssb_cfg.center_freq_hz   = dl_freq;
  ssb_cfg.ssb_freq_hz      = ssb_freq;
  ssb_cfg.scs              = scs;
  ssb_cfg.pattern          = pattern;
  ssb_cfg.duplex_mode      = duplex_mode;
  ssb_cfg.periodicity_ms   = 10;
  if (srsran_ssb_set_cfg(&ssb, &ssb_cfg) < SRSRAN_SUCCESS) {
    printf("Error set srsran_ssb_set_cfg\n");
    return false;
  }

  total_len    = ssb.sf_sz + ssb.ssb_sz;
  last_len     = total_len;
  round        = (total_len + ssb.corr_window - 1) / ssb.corr_window;
  total_len    = round * ssb.corr_sz;
  int n[1]     = {(int)ssb.corr_sz};
  int embed[1] = {1};
  cufftPlanMany(&fft_plan, 1, n, embed, 1, ssb.corr_window, embed, 1, ssb.corr_sz, CUFFT_C2C, round);
  cufftPlan1d(&ifft_plan, ssb.corr_sz, CUFFT_C2C, round);
  cudaMallocHost((void**)&h_pin_time, total_len * sizeof(cufftComplex)); // Pinned memory
  cudaMalloc((void**)&d_time, total_len * sizeof(cufftComplex));         // Time domain buffer
  cudaMalloc((void**)&d_freq, total_len * sizeof(cufftComplex));         // Frequency domain buffer
  cudaMalloc((void**)&d_corr, total_len * sizeof(cufftComplex));         // Correlation result buffer
  cudaMalloc((void**)&d_pss_seq, ssb.corr_sz * sizeof(cufftComplex));    // PSS sequence buffer
  cudaMalloc((void**)&d_corr_mag, total_len * sizeof(cufftComplex));     // Correlation magnitude buffer

  // Copy pss sequence to device
  cudaMemcpy(d_pss_seq, ssb.pss_seq[N_id_2], ssb.corr_sz * sizeof(cufftComplex), cudaMemcpyHostToDevice);

  // Allocate memory for CUDA kernel to find max value
  compareBlocksPerGrid = (total_len + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
  cudaMalloc((void**)&d_block_max_vals, compareBlocksPerGrid * sizeof(float));
  cudaMalloc((void**)&d_block_max_idxs, compareBlocksPerGrid * sizeof(int));
  h_block_max_vals = (float*)malloc(compareBlocksPerGrid * sizeof(float));
  h_block_max_idxs = (int*)malloc(compareBlocksPerGrid * sizeof(int));

  // Create a CUDA stream for asynchronous data transfer
  stream = new cudaStream_t();
  cudaStreamCreate(stream);
  cufftSetStream(fft_plan, *stream);
  return true;
}

int SSBCuda::ssb_pss_find_cuda(cf_t* in, uint32_t nof_samples, uint32_t* found_delay)
{
  if (ssb.corr_sz == 0) {
    return -1;
  }
  /* Copy the end of last ssb_sz to current buffer */
  memcpy(h_pin_time, h_pin_time + last_len - ssb.ssb_sz, sizeof(cufftComplex) * ssb.ssb_sz);
  /* Copy the current input buffer to pin buffer */
  memcpy(h_pin_time + ssb.ssb_sz, in, sizeof(cufftComplex) * nof_samples);
  /* Keep tracking the total len */
  last_len = nof_samples + ssb.ssb_sz;
  /* Set the remaining buffer to zero */
  memset(h_pin_time + last_len, 0, sizeof(cufftComplex) * (total_len - last_len));

  /* Copy the data to cuda device */
  cudaMemcpyAsync(d_time, h_pin_time, sizeof(cufftComplex) * last_len, cudaMemcpyHostToDevice, *stream);

  /* Convert time domain data to frequency domain */
  cufftExecC2C(fft_plan, d_time, d_freq, CUFFT_FORWARD);

  /* Perform correlation between frequency domain and PSS sequence */
  dim3 numBlocks((ssb.corr_sz + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK, round);
  // clang-format off
  complex_conj_mult_slide_window<<<numBlocks, THREADS_PER_BLOCK>>>(d_freq, d_pss_seq, d_corr, ssb.corr_sz);
  // clang-format on

  /* Convert the frequency domain correlation to time domain */
  cufftExecC2C(ifft_plan, d_corr, d_corr, CUFFT_INVERSE);

  /* Compute the power of the correlation */
  int compareNumBlocks = total_len / THREADS_PER_BLOCK;
  // clang-format off
  compute_power<<<compareNumBlocks, THREADS_PER_BLOCK>>>(d_corr, d_corr_mag, total_len);
  // clang-format on
  float best_corr  = 0;
  int   best_delay = -1;
  find_max(d_corr_mag, total_len, &best_corr, &best_delay);
  int round_number, round_offset;
  round_number = best_delay / ssb.corr_sz;
  round_offset = best_delay % ssb.corr_sz;
  *found_delay = round_number * ssb.corr_window + round_offset;
  return 0;
}

int SSBCuda::ssb_run_sync_find(cf_t*                          buffer,
                               uint32_t                       N_id,
                               srsran_csi_trs_measurements_t* meas,
                               srsran_pbch_msg_nr_t*          pbch_msg)
{
  if (buffer == nullptr || meas == nullptr || pbch_msg == nullptr) {
    return -1;
  }
  SRSRAN_MEM_ZERO(pbch_msg, srsran_pbch_msg_nr_t, 1);

  /* Search for PSS in time domain */
  uint32_t t_offset = 0;
  if (ssb_pss_find_cuda(buffer, ssb.sf_sz, &t_offset) != 0) {
    logger.error("Error search for NID 2");
    return -1;
  }

  // Remove CP offset prior demodulation
  if (t_offset >= ssb.cp_sz) {
    t_offset -= ssb.cp_sz;
  } else {
    t_offset += 0;
  }
  // Make sure SSB time offset is in bounded in the input buffer
  if (t_offset > ssb.sf_sz) {
    return SRSRAN_SUCCESS;
  }

  // Demodulate
  cf_t ssb_grid[SRSRAN_SSB_NOF_RE] = {};
  if (ssb_demodulate(&ssb, (cf_t*)h_pin_time, t_offset, 0.0f, ssb_grid) < SRSRAN_SUCCESS) {
    ERROR("Error demodulating");
    return SRSRAN_ERROR;
  }

  // Measure selected N_id
  if (ssb_measure(&ssb, ssb_grid, N_id, meas)) {
    ERROR("Error measuring");
    return SRSRAN_ERROR;
  }

  // Select the most suitable SSB candidate
  uint32_t                n_hf      = 0;
  uint32_t                ssb_idx   = 0; // SSB candidate index
  srsran_dmrs_pbch_meas_t pbch_meas = {};
  if (ssb_select_pbch(&ssb, N_id, ssb_grid, &n_hf, &ssb_idx, &pbch_meas) < SRSRAN_SUCCESS) {
    ERROR("Error selecting PBCH");
    return SRSRAN_ERROR;
  }

  // Avoid decoding if the selected PBCH DMRS do not reach the minimum threshold
  if (pbch_meas.corr < ssb.args.pbch_dmrs_thr) {
    return SRSRAN_SUCCESS;
  }

  // Calculate the SSB offset in the subframe
  uint32_t ssb_offset = srsran_ssb_candidate_sf_offset(&ssb, ssb_idx);

  // Compute PBCH channel estimates
  if (ssb_decode_pbch(&ssb, N_id, n_hf, ssb_idx, ssb_grid, pbch_msg) < SRSRAN_SUCCESS) {
    ERROR("Error decoding PBCH");
    return SRSRAN_ERROR;
  }

  // SSB delay in SF
  float ssb_delay_us = (float)(1e6 * (((double)t_offset - (double)ssb.ssb_sz - (double)ssb_offset) / ssb.cfg.srate_hz));

  // Add delay to measure
  meas->delay_us += ssb_delay_us;
  meas->ssb_offset = t_offset - ssb.ssb_sz - ssb_offset;
  return SRSRAN_SUCCESS;
}
