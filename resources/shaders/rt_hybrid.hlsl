#define LIGHTS_REGISTER register(t2)
#include "util.hlsl"
#include "pbr_util.hlsl"
#include "material_util.hlsl"
#include "lighting.hlsl"

struct Vertex
{
	float3 pos;
	float2 uv;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float3 color;
};

struct Material
{
	float albedo_id;
	float normal_id;
	float roughness_id;
	float metalicness_id;

	MaterialData data;
};

struct Offset
{
    float material_idx;
    float idx_offset;
    float vertex_offset;
};

RWTexture2D<float4> output_refl_shadow : register(u0); // xyz: reflection, a: shadow factor
ByteAddressBuffer g_indices : register(t1);
StructuredBuffer<Vertex> g_vertices : register(t3);
StructuredBuffer<Material> g_materials : register(t4);
StructuredBuffer<Offset> g_offsets : register(t5);

Texture2D g_textures[90] : register(t8);
Texture2D gbuffer_albedo : register(t98);
Texture2D gbuffer_normal : register(t99);
Texture2D gbuffer_depth : register(t100);
Texture2D skybox : register(t6);
TextureCube irradiance_map : register(t7);
SamplerState s0 : register(s0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

struct ReflectionHitInfo
{
	float3 origin;
	float3 color;
	unsigned int seed;
};

cbuffer CameraProperties : register(b0)
{
	float4x4 inv_view;
	float4x4 inv_projection;
	float4x4 inv_vp;

	float2 padding;
	float frame_idx;
	float intensity;
};

struct Ray
{
	float3 origin;
	float3 direction;
};

uint3 Load3x32BitIndices(uint offsetBytes)
{
	// Load first 2 indices
	return g_indices.Load3(offsetBytes);
}

// Retrieve hit world position.
float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

float3 TraceReflectionRay(float3 origin, float3 norm, float3 direction, uint rand_seed)
{
	origin += norm * EPSILON;

	ReflectionHitInfo payload = {origin, float3(0, 0, 1), rand_seed};

	// Define a ray, consisting of origin, direction, and the min-max distance values
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0;
	ray.TMax = 10000.0;

	// Trace the ray
	TraceRay(
		Scene,
		RAY_FLAG_NONE,
		//RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
		// RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		~0, // InstanceInclusionMask
		0, // RayContributionToHitGroupIndex
		1, // MultiplierForGeometryContributionToHitGroupIndex
		0, // miss shader index
		ray,
		payload);

	return payload.color;
}

float3 unpack_position(float2 uv, float depth)
{
	// Get world space position
	const float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
	float4 wpos = mul(inv_vp, ndc);
	return (wpos.xyz / wpos.w).xyz;
}

float3 DoReflection(float3 wpos, float3 V, float3 normal, uint rand_seed)
{
	// Calculate ray info
	float3 reflected = reflect(-V, normal);

	// Shoot reflection ray
	float3 reflection = TraceReflectionRay(wpos, normal, reflected, rand_seed);
	return reflection;
}

float3 DoShadowAllLights(float3 wpos, uint depth, inout float rand_seed)
{
	uint light_count = lights[0].tid >> 2;	//Light count is stored in 30 upper-bits of first light

	float3 res = float3(0, 0, 0);

	[unroll]
	for (uint i = 0; i < light_count; i++)
	{
		// Get light and light type
		Light light = lights[i];
		uint tid = light.tid & 3;

		//Light direction (constant with directional, position dependent with other)
		float3 L = (lerp(light.pos - wpos, -light.dir, tid == light_type_directional));
		float light_dist = length(L);
		L /= light_dist;

		// Get maxium ray length (depending on type)
		float t_max = lerp(light_dist, 100000, tid == light_type_directional);

		// Add shadow factor to final result
		res += GetShadowFactor(wpos, L, t_max, depth + 1, rand_seed);
	}

	// return final res
	return res / float(light_count);
}

#define M_PI 3.14159265358979

[shader("raygeneration")]
void RaygenEntry()
{
	uint rand_seed = initRand(DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x, frame_idx);

	// Texture UV coordinates [0, 1]
	float2 uv = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy - 1);

	// Screen coordinates [0, resolution] (inverted y)
	int2 screen_co = DispatchRaysIndex().xy;

	// Get g-buffer information
	float4 albedo_roughness = gbuffer_albedo[screen_co];
	float4 normal_metallic = gbuffer_normal[screen_co];

	// Unpack G-Buffer
	float depth = gbuffer_depth[screen_co].x;
	float3 wpos = unpack_position(float2(uv.x, 1.f - uv.y), depth);
	float3 albedo = albedo_roughness.rgb;
	float roughness = albedo_roughness.w;
	float3 normal = normal_metallic.xyz;
	float metallic = normal_metallic.w;

	// Do lighting
	float3 cpos = float3(inv_view[0][3], inv_view[1][3], inv_view[2][3]);
	float3 V = normalize(cpos - wpos);

	if (length(normal) == 0)		//TODO: Could be optimized by only marking pixels that need lighting, but that would require execute rays indirect
	{
		// A value of 1 in the output buffer, means that there is shadow
		// So, the far plane pixels are set to 0
		output_refl_shadow[DispatchRaysIndex().xy] = float4(skybox.SampleLevel(s0, SampleSphericalMap(-V), 0));
		output_refl_shadow[DispatchRaysIndex().xy].a = 0;
		return;
	}

	wpos += normal * EPSILON;
	// Get shadow factor
	float shadow_result = DoShadowAllLights(wpos, 0, rand_seed).x;
	shadow_result = abs(shadow_result - 1.0);

	// Get reflection result
	float3 reflection_result = DoReflection(wpos, V, normal, rand_seed);

	// xyz: reflection, a: shadow factor
	output_refl_shadow[DispatchRaysIndex().xy] = float4(reflection_result.xyz, shadow_result);
}

