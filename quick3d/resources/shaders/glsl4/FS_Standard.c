#version 430 compatibility

// Standard fragment shader

#ifdef GL_ES
precision highp int;
precision highp float;
#endif

// Constant incoming data

uniform vec2            u_resolution;
uniform float           u_time;
uniform float           u_deltaTime;
uniform float           u_shaderQuality;
uniform int             u_rendering_shadows;
uniform int             u_normals_only;

uniform mat4            u_camera_projection_matrix;
uniform mat4            u_camera_matrix;
uniform mat4            u_shadow_projection_matrix;
uniform mat4            u_shadow_matrix;
uniform mat4            u_model_matrix;
uniform vec3            u_camera_true_position;
uniform vec3            u_camera_position;
uniform vec3            u_camera_direction;
uniform vec3            u_camera_up;
uniform vec3            u_world_origin;
uniform vec3            u_world_up;
uniform float           u_camera_altitude;
uniform float           u_atmosphere_altitude;

uniform vec3            u_global_ambient;

uniform int             u_num_lights;
uniform int             u_light_is_sun[8];
uniform vec3            u_light_position[8];
uniform vec3            u_light_screen_position[8];
uniform vec3            u_light_direction[8];
uniform vec3            u_light_color[8];
uniform float           u_light_distance_to_camera[8];
uniform float           u_light_distance[8];
uniform float           u_light_spot_angle[8];
uniform float           u_light_occlusion[8];

uniform int             u_texture_diffuse_enable;
uniform sampler2D       u_texture_diffuse_0;
uniform sampler2D       u_texture_diffuse_1;
uniform sampler2D       u_texture_diffuse_2;
uniform sampler2D       u_texture_diffuse_3;
uniform sampler2D       u_texture_diffuse_4;
uniform sampler2D       u_texture_diffuse_5;
uniform sampler2D       u_texture_diffuse_6;
uniform sampler2D       u_texture_diffuse_7;
uniform sampler2D       u_shadow_texture;

uniform int             u_texture_bump_enable;
uniform sampler2D       u_texture_bump;

uniform vec4            u_material_ambient;
uniform vec4            u_material_diffuse;
uniform vec4            u_material_specular;
uniform vec4            u_material_subdermal;
uniform float           u_material_self_illum;
uniform float           u_material_shininess;
uniform float           u_material_metalness;
uniform float           u_material_sss_factor;
uniform float           u_material_sss_radius;

uniform int             u_depth_computing;
uniform int             u_shadow_enable;

uniform int             u_sky_enable;

uniform int             u_wave_enable;
uniform float           u_wave_amplitude;

uniform int             u_fog_enable;
uniform float           u_fog_distance;
uniform float           u_fog_level;
uniform vec3            u_fog_color;
uniform vec3            u_sky_zenith_color;
uniform vec3            u_sun_color;
uniform float           u_sun_intensity;

uniform int             u_IR_enable;
uniform float           u_IR_factor;

uniform int             u_inverse_polarity_enable;

uniform int             u_lightrays_enable;

uniform float           u_contrast;
uniform float           u_saturation;
uniform float           u_brightness;

// Interpolated values

in vec3			vo_position;
in vec3			vo_normal;
in vec3			vo_tangent;
in vec3			vo_binormal;
in vec3			vo_texcoord;
in vec4			vo_shadow_coord;
in float			vo_distance;
in float			vo_altitude;
in float			vo_difftex_weight_0;
in float			vo_difftex_weight_1;
in float			vo_difftex_weight_2;
in float			vo_difftex_weight_3;
in float			vo_difftex_weight_4;
in float			vo_difftex_weight_5;
in float			vo_difftex_weight_6;
in float			vo_difftex_weight_7;

// Globals

float gAtmosphereFactor;
float gSeaAltitudeFactor_1;
float gSeaAltitudeFactor_2;
float gStepSize;
float gStepFactor;

float pixelDistanceX = 0.01;
float pixelDistanceY = 0.01;

// Constants

#define ROOTTHREE       1.73205081
#define uTMK            20.0
#define MAX_STEPS       64
#define TM_MIN          0.05
#define pi              3.1415926535
#define PI              pi
#define SUN_DISC_SHARP_EXPONENT   1000.0
#define SUN_DISC_BLURRY_EXPONENT  32.0
#define SUN_DISC_BLURRY_DIM_FACTOR 0.2
#define SUN_GLOW_SHARP_FACTOR     0.08
#define SUN_GLOW_BLURRY_FACTOR    0.12
#define FOG_DENSE_FALLOFF_EXPONENT 0.5

//-------------------------------------------------------------------------------------------------
// Math functions

float sqr(float x)
{
    return x * x;
}

float distance(vec3 pos)
{
    return abs(sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z));
}

// Brings all values below a threshold smoothly to that threshold
float almostIdentity( float x, float m, float n )
{
    if (x > m) return x;

    float a = 2.0 * n - m;
    float b = 2.0 * m - 3.0 * n;
    float t = x / m;

    return (a * t + b) * t * t + n;
}

