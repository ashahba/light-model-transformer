// Copyright (C) 2021-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#ifndef BERT_CONTEXT_H_
#define BERT_CONTEXT_H_

#include "dnnl_common.h"

#include <cstdlib>
#include <omp.h>
#include <vector>
#include <memory>
#include <cassert>

#define QUANT_INT8

class BertContext {

    using dt = dnnl::memory::data_type;
    using dims = dnnl::memory::dims;
    using dim = dnnl::memory::dim;

public:

    // All BERT models use same head size - 64
    // * Base: hiddenSize = 768, heads = 12
    // * Large: hiddenSize = 1024, heads = 16
    // TODO(rfsaliev) review correlation with the 'NumAttentionHeads' attribute
    static constexpr int head_size = 64;
    static constexpr int tensors_per_layer = 16;

    BertContext(int maxTokenSize = 128, int hiddenSize = 768, int intermediateSize = 3072, int batch = 1,
                int numLayers = 12, bool use_quantization = false, bool use_bfloat16 = false, bool calibrate_quant_factors = false)
        : maxTokenSize{maxTokenSize}
        , hiddenSize{hiddenSize}
        , intermediateSize{intermediateSize}
        , batch_{batch}
        , numLayers{numLayers}
        , numHeads{hiddenSize / head_size}
        , use_quantization{use_quantization}
        , use_bfloat16{use_bfloat16}
        , calibrate_quant_factors{calibrate_quant_factors}
        , query{dnnl::memory::desc{{batch * maxTokenSize, hiddenSize}, dt::f32, dims{}}, dnnl_context.getEngine()}
        , key  {dnnl::memory::desc{{batch * maxTokenSize, hiddenSize}, dt::f32, dims{}}, dnnl_context.getEngine()}
        , value{dnnl::memory::desc{{batch * maxTokenSize, hiddenSize}, dt::f32, dims{}}, dnnl_context.getEngine()}
        , resultBuffer1{dnnl::memory::desc{{batch * maxTokenSize, hiddenSize}, dt::f32, dims{}}, dnnl_context.getEngine()}
        , intermediateBuffer{dnnl::memory::desc{{batch * maxTokenSize, intermediateSize}, QuantizationType(), dims{}}, dnnl_context.getEngine()}
        , qk_resultBuffer{dnnl::memory::desc{{batch, hiddenSize / head_size, maxTokenSize, maxTokenSize}, dt::f32, dims{}}, dnnl_context.getEngine()}
    {
        assert(hiddenSize % head_size == 0);
    }

    dnnl::memory::data_type QuantizationType() const { return use_quantization ? dt::s8 : dt::f32; }

    dnnl::memory::data_type FloatType() const { return use_bfloat16 ? dt::bf16 : dt::f32; }



    int maxTokenSize;
    int hiddenSize;
    int intermediateSize;
    int batch_;
    int numLayers;
    int numHeads;
    bool use_quantization;
    bool use_bfloat16;
    bool calibrate_quant_factors;
    DnnlCommon dnnl_context;

    // Store the result of input*qkvWeight
    dnnl::memory query;
    dnnl::memory key;
    dnnl::memory value;
    // Buffer like the dimesion of 128x768
    dnnl::memory resultBuffer1;
    // Buffer to store the result of intermediate
    dnnl::memory intermediateBuffer;
    // Store the BatchMatMul result of query and key
    dnnl::memory qk_resultBuffer;
};



// Helper class to build a BertContext.
// In TF op we get some of the context arguments
// later than others, so we need a way to store them. 
// Also makes the arguments of the BertContext ctor more
// manageable.
class BertContextBuilder {
public:

    std::shared_ptr<BertContext> Build() {
        return std::make_shared<BertContext>(max_token_size, hidden_size, intermediate_size, batch_size, num_layers,
                                             use_quantization, use_bfloat16, calibrate_quant_factors);
    }

    void MaxTokenSize(int max_token_size) { this->max_token_size = max_token_size; }
    void HiddenSize(int hidden_size) { this->hidden_size = hidden_size; }
    void IntermediateSize(int intermediate_size) { this->intermediate_size = intermediate_size; }
    void BatchSize(int batch_size) { this->batch_size = batch_size; }
    void NumLayers(int num_layers) { this->num_layers = num_layers; }
    void NumAttentionHeads (int num_attention_heads) { this->num_attention_heads = num_attention_heads; }
    void UseQuantization (bool b) { this->use_quantization = b; }
    void UseBfloat16 (bool b) { this->use_bfloat16 = b; }
    void CalibrateQuantFactors(bool b) { this->calibrate_quant_factors = b; }

    int MaxTokenSize() const { return this->max_token_size; }
    int HiddenSize() const { return this->hidden_size; }
    int IntermediateSize() const { return this->intermediate_size; }
    int BatchSize() const { return this->batch_size; }
    int NumLayers() const { return this->num_layers; }
    int NumAttentionHeads() const { return this->num_attention_heads; }
    bool UseQuantization () const { return this->use_quantization; }
    bool UseBfloat16 () const { return this->use_bfloat16; }
    bool CalibrateQuantFactors() const { return this->calibrate_quant_factors; }

private:
    int max_token_size = 128;
    int hidden_size = 768;
    int intermediate_size = 3072;
    int batch_size = 1;
    int num_layers = 12;
    int num_attention_heads = 12;
    bool use_quantization = false;
    bool use_bfloat16 = false;
    bool calibrate_quant_factors = false;
};

#endif