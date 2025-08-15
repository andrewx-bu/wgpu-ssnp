#include "ssnp_diffract.h"

// INPUT PARAMS
struct Params {
    float dz;
};

static size_t buffer_len;
static size_t res_buffer_len;

// CREATING BIND GROUP AND LAYOUT
static wgpu::BindGroupLayout createBindGroupLayout(wgpu::Device& device) {
    wgpu::BindGroupLayoutEntry ufBufferLayout = {};
    ufBufferLayout.binding = 0;
    ufBufferLayout.visibility = wgpu::ShaderStage::Compute;
    ufBufferLayout.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    wgpu::BindGroupLayoutEntry ubBufferLayout = {};
    ubBufferLayout.binding = 1;
    ubBufferLayout.visibility = wgpu::ShaderStage::Compute;
    ubBufferLayout.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    wgpu::BindGroupLayoutEntry resBufferLayout = {};
    resBufferLayout.binding = 2;
    resBufferLayout.visibility = wgpu::ShaderStage::Compute;
    resBufferLayout.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    wgpu::BindGroupLayoutEntry cgammaBufferLayout = {};
    cgammaBufferLayout.binding = 3;
    cgammaBufferLayout.visibility = wgpu::ShaderStage::Compute;
    cgammaBufferLayout.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

    wgpu::BindGroupLayoutEntry newUFBufferLayout = {};
    newUFBufferLayout.binding = 4;
    newUFBufferLayout.visibility = wgpu::ShaderStage::Compute;
    newUFBufferLayout.buffer.type = wgpu::BufferBindingType::Storage;

    wgpu::BindGroupLayoutEntry newUBBufferLayout = {};
    newUBBufferLayout.binding = 5;
    newUBBufferLayout.visibility = wgpu::ShaderStage::Compute;
    newUBBufferLayout.buffer.type = wgpu::BufferBindingType::Storage;

    wgpu::BindGroupLayoutEntry uniformBufferLayout = {};
    uniformBufferLayout.binding = 6;
    uniformBufferLayout.visibility = wgpu::ShaderStage::Compute;
    uniformBufferLayout.buffer.type = wgpu::BufferBindingType::Uniform;

    wgpu::BindGroupLayoutEntry entries[] = {ufBufferLayout, ubBufferLayout, resBufferLayout, cgammaBufferLayout, newUFBufferLayout, newUBBufferLayout, uniformBufferLayout};

    wgpu::BindGroupLayoutDescriptor layoutDesc = {};
    layoutDesc.entryCount = 7;
    layoutDesc.entries = entries;

    return device.createBindGroupLayout(layoutDesc);
}

static wgpu::BindGroup createBindGroup(
    wgpu::Device& device, 
    wgpu::BindGroupLayout bindGroupLayout, 
    wgpu::Buffer ufBuffer, 
    wgpu::Buffer ubBuffer, 
    wgpu::Buffer resBuffer, 
    wgpu::Buffer cgammaBuffer, 
    wgpu::Buffer newUFBuffer, 
    wgpu::Buffer newUBBuffer, 
    wgpu::Buffer uniformBuffer
) {
    wgpu::BindGroupEntry ufEntry = {};
    ufEntry.binding = 0;
    ufEntry.buffer = ufBuffer;
    ufEntry.offset = 0;
    ufEntry.size = sizeof(float) * 2 * buffer_len;

    wgpu::BindGroupEntry ubEntry = {};
    ubEntry.binding = 1;
    ubEntry.buffer = ubBuffer;
    ubEntry.offset = 0;
    ubEntry.size = sizeof(float) * 2 * buffer_len;

    wgpu::BindGroupEntry resEntry = {};
    resEntry.binding = 2;
    resEntry.buffer = resBuffer;
    resEntry.offset = 0;
    resEntry.size = sizeof(float) * res_buffer_len;

    wgpu::BindGroupEntry cgammaEntry = {};
    cgammaEntry.binding = 3;
    cgammaEntry.buffer = cgammaBuffer;
    cgammaEntry.offset = 0;
    cgammaEntry.size = sizeof(float) * buffer_len;

    wgpu::BindGroupEntry newUFEntry = {};
    newUFEntry.binding = 4;
    newUFEntry.buffer = newUFBuffer;
    newUFEntry.offset = 0;
    newUFEntry.size = sizeof(float) * 2 * buffer_len;

    wgpu::BindGroupEntry newUBEntry = {};
    newUBEntry.binding = 5;
    newUBEntry.buffer = newUBBuffer;
    newUBEntry.offset = 0;
    newUBEntry.size = sizeof(float) * 2 * buffer_len;

    wgpu::BindGroupEntry uniformEntry = {};
    uniformEntry.binding = 6;
    uniformEntry.buffer = uniformBuffer;
    uniformEntry.offset = 0;
    uniformEntry.size = sizeof(Params);

    wgpu::BindGroupEntry entries[] = {ufEntry, ubEntry, resEntry, cgammaEntry, newUFEntry, newUBEntry, uniformEntry};

    wgpu::BindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = 7;
    bindGroupDesc.entries = entries;

    return device.createBindGroup(bindGroupDesc);
}