// Impulse
float impulse(float k, float x)
{
    float h = k * x;
    return h * exp(1.0 - h);
}

// Cubic pulse
float cubicPulse(float c, float w, float x)
{
    x = abs(x - c);
    if (x > w) return 0.0;
    x /= w;
    return 1.0 - x * x * (3.0 - 2.0 * x);
}

// Exponential step
float expStep(float x, float k, float n)
{
    return exp(-k * pow(x, n));
}

// Parabola
float parabola(float x, float k)
{
    return pow(4.0 * x * (1.0 - x), k);
}

vec4 quaternionFromAxisAndAngle(float x, float y, float z, float angle)
{
    vec4 result = vec4(0.0, 0.0, 0.0, 0.0);
    //x, y, and z form a normalized vector which is now the axis of rotation.
    result.w  = cos(angle / 2.0);
    result.x = x * sin(angle / 2.0);
    result.y = y * sin(angle / 2.0);
    result.z = z * sin(angle / 2.0);
    return result;
}

vec4 multQuat(vec4 q1, vec4 q2)
{
    return vec4(
                q1.w * q2.x + q1.x * q2.w + q1.z * q2.y - q1.y * q2.z,
                q1.w * q2.y + q1.y * q2.w + q1.x * q2.z - q1.z * q2.x,
                q1.w * q2.z + q1.z * q2.w + q1.y * q2.x - q1.x * q2.y,
                q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
                );
}

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

vec3 rotateVectorFast(vec4 quat, vec3 position)
{
    return position + 2.0 * cross(cross(position, quat.xyz) + quat.w * position, quat.xyz);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 mapDirectionVectorToEqui(vec3 position)
{
    vec2 result = vec2(0.0, 0.0);

//    result.x = (atan2(position.z, position.x) / _2PI) + 0.25;
//    result.y = acos(position.y) / PI;

//    result.x = mod(1.0 - result.x, 1.0);
//    result.y = mod(1.0 - result.y, 1.0);

    return result;
}

float flatDisc(vec3 discPosition, float discRadius, vec3 pixelPosition)
{
    float dist = distance(discPosition - pixelPosition);
    return dist > discRadius ? 0.0 : 1.0;
}

float disc(vec3 discPosition, float discRadius, vec3 pixelPosition)
{
    float dist = distance(vec3(discPosition.xy, 0.0) - vec3(pixelPosition.xy, 0.0));
    return dist > discRadius ? 0.0 : (discRadius - dist) / discRadius;
}

//-------------------------------------------------------------------------------------------------
// Color functions

float colorLuminosity(vec4 color)
{
    return dot(color.rgb, vec3(0.299, 0.587, 0.114));
}

vec4 contrastSaturationBrightness(vec4 color, float contrast, float saturation, float brightness)
{
    vec3 newColor = mix(vec3(0.5), mix(vec3(dot(vec3(0.2125, 0.7154, 0.0721), color.rgb * brightness)), color.rgb * brightness, saturation), contrast);

    return vec4(newColor.rgb, color.a);
}

//-------------------------------------------------------------------------------------------------
// BRDF functions

float SchlickFresnel(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m * m;
    return m2 * m2 * m; // pow(m,5)
}

float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1/PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}

float GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t * t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

//-------------------------------------------------------------------------------------------------
// Convert RGB color to HSV color

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

//-------------------------------------------------------------------------------------------------
// Convert HSV color to RGB color

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

//-------------------------------------------------------------------------------------------------
// Random

float random(vec2 p)
{
    // We need irrationals for pseudo randomness.
    // Most (all?) known transcendental numbers will (generally) work.
    const vec2 r = vec2(
                23.1406926327792690,	// e^pi (Gelfond's constant)
                2.6651441426902251		// 2^sqrt(2) (Gelfond�Schneider constant)
                );

    return fract(cos(mod(123456789.0, 1e-7 + 256.0 * dot(p,r))));
}

//-------------------------------------------------------------------------------------------------
// Perlin noise

float perlin_hash(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 321.4))) * 43758.5453123);
}

float perlin(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f *= f * (3.0 - 2.0 * f);

    float n = mix(
                mix(mix(perlin_hash(i + vec3(0.0, 0.0, 0.0)), perlin_hash(i + vec3(1.0, 0.0, 0.0)),f.x),
                    mix(perlin_hash(i + vec3(0.0, 1.0, 0.0)), perlin_hash(i + vec3(1.0, 1.0, 0.0)),f.x),
                    f.y),
                mix(mix(perlin_hash(i + vec3(0.0, 0.0, 1.0)), perlin_hash(i + vec3(1.0, 0.0, 1.0)),f.x),
                    mix(perlin_hash(i + vec3(0.0, 1.0, 1.0)), perlin_hash(i + vec3(1.0, 1.0, 1.0)),f.x),
                    f.y),
                f.z);

    return clamp((n - 0.49) * 2.05, -1.0, 1.0);
}

float perlin_0_1(vec3 p)
{
    return (perlin(p) + 1.0) / 2.0;
}

