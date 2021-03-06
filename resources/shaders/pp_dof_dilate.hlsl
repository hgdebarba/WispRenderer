/*!
 * Copyright 2019 Breda University of Applied Sciences and Team Wisp (Viktor Zoutman, Emilio Laiso, Jens Hagen, Meine Zeinstra, Tahar Meijs, Koen Buitenhuis, Niels Brunekreef, Darius Bouma, Florian Schut)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __PP_DOF_DILATE_HLSL__
#define __PP_DOF_DILATE_HLSL__

#include "pp_dof_properties.hlsl"
#include "pp_dof_util.hlsl"

Texture2D source_near : register(t0);
RWTexture2D<float4> output_near : register(u0);
SamplerState s0 : register(s0);

[numthreads(16, 16, 1)]
void main_cs(int3 dispatch_thread_id : SV_DispatchThreadID)
{
	float2 screen_size = float2(0.f, 0.f);
	output_near.GetDimensions(screen_size.x, screen_size.y);
	screen_size -= 1.0f;

	float2 screen_coord = int2(dispatch_thread_id.x, dispatch_thread_id.y);


	float2 uv = (screen_coord) / screen_size;

	static const int sample_radius = 3;

	float output = source_near.SampleLevel(s0, uv , 0).r;

	[unroll]
	for (int y = -sample_radius; y <= sample_radius; ++y)
	{
		[unroll]
		for (int x = -sample_radius; x <= sample_radius; ++x)
		{
			output = max(output, source_near.SampleLevel(s0, saturate((screen_coord + float2(x, y)) / screen_size), 0).r);
		}
	}

	output_near[int2(dispatch_thread_id.xy)] = output;
}

#endif //__PP_DOF_DILATE_HLSL__