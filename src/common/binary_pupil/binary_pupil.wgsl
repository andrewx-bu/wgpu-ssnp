@group(0) @binding(0) var<storage, read> cgamma : array<f32>;
@group(0) @binding(1) var<storage, read_write> mask : array<u32>;
@group(0) @binding(2) var<uniform> params : f32;

@compute @workgroup_size({{WORKGROUP_SIZE}})
fn main(@builtin(global_invocation_id) global_id : vec3<u32>) {
    let idx = global_id.x;
    if (idx >= arrayLength(&cgamma)) {
        return;
    }
    
    let threshold = sqrt(1.0 - params * params);
    mask[idx] = select(0u, 1u, cgamma[idx] > threshold);
}