//-------------------------------------------------------------------------------------------------
// Turbulence

#define TURBULENCE_OCTAVES 4

float turbulence(vec3 ipos)
{
    vec3 pos = ipos;

    float f = 0.0;

    for (int i = 0; i < TURBULENCE_OCTAVES; i++)
    {
        pos = (pos.yzx + pos.zyx * vec3(1.0, -1.0, 1.0)) / sqrt(2.0);
        f = f * 2.0 + perlin(pos);
        pos *= 1.5;
    }

    f /= pow(2.0, float(TURBULENCE_OCTAVES));
    return f;
}

float turbulence_0_1(vec3 ipos)
{
    vec3 pos = ipos;

    float f = 0.0;

    for (int i = 0; i < TURBULENCE_OCTAVES; i++)
    {
        pos = (pos.yzx + pos.zyx * vec3(1.0, -1.0, 1.0)) / sqrt(2.0);
        f = f * 2.0 + perlin_0_1(pos);
        pos *= 1.5;
    }

    f /= pow(2.0, float(TURBULENCE_OCTAVES));
    return f;
}

float wavelet_pattern(vec3 ipos)
{
    vec3 pos = ipos * 4.0;

    float x = mod(ipos.x, pi * 0.5);
    float y = mod(ipos.y, pi * 0.5);
    float z = mod(ipos.z, pi * 0.5);

    float f = sin(ipos.x) + sin(ipos.y) + sin(ipos.z);

    return f;
}

float movingTurbulence(vec3 ipos, float timeScale)
{
    float time = u_time * timeScale;

    return pow(turbulence_0_1(ipos + vec3(1.0, 1.0, 1.0) * time), 2.0);
}

float staticTurbulence(vec3 ipos)
{
    return pow(turbulence_0_1(ipos), 2.0);
}

//-------------------------------------------------------------------------------------------------
// BRDFs

float orenNayarDiffuse(vec3 lightDirection, vec3 viewDirection, vec3 surfaceNormal, float roughness, float albedo)
{
    float LdotV = dot(lightDirection, viewDirection);
    float NdotL = dot(lightDirection, surfaceNormal);
    float NdotV = dot(surfaceNormal, viewDirection);

    float s = LdotV - NdotL * NdotV;
    float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));

    float sigma2 = roughness * roughness;
    float A = 1.0 + sigma2 * (albedo / (sigma2 + 0.13) + 0.5 / (sigma2 + 0.33));
    float B = 0.45 * sigma2 / (sigma2 + 0.09);

    return albedo * max(0.0, NdotL) * (A + B * s / t) / PI;
}

