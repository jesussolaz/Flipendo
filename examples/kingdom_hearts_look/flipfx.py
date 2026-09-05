"""flipfx - post-proceso "look Kingdom Hearts" para Flipendo (Metal/macOS).

Un filtro 2D nativo en un solo pase: threshold de brillo -> bloom gaussiano
separable barato -> color grading (contraste, saturacion, tinte) -> viñeta.
Usa la sintaxis 2D-filter de Flipendo, que compila en Metal.

Uso A (componente, recomendado): adjunta flipfx.PostFX a la camara del juego.
Uso B (script): llama a flipfx.install(scene) una vez al arrancar.
"""
import bge

# --- fragment shader del filtro (sintaxis Flipendo: bgl_RenderedTexture, bgl_TexCoord, fragColor) ---
_FRAG = """
uniform float u_bloom_thr;    // umbral de brillo para el bloom
uniform float u_bloom_int;    // intensidad del bloom
uniform float u_bloom_spread; // radio del glow en pixeles
uniform float u_contrast;
uniform float u_saturation;
uniform vec3  u_tint;         // tinte multiplicativo (look KH: azulado/calido)
uniform float u_vignette;     // 0 = nada, 1 = fuerte
uniform float u_px;           // 1.0 / ancho
uniform float u_py;           // 1.0 / alto

vec3 sample_scene(vec2 uv){ return texture(bgl_RenderedTexture, uv).rgb; }

// brillo por encima del umbral, para el bloom
vec3 bright(vec2 uv){
  vec3 c = sample_scene(uv);
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  float k = max(0.0, l - u_bloom_thr) / max(1.0 - u_bloom_thr, 0.001);
  return c * k;
}

// bloom: glow ancho de verdad. Muestreo radial en 24 direcciones a 3 radios
// crecientes (hasta ~u_bloom_spread px), pesado por gaussiana. Un solo pase,
// barato en un rasterizador con margen de sobra.
vec3 bloom(vec2 uv){
  vec2 texel = vec2(u_px, u_py);
  vec3 acc = bright(uv) * 0.10;
  float wsum = 0.10;
  const int DIRS = 20;
  const int RINGS = 5;
  for (int r = 1; r <= RINGS; ++r){
    float fr = float(r) / float(RINGS);
    float radius = u_bloom_spread * fr;                     // px
    float w = exp(-fr * fr * 3.0);                          // gaussiana continua
    float rot = 0.31416 * float(r);                         // rota cada anillo -> sin pétalos
    for (int d = 0; d < DIRS; ++d){
      float ang = 6.2831853 * float(d) / float(DIRS) + rot;
      vec2 off = vec2(cos(ang), sin(ang)) * radius * texel;
      acc += bright(uv + off) * w;
      wsum += w;
    }
  }
  return acc / wsum;
}

vec3 grade(vec3 c){
  c = (c - 0.5) * u_contrast + 0.5;                       // contraste
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = mix(vec3(l), c, u_saturation);                      // saturacion
  c *= u_tint;                                            // tinte
  return c;
}

float vignette(vec2 uv){
  vec2 d = uv - 0.5;
  float r = dot(d, d);
  return 1.0 - u_vignette * smoothstep(0.15, 0.75, r);
}

void main(){
  vec2 uv = bgl_TexCoord.xy;
  vec3 c = sample_scene(uv);
  c += bloom(uv) * u_bloom_int;      // aditivo
  c = grade(c);
  c *= vignette(uv);
  fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
"""

# preset "Kingdom Hearts": colores vivos, bloom marcado, ligero tinte frio en sombras
PRESET_KH = {
    "u_bloom_thr": 0.62, "u_bloom_int": 1.6, "u_bloom_spread": 42.0,
    "u_contrast": 1.08, "u_saturation": 1.30,
    "u_tint": (1.03, 1.01, 1.05), "u_vignette": 0.38,
}


def install(scene=None, preset=None):
    """Instala el filtro en el pase 0 y devuelve el objeto filtro."""
    scene = scene or bge.logic.getCurrentScene()
    p = dict(PRESET_KH); 
    if preset: p.update(preset)
    fm = scene.filterManager
    f = fm.addFilter(0, bge.logic.RAS_2DFILTER_CUSTOMFILTER, _FRAG)
    res = bge.render.getWindowWidth(), bge.render.getWindowHeight()
    f.setUniform1f("u_bloom_thr", p["u_bloom_thr"])
    f.setUniform1f("u_bloom_int", p["u_bloom_int"])
    f.setUniform1f("u_bloom_spread", p["u_bloom_spread"])
    f.setUniform1f("u_contrast",  p["u_contrast"])
    f.setUniform1f("u_saturation",p["u_saturation"])
    f.setUniform3f("u_tint", *p["u_tint"])
    f.setUniform1f("u_vignette", p["u_vignette"])
    f.setUniform1f("u_px", 1.0 / max(res[0], 1))
    f.setUniform1f("u_py", 1.0 / max(res[1], 1))
    return f


class PostFX(bge.types.KX_PythonComponent):
    """Componente para la camara: aplica el look KH al arrancar."""
    from collections import OrderedDict as _OD
    args = _OD([
        ("bloom_intensidad", 1.35),
        ("bloom_umbral", 0.62),
        ("contraste", 1.10),
        ("saturacion", 1.28),
        ("vignette", 0.38),
    ])

    def start(self, args):
        preset = {
            "u_bloom_int": args["bloom_intensidad"],
            "u_bloom_thr": args["bloom_umbral"],
            "u_contrast": args["contraste"],
            "u_saturation": args["saturacion"],
            "u_vignette": args["vignette"],
        }
        self.filtro = install(self.object.scene, preset)

    def update(self):
        pass
