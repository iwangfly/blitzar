#include "cbindings/inner_product.h"

#include <iostream>
#include <vector>

#include "cbindings/backend.h"
#include "sxt/base/container/span.h"
#include "sxt/base/error/assert.h"
#include "sxt/base/num/ceil_log2.h"
#include "sxt/proof/inner_product/proof_descriptor.h"
#include "sxt/seqcommit/generator/precomputed_generators.h"

using namespace sxt;

namespace sxt::cbn {
//--------------------------------------------------------------------------------------------------
// check_prove_inner_product_input
//--------------------------------------------------------------------------------------------------
static void check_prove_inner_product_input(sxt_compressed_ristretto* l_vector,
                                            sxt_compressed_ristretto* r_vector,
                                            sxt_scalar* ap_value, sxt_transcript* transcript,
                                            uint64_t n, const sxt_scalar* b_vector,
                                            const sxt_scalar* a_vector) noexcept {

  SXT_RELEASE_ASSERT(
      transcript != nullptr,
      "transcript must not be null in the `sxt_prove_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      ap_value != nullptr,
      "ap_value must not be null in the `sxt_prove_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      b_vector != nullptr,
      "b_vector must not be null in the `sxt_prove_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      a_vector != nullptr,
      "a_vector must not be null in the `sxt_prove_inner_product` c binding function");
  SXT_RELEASE_ASSERT(n > 0, "a_vector and b_vector lengths must be greater than zero in the "
                            "`sxt_prove_inner_product` c binding function");
  SXT_RELEASE_ASSERT(n == 1 || (l_vector != nullptr && r_vector != nullptr),
                     "l_vector and r_vector lengths must not be null when a_vector size is bigger "
                     "than one in the `sxt_prove_inner_product` c binding function");
}

//--------------------------------------------------------------------------------------------------
// check_verify_inner_product_input
//--------------------------------------------------------------------------------------------------
static void check_verify_inner_product_input(sxt_transcript* transcript, uint64_t n,
                                             const sxt_scalar* b_vector, const sxt_scalar* product,
                                             const sxt_ristretto* a_commit,
                                             const sxt_compressed_ristretto* l_vector,
                                             const sxt_compressed_ristretto* r_vector,
                                             const sxt_scalar* ap_value) noexcept {

  SXT_RELEASE_ASSERT(
      transcript != nullptr,
      "transcript must not be null in the `sxt_verify_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      ap_value != nullptr,
      "ap_value must not be null in the `sxt_verify_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      product != nullptr,
      "product must not be null in the `sxt_verify_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      a_commit != nullptr,
      "a_commit must not be null in the `sxt_verify_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      b_vector != nullptr,
      "b_vector must not be null in the `sxt_verify_inner_product` c binding function");
  SXT_RELEASE_ASSERT(n > 0, "b_vector length must be greater than zero in the "
                            "`sxt_verify_inner_product` c binding function");
  SXT_RELEASE_ASSERT(
      n == 1 || (l_vector != nullptr && r_vector != nullptr),
      "l_vector and r_vector lengths must not be null when a_vector "
      "size is bigger than one in the `sxt_verify_inner_product` c binding function");
}

} // namespace sxt::cbn

//--------------------------------------------------------------------------------------------------
// sxt_prove_inner_product
//--------------------------------------------------------------------------------------------------
void sxt_prove_inner_product(struct sxt_compressed_ristretto* l_vector,
                             struct sxt_compressed_ristretto* r_vector, struct sxt_scalar* ap_value,
                             struct sxt_transcript* transcript, uint64_t n,
                             uint64_t generators_offset, const struct sxt_scalar* a_vector,
                             const struct sxt_scalar* b_vector) {
  sxt::cbn::check_prove_inner_product_input(l_vector, r_vector, ap_value, transcript, n, b_vector,
                                            a_vector);

  auto n_lg2 = static_cast<size_t>(basn::ceil_log2(n));
  auto np = 1ull << n_lg2;

  // Note: it's inefficient calling `get_precomputed_generators` with
  // `use_gpu` set to false in case both `generators_offset + n > precomputed_elements_n`
  // and `SXT_GPU_BACKEND` is set to false. The execution would be slower than necessary.
  // However, if the backend is properly configured to use the right number of
  // `precomputed_elements_n`, then this call shouldn't matter.
  std::vector<c21t::element_p3> temp_generators_data;
  auto precomputed_generators =
      sqcgn::get_precomputed_generators(temp_generators_data, np + 1, generators_offset, false);

  prfip::proof_descriptor descriptor{
      .b_vector = {reinterpret_cast<const s25t::element*>(b_vector), n},
      .g_vector = {precomputed_generators.data(), np},
      .q_value = precomputed_generators.data() + np};

  auto backend = sxt::cbn::get_backend();
  backend->prove_inner_product({reinterpret_cast<rstt::compressed_element*>(l_vector), n_lg2},
                               {reinterpret_cast<rstt::compressed_element*>(r_vector), n_lg2},
                               *reinterpret_cast<s25t::element*>(ap_value),
                               *reinterpret_cast<prft::transcript*>(transcript), descriptor,
                               {reinterpret_cast<const s25t::element*>(a_vector), n});
}

//--------------------------------------------------------------------------------------------------
// sxt_verify_inner_product
//--------------------------------------------------------------------------------------------------
int sxt_verify_inner_product(struct sxt_transcript* transcript, uint64_t n,
                             uint64_t generators_offset, const struct sxt_scalar* b_vector,
                             const struct sxt_scalar* product, const struct sxt_ristretto* a_commit,
                             const struct sxt_compressed_ristretto* l_vector,
                             const struct sxt_compressed_ristretto* r_vector,
                             const struct sxt_scalar* ap_value) {
  // Even though the input should not be trusted,
  // we abort here in case of invalid input parameters
  sxt::cbn::check_verify_inner_product_input(transcript, n, b_vector, product, a_commit, l_vector,
                                             r_vector, ap_value);

  auto n_lg2 = static_cast<size_t>(basn::ceil_log2(n));
  auto np = 1ull << n_lg2;

  // Note: it's inefficient calling `get_precomputed_generators` with
  // `use_gpu` set to false in case both `generators_offset + n > precomputed_elements_n`
  // and `SXT_GPU_BACKEND` is set to false. The execution would be slower than necessary.
  // However, if the backend is properly configured to use the right number of
  // `precomputed_elements_n`, then this call shouldn't matter.
  std::vector<c21t::element_p3> temp_generators_data;
  auto precomputed_generators =
      sqcgn::get_precomputed_generators(temp_generators_data, np + 1, generators_offset, false);

  prfip::proof_descriptor descriptor{
      .b_vector = {reinterpret_cast<const s25t::element*>(b_vector), n},
      .g_vector = {precomputed_generators.data(), np},
      .q_value = precomputed_generators.data() + np};

  auto backend = sxt::cbn::get_backend();
  auto res = backend->verify_inner_product(
      *reinterpret_cast<prft::transcript*>(transcript), descriptor,
      *reinterpret_cast<const s25t::element*>(product),
      *reinterpret_cast<const c21t::element_p3*>(a_commit),
      {reinterpret_cast<const rstt::compressed_element*>(l_vector), n_lg2},
      {reinterpret_cast<const rstt::compressed_element*>(r_vector), n_lg2},
      *reinterpret_cast<const s25t::element*>(ap_value));

  return static_cast<int>(res);
}