float ggxGlossy(vec3 lightDirection, vec3 viewDirection, vec3 surfaceNormal, float roughness)
{
    viewDirection = -viewDirection;

    vec3 H       = normalize(viewDirection + lightDirection);
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(surfaceNormal, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

//-----------------------------------------------
// Compute bump normal

float sampleBumMap(vec2 pixelPosition)
{
    return colorLuminosity(texture2D(u_texture_bump, pixelPosition));
}

vec3 bumpAsNormalMap(vec3 surfaceNormal, vec3 pixelPosition)
{
    float me = pixelPosition.z;
    float n = sampleBumMap(pixelPosition.xy + vec2(0.0, -pixelDistanceY)) * 20.0;
    float s = sampleBumMap(pixelPosition.xy + vec2(0.0,  pixelDistanceY)) * 20.0;
    float e = sampleBumMap(pixelPosition.xy + vec2( pixelDistanceX, 0.0)) * 20.0;
    float w = sampleBumMap(pixelPosition.xy + vec2(-pixelDistanceX, 0.0)) * 20.0;

    if (n > 0.0 && s > 0.0 && e > 0.0 && w > 0.0)
    {
        // find perpendicular vector to normal
        vec3 temp = surfaceNormal;
        if (surfaceNormal.x == 1) temp.y += 0.5; else temp.x += 0.5;

        // form a basis with normal being one of the axes
        vec3 perp1 = normalize(cross(surfaceNormal, temp));
        vec3 perp2 = normalize(cross(surfaceNormal, perp1));

        // use the basis to move the normal in its own space by the offset
        vec3 normalOffset = -1.0 * (((n - me) - (s - me)) * perp1 + ((e - me) - (w - me)) * perp2);
        surfaceNormal = normalize(surfaceNormal - normalOffset);

        // find perpendicular vector to normal
        // temp = surfaceNormal;
        // if (surfaceNormal.x == 1) temp.y += 0.5; else temp.x += 0.5;
        // surfaceNormalTangent = normalize(cross(surfaceNormal, temp));
        // surfaceNormalBitangent = normalize(cross(surfaceNormal, surfaceNormalTangent));
    }

    return surfaceNormal;
}

//-------------------------------------------------------------------------------------------------
// Clouds

vec3 cloudNormal = vec3(0.0, 1.0, 0.0);

#define CLOUD_OCTAVES 4

float getCloudTurbulenceAt(vec3 pos)
{
    float f = 0.0;

    for (int i = 0; i < CLOUD_OCTAVES; i++)
    {
        pos = (pos.yzx + pos.zyx * vec3(1.0, -1.0, 1.0)) / sqrt(2.0);
        f = f * 2.0 + abs(perlin(pos) - 0.5) * 2.0;
        pos *= 2.0;
    }

    f /= exp2(float(CLOUD_OCTAVES));
    return f;
}

float getCloudDensityAt(vec3 origin, vec3 position)
{
    float density_factor = 0.4;
    vec3 up = normalize(u_camera_true_position);
    float dAltitude = (up.x * position.x + up.y * position.y + up.z * position.z) + u_camera_altitude;

    float layer_min = 1000.0 - perlin_0_1(position * 0.01) * 300.0;
    float layer_max = 1000.0 + perlin_0_1(position * 0.01) * 300.0;

    if (dAltitude < layer_min) return 0.0;
    if (dAltitude > layer_max) return 0.0;

    float layer_height = layer_max - layer_min;

    float alt_factor_1 = abs((dAltitude - layer_min) / layer_height);
    float alt_factor_2 = abs((layer_max - dAltitude) / layer_height);
    float alt_factor = min(alt_factor_1, alt_factor_2);

    float a = pow(perlin(position * 0.01), 3.0);
    float b = pow(getCloudTurbulenceAt(position * 0.01), 4.0);
    float c = (a + b) * density_factor * alt_factor;

    vec3 normal_position = vec3(0.0, (perlin(position * 0.01) - 0.5) * 2.0, 0.0);
    cloudNormal = normalize(vec3(0.0, 1.0, 0.0) + (origin - normal_position));

    return c;
}

#define CLOUD_STEPS 20

vec4 getCloudsAt(vec3 origin, vec3 direction)
{
    float maxTraceDistance = 2000.0;
    float stepSize = maxTraceDistance / float(CLOUD_STEPS);
    float totalDensity = 0.0;
    float fogIntensity = clamp(((u_fog_color.r + u_fog_color.g + u_fog_color.b) / 3.0) * 1.5, 0.0, 1.0);

    vec3 stepDistance = direction * stepSize;
    vec3 colorLight = vec3(0.9, 0.9, 0.9) * fogIntensity;
    vec3 colorDark = vec3(0.5, 0.5, 0.6) * fogIntensity;
    vec3 position = origin;

    float accumulatedColor = 1.0;

    for (int i = 0; i < CLOUD_STEPS; i++)
    {
        float local_density = getCloudDensityAt(origin, position);

        totalDensity += local_density;
        accumulatedColor += (cloudNormal.y * local_density);

        position += stepDistance;
    }

    vec4 color;

    color.a = clamp(totalDensity, 0.0, 1.0);
    color.rgb = mix(colorDark, colorLight, clamp(accumulatedColor, 0.0, 1.5));

    return color * gAtmosphereFactor;
}

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Rays and flares

vec3 lightRays(vec3 origin, vec3 direction, vec2 xy)
{
    vec3 color = vec3(0.0, 0.0, 0.0);

    for (int index = 0; index < u_num_lights; index++)
    {
        if (u_light_occlusion[index] == 0.0)
        {
            float distanceFactor = clamp((u_light_distance[index] * 2.0) / u_light_distance_to_camera[index], 0.0, 1.0);

            // Large glow
            if (u_light_screen_position[index].z > 0.0 && u_light_is_sun[index] > 0)
            {
                vec3 lightDirection = normalize(u_light_position[index] - origin);
                float amount = max(dot(direction, lightDirection), 0.0);
                float sunBlur = clamp(u_fog_level, 0.0, 1.0);
                float sunDiscExponent = mix(SUN_DISC_SHARP_EXPONENT, SUN_DISC_BLURRY_EXPONENT, sunBlur);
                float sunDiscDim = mix(1.0, SUN_DISC_BLURRY_DIM_FACTOR, sunBlur);
                float sunGlowFactor = mix(SUN_GLOW_SHARP_FACTOR, SUN_GLOW_BLURRY_FACTOR, sunBlur);
                color += (u_light_color[index] * amount * amount * sunGlowFactor) * gAtmosphereFactor;
                color += (u_light_color[index] * min(pow(amount, sunDiscExponent), 1.0) * sunDiscDim);
                distanceFactor = 1.0;
            }

            // Ray
//            if (u_light_screen_position[index].z > 0.0)
//            {
//                float diffX = abs(xy.x - u_light_screen_position[index].x);
//                float mixX = mix(1.0, 0.90, diffX);
//                float rayAmountX = clamp(mixX, 0.0, 1.0) * distanceFactor;
//                float diffY = abs(xy.y - u_light_screen_position[index].y);
//                float mixY = mix(0.9, 0.00, diffY);
//                float rayAmountY = clamp(mixY, 0.0, 1.0) * distanceFactor;
//                float final = pow(rayAmountX * rayAmountY, 20.0);
//                color += u_light_color[index] * final;
//            }
        }
    }

    return color;
}

vec3 flareDisc(vec3 light, float where)
{
    vec3 center = vec3(0.5, 0.5, 0.0);
    vec3 ray = light - center;
    return center - ray * where;
}

float flareType1(float value)
{
    return clamp(impulse(8.0, value), 0.0, 1.0);
}

float flareType2(float value)
{
    return clamp(cubicPulse(0.5, 0.5, value), 0.0, 1.0);
}

vec4 lightFlares(vec3 lightPosition, vec3 pixelPosition, vec4 lightColor, float lightFlareIntensity)
{
    float final = 0.0;

    if (lightPosition.x >= 0.0 && lightPosition.x <= 1.0 && lightPosition.y >= 0.0 && lightPosition.y <= 1.0)
    {
        final += clamp(flareType1(disc(flareDisc(vec3(lightPosition.xy, 0.0), 0.2), 0.06, pixelPosition)), 0.0, 1.0);
        final += clamp(flareType1(disc(flareDisc(vec3(lightPosition.xy, 0.0), 0.3), 0.03, pixelPosition)), 0.0, 1.0);
        final += clamp(flareType2(disc(flareDisc(vec3(lightPosition.xy, 0.0), 0.4), 0.03, pixelPosition)), 0.0, 1.0);
        final += clamp(flareType2(disc(flareDisc(vec3(lightPosition.xy, 0.0), 0.7), 0.10, pixelPosition)), 0.0, 1.0);
    }

    final *= lightFlareIntensity * 0.5;

    return vec4(lightColor.rgb, final);
}

//-------------------------------------------------------------------------------------------------
// Sky

#include "../RayleighSky.inc.c"

//-------------------------------------------------------------------------------------------------
// Water

float oceanDistanceFieldDetail(vec3 pos, float scale, float timeScale, float amplitude)
{
    float v1 = movingTurbulence(pos * scale, timeScale) * amplitude * 1.0;
    return pos.y + v1;
}

vec3 oceanNormal(vec3 inputPosition, float scale, float timeScale, float amplitude)
{
    vec3 position = inputPosition;
    vec3 normal;
    vec2 d = vec2(length(position) * 0.01, 0.0);

    normal.x = oceanDistanceFieldDetail(position + d.xyy, scale, timeScale, amplitude) - oceanDistanceFieldDetail(position - d.xyy, scale, timeScale, amplitude);
    normal.y = oceanDistanceFieldDetail(position + d.yxy, scale, timeScale, amplitude) - oceanDistanceFieldDetail(position - d.yxy, scale, timeScale, amplitude);
    normal.z = oceanDistanceFieldDetail(position + d.yyx, scale, timeScale, amplitude) - oceanDistanceFieldDetail(position - d.yyx, scale, timeScale, amplitude);

    return normalize(normal);
}

//-------------------------------------------------------------------------------------------------
// Terrain noise

float terrainDistanceFieldDetail(vec3 pos, float scale, float amplitude)
{
    float v1 = staticTurbulence(pos * scale) * amplitude * 1.0;
    return pos.y + v1;
}

vec3 terrainNormal(vec3 inputPosition, float scale, float amplitude)
{
    vec3 pos = inputPosition;
    vec3 norm;
    vec2 d = vec2(length(pos) * 0.01, 0.0);

    norm.x = terrainDistanceFieldDetail(pos + d.xyy, scale, amplitude) - terrainDistanceFieldDetail(pos - d.xyy, scale, amplitude);
    norm.y = terrainDistanceFieldDetail(pos + d.yxy, scale, amplitude) - terrainDistanceFieldDetail(pos - d.yxy, scale, amplitude);
    norm.z = terrainDistanceFieldDetail(pos + d.yyx, scale, amplitude) - terrainDistanceFieldDetail(pos - d.yyx, scale, amplitude);

    return normalize(norm);
}

/*
//-------------------------------------------------------------------------------------------------
// Volumetric lights

float getTransmittance(vec3 ro, vec3 rd)
{
    vec3 step = rd * gStepSize;
    vec3 pos = ro;

    float tm = 1.0;

    for (int i = 0; i < MAX_STEPS; ++i)
    {
        tm *= exp(-uTMK * gStepSize * perlin(pos));

        pos += step;

        if (tm < TM_MIN ||
            pos.x > 1.0 || pos.x < 0.0 ||
            pos.y > 1.0 || pos.y < 0.0 ||
            pos.z > 1.0 || pos.z < 0.0)
            break;
    }

    return tm;
}

vec4 rayMarchLight(vec3 ro, vec3 rd)
{
    vec3 step = rd * gStepSize;
    vec3 pos = ro;

    vec3 col = vec3(0.0);   // accumulated color
    float tm = 1.0;         // accumulated transmittance

    for (int i = 0; i < MAX_STEPS; ++i)
    {
        // delta transmittance
        float dtm = exp(-uTMK * gStepSize * perlin(pos));
        tm *= dtm;

        // get contribution per light
        for (int k = 0; k < u_num_lights; ++k)
        {
            if (u_light_direction[k] != vec3(0.0, 0.0, 0.0))
            {
                vec3 ld = normalize(u_light_position[k] - pos);
                float ltm = getTransmittance(pos, ld);
                col += (1.0 - dtm) * u_material_diffuse.xyz * u_light_color[k] * tm * ltm;
            }
        }

        pos += step;

        if (tm < TM_MIN ||
            pos.x > 1.0 || pos.x < 0.0 ||
            pos.y > 1.0 || pos.y < 0.0 ||
            pos.z > 1.0 || pos.z < 0.0)
            break;
    }

    float alpha = 1.0 - tm;
    return vec4(col/alpha, alpha);
}

vec4 volumetricLights(vec3 pos)
{
    vec3 color;

    // step_size = root_three / max_steps ; to get through diagonal
    gStepSize = ROOTTHREE / float(MAX_STEPS);
    gStepFactor = 32.0 * gStepSize;

    vec3 cam = (u_camera_matrix * vec4(0.0, 0.0, 0.0, 0.0)).xyz;

    return rayMarchLight(pos, pos - cam);
}
*/

//-------------------------------------------------------------------------------------------------
// Lights

vec4 uberShader(
        vec3 lightDirection,
        vec3 viewDirection,
        vec3 surfaceNormal,
        vec4 diffuseColor,
        vec4 glossyColor,
        float glossiness,
        float metalness,
        float IOR
        )
{
    float roughness = clamp(1.0 - glossiness, 0.1, 1.0);
    float diffuseAmount = orenNayarDiffuse(lightDirection, viewDirection, surfaceNormal, roughness, 1.0);
    float glossyAmount = ggxGlossy(lightDirection, viewDirection, surfaceNormal, roughness);

    vec3 dielectricColor = diffuseColor.rgb * diffuseAmount;
    vec3 metallicColor = glossyColor.rgb * glossyAmount;
    vec3 finalColor = mix(dielectricColor, metallicColor, metalness);

    return vec4(finalColor.rgb, 1.0);
}

vec3 doLighting(vec3 worldPosition, vec3 surfaceNormal, vec3 viewDirection, vec2 normScreenCoords)
{
    vec3 finalColor = vec3(0.0, 0.0, 0.0);

    float selfIllumination = u_material_self_illum;

    if (bool(u_IR_enable))
    {
        if (selfIllumination < u_IR_factor) selfIllumination = u_IR_factor;
    }

    for (int index = 0; index < u_num_lights; index++)
    {
        vec3 rawLightRay = u_light_position[index] - worldPosition;
        float lightRayLength = distance(rawLightRay);

        if (u_light_distance[index] == 0.0 || lightRayLength < u_light_distance[index])
        {
            // Light ray
            vec3 lightRay = normalize(rawLightRay);

            // Compute distance attenuation
            float attenuation = 1.0;

            if (u_light_distance[index] > 0.0)
            {
                attenuation = clamp(1.0 - (lightRayLength / u_light_distance[index]), 0.0, 1.0);
            }

            // Compute spot attenuation
            if (u_light_direction[index] != vec3(0.0, 0.0, 0.0) && u_light_spot_angle[index] != 0.0)
            {
                float spotCutoff = cos(u_light_spot_angle[index]);
                float dotDirectionRay = dot(-u_light_direction[index], lightRay);

                if (dotDirectionRay < spotCutoff)
                {
                    attenuation = 0.0;
                }
            }

            // Continue if attenuation is not 0
            if (attenuation > 0.0)
            {
                vec4 diffuseColor = uberShader(
                            lightRay,
                            viewDirection,
                            surfaceNormal,
                            u_material_diffuse,
                            u_material_specular,
                            u_material_shininess,
                            u_material_metalness,
                            1.4
                            );

                finalColor = finalColor + (diffuseColor.rgb * u_light_color[index] * attenuation);
            }
        }
    }

    // Return material ambient plus direct light. Global ambient is added
    // after texturing in main(), otherwise a dark texture would absorb it
    return u_material_ambient.xyz + u_material_diffuse.xyz * selfIllumination + finalColor;
}

//-------------------------------------------------------------------------------------------------
// Fog

vec4 computeLinearFog(vec4 color)
{
    // Compute fog distance, increase if rendering sky
    float fogDistance = u_fog_distance;
    if (bool(u_sky_enable)) fogDistance *= 4.0;

    // Compute fog factor based on distance and atmosphere
    float fogFactor = clamp(((fogDistance - vo_distance) / fogDistance), 0.0, 1.0);
    float fogDensity = mix(1.0, FOG_DENSE_FALLOFF_EXPONENT, clamp(u_fog_level, 0.0, 1.0));
    fogFactor = pow(1.0 - fogFactor, fogDensity) * gAtmosphereFactor;

    // Compute attenuated sun color
    float average = (0.2125 * u_sun_color.r) + (0.7154 * u_sun_color.g) + (0.0721 * u_sun_color.b);
    vec3 sunIntensity = vec3(average, average, average);
    vec3 sunLight = mix(u_sun_color, sunIntensity, 0.5);
    // vec3 sun_light = average;

    // Compute color, mix between fog color and sun color
    vec3 fogColor = mix(sunLight * 0.75, u_fog_color, fogFactor);

    // Attenuate if IR
    if (bool(u_IR_enable)) fogColor *= 0.6;

    return vec4(color.xyz * (1.0 - fogFactor) + fogColor * fogFactor, color.a);
}

//-------------------------------------------------------------------------------------------------
// Compute normal

vec3 getNormal()
{
    // Input normal
    vec3 normal = vo_normal;

    // In case of water, perturbate normal
    if (bool(u_wave_enable))
    {
        if (u_shaderQuality >= 0.75)
        {
            float large_amplitude = u_wave_amplitude * 0.3;
            float small_amplitude = large_amplitude * 2.0;

            mat3 toLocalTangent = inverse(mat3(
                                              vo_tangent.x, vo_normal.x, vo_binormal.x,
                                              vo_tangent.y, vo_normal.y, vo_binormal.y,
                                              vo_tangent.z, vo_normal.z, vo_binormal.z
                                              ));

            // Perturbate normal
            vec3 normal_1 = oceanNormal(vo_texcoord, 0.05, 0.1, large_amplitude).xyz;
            vec3 normal_2 = vec3(0.0, 0.0, 0.0);
            vec3 normal_3 = vec3(0.0, 0.0, 0.0);

            if (vo_distance < 1000.0)
            {
                float factor = (1000.0 - vo_distance) / 1000.0;
                normal_2 = oceanNormal(vo_texcoord, 3.0, 0.5, small_amplitude).xyz * factor;
            }

            if (vo_distance < 500.0)
            {
                float factor = (500.0 - vo_distance) / 500.0;
                normal_3 = oceanNormal(vo_texcoord * -1.0, 10.0, 2.0, small_amplitude).xyz * factor;
            }

            normal = normalize(toLocalTangent * (normal_1 + normal_2 + normal_3));
            normal = (u_model_matrix * vec4(normal, 0.0)).xyz;
        }
    }
    /*
    else
    {
        mat3 toLocalTangent = inverse(mat3(
                                          vo_tangent.x, vo_normal.x, vo_binormal.x,
                                          vo_tangent.y, vo_normal.y, vo_binormal.y,
                                          vo_tangent.z, vo_normal.z, vo_binormal.z
                                          ));

        // Perturbate normal
        vec3 normal_1 = terrainNormal(v_position, 0.005, 2.0).xyz;

        normal = normalize(toLocalTangent * normal_1);
        normal = (u_model_matrix * vec4(normal, 0.0)).xyz;
    }
    */

    return normal;
}

//-------------------------------------------------------------------------------------------------
// Get texture

vec4 getTexture()
{
    vec4 textureColor = vec4(0.0, 0.0, 0.0, 1.0);

    if (bool(u_texture_diffuse_enable) && u_shaderQuality >= 0.20)
    {
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_0, vo_texcoord.xy), vo_difftex_weight_0);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_1, vo_texcoord.xy), vo_difftex_weight_1);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_2, vo_texcoord.xy), vo_difftex_weight_2);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_3, vo_texcoord.xy), vo_difftex_weight_3);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_4, vo_texcoord.xy), vo_difftex_weight_4);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_5, vo_texcoord.xy), vo_difftex_weight_5);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_6, vo_texcoord.xy), vo_difftex_weight_6);
        textureColor = mix(textureColor, texture2D(u_texture_diffuse_7, vo_texcoord.xy), vo_difftex_weight_7);
    }
    else
    {
        return vec4(1.0, 1.0, 1.0, 1.0);
    }

    return textureColor;
}