//Reflections

float3 HitAttribute(float3 a, float3 b, float3 c, BuiltInTriangleIntersectionAttributes attr)
{
	float3 vertexAttribute[3];
	vertexAttribute[0] = a;
	vertexAttribute[1] = b;
	vertexAttribute[2] = c;

	return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

[shader("closesthit")]
void ReflectionHit(inout ReflectionHitInfo payload, in MyAttributes attr)
{
	// Calculate the essentials
	const Offset offset = g_offsets[InstanceID()];
	const Material material = g_materials[offset.material_idx];
	const float index_offset = offset.idx_offset;
	const float vertex_offset = offset.vertex_offset;
	const float3x4 model_matrix = ObjectToWorld3x4();

	// Find first index location
	const uint index_size = 4;
	const uint indices_per_triangle = 3;
	const uint triangle_idx_stride = indices_per_triangle * index_size;

	uint base_idx = PrimitiveIndex() * triangle_idx_stride;
	base_idx += index_offset * 4; // offset the start

	uint3 indices = Load3x32BitIndices(base_idx);
	indices += float3(vertex_offset, vertex_offset, vertex_offset); // offset the start

	// Gather triangle vertices
	const Vertex v0 = g_vertices[indices.x];
	const Vertex v1 = g_vertices[indices.y];
	const Vertex v2 = g_vertices[indices.z];

	//Get data from VBO
	float2 uv = HitAttribute(float3(v0.uv, 0), float3(v1.uv, 0), float3(v2.uv, 0), attr).xy;
	uv.y = 1.0f - uv.y;

	OutputMaterialData output_data = InterpretMaterialDataRT(material.data,
		g_textures[material.albedo_id],
		g_textures[material.normal_id],
		g_textures[material.roughness_id],
		g_textures[material.metalicness_id],
		0,
		s0,
		uv);

	float3 albedo = output_data.albedo;
	float roughness = output_data.roughness;
	float metal = output_data.metallic;


	//Direction & position
	const float3 hit_pos = HitWorldPosition();
	float3 V = normalize(payload.origin - hit_pos);

	//Normal mapping
	float3 normal = normalize(HitAttribute(v0.normal, v1.normal, v2.normal, attr));
	float3 tangent = HitAttribute(v0.tangent, v1.tangent, v2.tangent, attr);
	float3 bitangent = HitAttribute(v0.bitangent, v1.bitangent, v2.bitangent, attr);

	float3 N = normalize(mul(model_matrix, float4(normal, 0)));
	float3 T = normalize(mul(model_matrix, float4(tangent, 0)));
	float3 B = normalize(mul(model_matrix, float4(bitangent, 0)));
	float3x3 TBN = float3x3(T, B, N);

	float3 normal_t = output_data.normal;

	float3 fN = normalize(mul(normal_t, TBN));
	if (dot(fN, V) <= 0.0f) fN = -fN;

	//TODO: Reflections

	//Shading
	float3 flipped_N = fN;
	flipped_N.y *= -1;
	const float3 sampled_irradiance = irradiance_map.SampleLevel(s0, flipped_N, 0).xyz;

	const float3 F = F_SchlickRoughness(max(dot(fN, V), 0.0), metal, albedo, roughness);
	float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metal;

	float3 specular = (float3(0, 0, 0)) * F;
	float3 diffuse = albedo * sampled_irradiance;
	float3 ambient = (kD * diffuse + specular);

	// Output the final reflections here
	payload.color = float3(ambient);
}

//Reflection skybox
[shader("miss")]
void ReflectionMiss(inout ReflectionHitInfo payload)
{
	payload.color = skybox.SampleLevel(s0, SampleSphericalMap(WorldRayDirection()), 0);
}
