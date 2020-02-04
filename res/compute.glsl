#version 450 core

layout(local_size_x = 16, local_size_y = 16) in;
layout(rgba32f, binding = 0) uniform image2D output_image;

uniform float elapsed_time;
uniform float delta_time;
uniform ivec2 mouse_coord;
uniform int frame_count;

#define pi 3.141

mat3 look_at(vec3 d)
{
	vec3 u = vec3(0, 1, 0);
	vec3 r = normalize(cross(d, u));
	u = normalize(cross(r, d));
	return mat3(r, u, d);
}

mat2 rotateXY(float a)
{
	return mat2(
		cos(a), -sin(a),
		sin(a), cos(a)
	);
}

float sphere(vec3 p, float r)
{
	return length(p) - r;
}

float roundcube(vec3 p, vec2 r)
{
	return length(max(abs(p) - (r.x - r.y), vec3(0))) - r.y;
}

float roundcube(vec3 p, vec4 r)
{
	return length(max(abs(p) - (r.xyz - r.w), vec3(0))) - r.w;
}

float cube(vec3 p, float r)
{
	return roundcube(p, vec2(r, .0));
}

float cube(vec3 p, vec3 r)
{
	return roundcube(p, vec4(r, 0));
}

float quickcube(vec3 p, float r)
{
	p = abs(p);
	return max(p.x, max(p.y, p.z)) - r;
}

float quickcube(vec3 p, vec3 r)
{
	p = abs(p);
	return max(p.x - r.x, max(p.y - r.y, p.z - r.z));
}

float plane(vec3 p, vec3 n, float r)
{
	return dot(p, n) - r;
}

float line(vec3 p, vec3 a, vec3 b, float r)
{
	vec3 ab = b - a;
	vec3 ap = p - a;
	float h = clamp(dot(ap, ab) / dot(ab, ab), 0., 1.);
	return length(ap - h * ab) - r;
}

float torus(vec3 p, vec2 r)
{
	vec3 p0 = normalize(vec3(p.xy, 0)) * r.x;
	return length(p0 - p) - r.y;
}

float onion(float d, float thickness)
{
	return abs(d + thickness) - thickness;
}

vec3 elongate(vec3 p, vec3 a, vec3 b)
{
	return max(min(p, a), p - b);
}

float mandelbrot(vec2 c)
{
	vec2 z = c;
	for (int i = 0; i < 500; ++i) {
		z = vec2(z.x * z.x - z.y * z.y, z.x * z.y * 2.) + c;
		if (length(z) > 2.)
			/* return length(z) - 2.; */
			return 0;
	}
	return length(z);
}

#define SCENE \
	float s0 = sphere(p, .2); \
	float c0 = roundcube(p - vec3(3, 0, 0), vec2(1., .3)); \
	float c1 = cube(p - vec3(10, 0, 0), 1.); \
	float t0 = torus(p - vec3(0, 0, -10), vec2(5, 1)); \
	float l = min(s0, min(min(c0, c1), t0));

float scene(vec3 p)
{
	SCENE
	return l;
}

float scene_id(vec3 p, out int id)
{
	SCENE
	
	id = 0;

	if (l == s0) id = 1;

	return l;
}

vec3 normal(vec3 p)
{
	float l = scene(p);
	vec2 e = vec2(0, .001);

	return normalize(
		l - vec3(
			scene(p - e.yxx),
			scene(p - e.xyx),
			scene(p - e.xxy)
		)
	);
}

bool march(vec3 ro, vec3 rv, out vec3 p, out vec3 d, out float steps)
{
	p = ro;
	float ol = 0.;

	for (int i = 0; i < 100; ++i) {
		float l = scene(p);
		/* l = min(10., l); */
		ol += l;
		/* float g = 1. / (1. + pow(length(l), 2.)); */
		/* rv -= normal(p) * g * .05; */
		float g = 1. / (pow(length(p), 2.));
		rv -= normalize(p) * g * l * .1;
		d = normalize(rv);
		p += d * l;
		steps = float(i) / 100.;

		if (l < .01)
			return true;
		if (ol > 100.)
			return false;
	}

	return false;
}

float skybox(vec3 rd)
{
	float r = 20.;
	vec3 uv = abs(sin(rd * r * 3.));
	/* return roundcube(uv, vec2(.8, .3)) * .5; */
	/* return dot(uv, vec3(1. / 3.)); */
	return abs(rd.y);
}

void main() {
	vec2 output_size = vec2(imageSize(output_image));
  vec2 output_coord = gl_GlobalInvocationID.xy;
	if (output_coord.x >= output_size.x || output_coord.y >= output_size.y) return;

	vec2 uv = (output_coord - output_size * .5) / output_size.y;
	vec3 c = vec3(0);
  vec2 m = vec2(mouse_coord - output_size * .5) / output_size.y;

	m *= -pi;
	vec3 ro = vec3(sin(m.x) * cos(m.y), sin(m.y), cos(m.x) * cos(m.y)) * 5.;
	vec3 dd = normalize(vec3(uv, 1));
	vec3 rd = look_at(normalize(-ro)) * normalize(vec3(uv, 1));
	/* vec3 ro = camera_pos + vec3(0, .4, 0); */
	/* vec3 rd = look_at(camera_dir) * normalize(vec3(uv, 1)); */

	vec3 p, d;
	float steps;
	bool hit = march(ro, rd, p, d, steps);
	vec3 n = normal(p);
	int id;
	float l = scene_id(p, id);
	c.b += hit ? 1. - steps : 0.;
	c.g += hit ? max(.1, dot(-n, d)) * .3 : 0.;
	c.r += hit ? max(0., sign(dot(rd, n))) : 0.;
	c.g += hit ? 0. : skybox(d);
	c *= hit ? (id == 1 ? .0 : 1.) : 1.;

	float gamma = 2.2;
	c = pow(c, vec3(1./gamma));
	imageStore(output_image, ivec2(output_coord), vec4(c, 1));
}
