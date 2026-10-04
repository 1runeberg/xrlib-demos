// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#version 450
#extension GL_EXT_multiview : require
layout( push_constant ) uniform PushConsts { mat4 eyeVPs[ 2 ]; }
pushConsts;
layout( location = 0 ) in vec3 inPosition;
layout( location = 3 ) in vec2 inUV;
layout( location = 5 ) in vec3 inColor;
layout( location = 8 ) in vec4 model0;
layout( location = 9 ) in vec4 model1;
layout( location = 10 ) in vec4 model2;
layout( location = 11 ) in vec4 model3;
layout( location = 0 ) out vec4 color;
void main()
{
	gl_Position = pushConsts.eyeVPs[ gl_ViewIndex ] * mat4( model0, model1, model2, model3 ) * vec4( inPosition, 1 );
	color = vec4( inColor, inUV.x );
}
