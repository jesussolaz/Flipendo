/* Filtro 2D nativo de Flipendo: look "Kingdom Hearts".
 * Bloom (glow ancho) + color grading + viñeta, en un solo pase.
 * Migrado desde flipfx.py (Fase A, doctrina C++). Constantes horneadas:
 * es un filtro integrado sin uniforms, como Sobel/Invert. */

const float BLOOM_THR    = 0.62;
const float BLOOM_INT    = 1.6;
const float BLOOM_SPREAD = 42.0;   // radio del glow en pixeles
const float CONTRAST     = 1.08;
const float SATURATION    = 1.30;
const vec3  TINT         = vec3(1.03, 1.01, 1.05);
const float VIGNETTE     = 0.38;

vec3 bright(vec2 uv)
{
  vec3 c = texture(bgl_RenderedTexture, uv).rgb;
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  float k = max(0.0, l - BLOOM_THR) / max(1.0 - BLOOM_THR, 0.001);
  return c * k;
}

vec3 bloom(vec2 uv)
{
  vec2 texel = vec2(1.0 / g_data.width, 1.0 / g_data.height);
  vec3 acc = bright(uv) * 0.10;
  float wsum = 0.10;
  const int DIRS = 20;
  const int RINGS = 5;
  for (int r = 1; r <= RINGS; ++r) {
    float fr = float(r) / float(RINGS);
    float radius = BLOOM_SPREAD * fr;
    float w = exp(-fr * fr * 3.0);
    float rot = 0.31416 * float(r);
    for (int d = 0; d < DIRS; ++d) {
      float ang = 6.2831853 * float(d) / float(DIRS) + rot;
      vec2 off = vec2(cos(ang), sin(ang)) * radius * texel;
      acc += bright(uv + off) * w;
      wsum += w;
    }
  }
  return acc / wsum;
}

vec3 grade(vec3 c)
{
  c = (c - 0.5) * CONTRAST + 0.5;
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = mix(vec3(l), c, SATURATION);
  c *= TINT;
  return c;
}

float vignette(vec2 uv)
{
  vec2 d = uv - 0.5;
  return 1.0 - VIGNETTE * smoothstep(0.15, 0.75, dot(d, d));
}

void main(void)
{
  vec2 uv = bgl_TexCoord.xy;
  vec3 c = texture(bgl_RenderedTexture, uv).rgb;
  c += bloom(uv) * BLOOM_INT;
  c = grade(c);
  c *= vignette(uv);
  fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