//-------------------------------------------------------------------------------------------------
// Get shadow

vec3 getShadow()
{
    vec3 color = vec3(1.0, 1.0, 1.0);

    if (bool(u_shadow_enable) && u_num_lights > 0)
    {
        vec3 sc = vo_shadow_coord.xyz;

        sc /= vo_shadow_coord.w;
        sc += 1.0;
        sc *= 0.5;

        // Get shadow

        if (
                sc.x > 0.0 && sc.x < 1.0 &&
                sc.y > 0.0 && sc.y < 1.0 &&
                sc.z > 0.0 && sc.z < 1.0
                )
        {
            float shadowDepth = texture2D(u_shadow_texture, sc.xy).r;

            if (shadowDepth < sc.z - 0.001)
            {
                color = vec3(0.25, 0.25, 0.25);
            }
        }
    }

    return color;
}

//-------------------------------------------------------------------------------------------------
// Effects

vec3 postEffects(vec3 rgb, vec2 xy)
{
    // Contrast / saturation / brightness from the rendering environment
    rgb = mix(vec3(0.5), mix(vec3(dot(vec3(0.2125, 0.7154, 0.0721), rgb * u_brightness)), rgb * u_brightness, u_saturation), u_contrast);

    // Vignette
    // rgb *= 0.4 + 0.5 * pow(40.0 * xy.x * xy.y * (1.0 - xy.x) * (1.0 - xy.y), 0.2);

    // rgb = (1.0 - exp(-rgb * 6.0)) * 1.0024;

    return rgb;
}

