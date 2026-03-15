/*
 * Copyright (C) 2019 Open Source Robotics Foundation
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
 *
 */

#version ogre_glsl_ver_330

// The input texture, which is set up by the Ogre Compositor infrastructure.
vulkan_layout( ogre_t0 ) uniform texture2D RT;
vulkan_layout( ogre_t1 ) uniform texture2D depthTexture;
vulkan_layout( ogre_t2 ) uniform texture2D Blur1;

vulkan( layout( ogre_s0 ) uniform sampler samplerState );

// Other parameters are set in C++, via
// Ogre::GpuProgramParameters::setNamedConstant()

// vulkan( layout( ogre_P0 ) uniform Params { )
// 	uniform float OriginalImageWeight;
// 	uniform float BlurWeight;
// vulkan( }; )

// input params from vertex shader
vulkan_layout( location = 0 )
in block
{
  vec2 uv0;
} inPs;

// final output color
vulkan_layout( location = 0 )
out vec4 fragColor;

void main()
{
  float c1 = 0.0037;
  // float c1 = 0.1;
  float c2 = 0.0016;
  
  // float depth = 1.0;
  float depth = texture(vkSampler2D(depthTexture, samplerState), inPs.uv0).x;
  
  vec4 sharp	= texture( vkSampler2D( RT, samplerState ), inPs.uv0 );
	vec4 blur	= texture( vkSampler2D( Blur1, samplerState ), inPs.uv0 );

  float cd = min(clamp(1 / exp(pow(depth * c1, 2)), 0.0, 1.0), 0.5);
	float cb = clamp(1 / exp(pow(depth * c2, 2)), 0.0, 1.0);

  // float cd = depth;
	
  vec4 sceneColor = cb * sharp + (1-cb) * blur;
	// fragColor = clamp(cd * sceneColor + (1-cd) * vec4(0, 0.2, 0.4, 0.0), 0.0, 1.0);
  fragColor = clamp(vec4(0, 0, 1/depth, 0.0), 0.0, 1.0);
}