void diffract(
    WebGPUContext& context, 
    wgpu::Buffer& newUFBuffer, 
    wgpu::Buffer& newUBBuffer, 
    wgpu::Buffer& ufBuffer,
    wgpu::Buffer& ubBuffer,
    size_t bufferlen,
    std::vector<int> shape,
    std::optional<std::vector<float>> res, 
    std::optional<float> dz
) {
    buffer_len = bufferlen;
    res_buffer_len = res.value().size();
    Params params = {dz.value()};

    // INITIALIZING WEBGPU
    wgpu::Device device = context.device;
    wgpu::Queue queue = context.queue;
    
    // LOADING AND COMPILING SHADER CODE
    WorkgroupLimits limits = getWorkgroupLimits(device);
    std::string shaderCode = readShaderFile("src/ssnp/ssnp_diffract/ssnp_diffract.wgsl", limits.maxWorkgroupSizeX);
    wgpu::ShaderModule shaderModule = createShaderModule(device, shaderCode);

    // CREATING BUFFERS
    wgpu::Buffer cgammaBuffer = createBuffer(context.device, nullptr, sizeof(float) * buffer_len, WGPUBufferUsage(wgpu::BufferUsage::Storage));
    c_gamma(context, cgammaBuffer, res.value(), shape);
    wgpu::Buffer resBuffer = createBuffer(device, res.value().data(), sizeof(float) * res_buffer_len, wgpu::BufferUsage::Storage);
    wgpu::Buffer uniformBuffer = createBuffer(device, &params, sizeof(Params), wgpu::BufferUsage::Uniform);

    // CREATING BIND GROUP AND LAYOUT
    wgpu::BindGroupLayout bindGroupLayout = createBindGroupLayout(device);
    wgpu::BindGroup bindGroup = createBindGroup(device, bindGroupLayout, ufBuffer, ubBuffer, resBuffer, cgammaBuffer, newUFBuffer, newUBBuffer, uniformBuffer);

    // CREATING COMPUTE PIPELINE
    wgpu::ComputePipeline computePipeline = createComputePipeline(device, shaderModule, bindGroupLayout);

    // ENCODING AND DISPATCHING COMPUTE COMMANDS
    uint32_t workgroupsX = std::ceil(double(buffer_len)/limits.maxWorkgroupSizeX);
    wgpu::CommandBuffer commandBuffer = createComputeCommandBuffer(device, computePipeline, bindGroup, workgroupsX);
    queue.submit(1, &commandBuffer);

    // RELEASE RESOURCES
    commandBuffer.release();
    computePipeline.release();
    bindGroup.release();
    bindGroupLayout.release();
    shaderModule.release();
    cgammaBuffer.release();
    resBuffer.release();
    uniformBuffer.release();
}