//-------------------------------------------------------------------------------------------------

void main()
{
    if (bool(u_depth_computing))
    {
        float pixelDistance = 1.0;
        float pixelIncidence = 0.0;

        if (vo_distance > 0.0 && vo_distance < 10000.0)
        {
            // The normalized distance
            pixelDistance = vo_distance / 10000.0;

            // The eye-to-vertex vector
            vec3 eyeToVertex = normalize(vo_position - u_camera_position);

            // Angle of incidence between view vector and vertex' normal vector
            pixelIncidence = clamp(dot(vo_normal * -1.0, eyeToVertex), 0.0, 1.0);
        }

        // Red = distance, green = incidence, blue = 0
        gl_FragColor = vec4(pixelDistance, pixelIncidence, 0.0, 1.0);
    }
    else
    {
        if (bool(u_rendering_shadows))
        {
            if (vo_distance >= 1.0 && vo_distance < 2000.0)
            {
                float d = (vo_distance - 1.0) / 1999.0;

                gl_FragColor = vec4(d, d, d, 1.0);
            }
            else
            {
                gl_FragColor = vec4(1.0, 1.0, 1.0, 0.0);
            }
        }
        else
        {
            if (bool(u_normals_only))
            {
                vec3 color = vec3((vo_normal.x + 1.0) * 0.5, (vo_normal.y + 1.0) * 0.5, (vo_normal.z + 1.0) * 0.5);
                gl_FragColor = vec4(color, 1.0);
            }
            else
            {
                // Atmosphere factor
                gAtmosphereFactor = clamp((u_atmosphere_altitude - u_camera_altitude) / u_atmosphere_altitude, 0.0, 1.0);

                // Altitude factors for sea
                gSeaAltitudeFactor_1 = clamp((vo_altitude * -1.0) / 2.0, 0.0, 1.0);
                gSeaAltitudeFactor_2 = clamp((vo_altitude * -1.0) / 10.0, 0.0, 1.0);

                // Fragment normal
                vec3 normal = getNormal();

                // Eye-to-vertex vector
                vec3 eyeToVertex = normalize(vo_position - u_camera_position);

                // Screen coordinates
                vec2 xy = gl_FragCoord.xy / u_resolution.xy;

                // Input color
                vec4 color = vec4(doLighting(vo_position, normal, eyeToVertex, xy), 1.0);

                // Apply texture
                color *= getTexture();

                // Apply shadow
                color *= vec4(getShadow(), 1.0);

                // Apply global ambient after texture and shadow so it lifts
                // dark textured ground as much as untextured objects
                color.rgb += u_global_ambient;

                // Apply sky if asked
                if (bool(u_sky_enable))
                {
                    color = vec4(standardSkyAt(u_camera_position, eyeToVertex, xy), color.a);
                }

                // Apply water if asked
                if (bool(u_wave_enable))
                {
                    if (u_shaderQuality < 0.25)
                    {
                    }
                    else if (u_shaderQuality < 0.75)
                    {
                        float largeAmplitude = 2.5;
                        float smallAmplitude = largeAmplitude * 2.0;

                        float v1 = movingTurbulence(vo_texcoord * 0.05, 0.1) * largeAmplitude;
                        float v2 = movingTurbulence(vo_texcoord * 3.0, 0.5) * largeAmplitude;
                        float v3 = 0.0;

                        if (vo_distance < 500.0)
                        {
                            v3 = movingTurbulence(vo_texcoord * 20.0, 0.7) * smallAmplitude;
                        }

                        float turb = clamp((v1 + v2 + v3) * 0.5, 1.0, 100.0);

                        color = color * vec4(turb, turb, turb, 1.0);

                        // Compute alpha
                        color.a = gSeaAltitudeFactor_1;
                    }
                    else
                    {
                        // Compute alpha
                        color.a = gSeaAltitudeFactor_1;
                    }
                }

                // Apply atmospheric light - does not work yet
                // color = color + volumetricLights(v_position);

                // Apply fog
                if (bool(u_fog_enable))
                {
                    color = computeLinearFog(color);
                }

                // Apply IR if asked
                if (bool(u_IR_enable))
                {
                    float average = 0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b;
                    vec2 seed = vec2(sin(gl_FragCoord.x + u_time), cos(gl_FragCoord.y + u_time));
                    float value = average + random(seed) * 0.01;
                    color = vec4(value, value, value, color.a);
                }

                // Apply polarity if asked
                if (bool(u_inverse_polarity_enable))
                {
                    color.r = 1.0 - color.r;
                    color.g = 1.0 - color.g;
                    color.b = 1.0 - color.b;
                }

                // Light rays
                if (bool(u_lightrays_enable))
                {
                    color += vec4(lightRays(u_camera_position, eyeToVertex, xy), 0.0);
                }

                // Apply effects
                color = vec4(postEffects(color.rgb, xy), color.a);

                // Final color
                gl_FragColor = color;

                if (color.a < 0.01)
                {
                    discard;
                }
            }
        }
    }
}
