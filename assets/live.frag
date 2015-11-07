uniform vec3  iResolution;  // viewport resolution (in pixels)
uniform vec3  iColor;
uniform float iGlobalTime;
uniform float iZoom;
uniform float iAlpha;
uniform vec4  iMouse;

in Vertex
{
	vec2 	uv;
} vertex;

out vec4 oColor;

void main( void )
{
	vec4 color 	= vec4( 1.0, sin(iGlobalTime), 0.4, 1.0 );
	oColor 		= color;
}