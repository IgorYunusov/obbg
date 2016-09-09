#define _WIN32_WINNT 0x400
//#define GL_DEBUG

#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define STB_DEFINE
#include "stb.h"

#include "obbg_funcs.h"

// stb_gl.h
#define STB_GL_IMPLEMENTATION
#define STB_GLEXT_DEFINE "glext_list.h"
#include "stb_gl.h"

// SDL
#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_net.h"

// stb_glprog.h
#define STB_GLPROG_IMPLEMENTATION
#define STB_GLPROG_ARB_DEFINE_EXTENSIONS
#include "stb_glprog.h"

// stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// stb_easy_font.h
#include "stb_easy_font.h" // doesn't require an IMPLEMENTATION

char *game_name = "obbg";

#define REVERSE_DEPTH


char *dumb_fragment_shader =
   "#version 150 compatibility\n"
   "uniform sampler2DArray tex;\n"
   "void main(){gl_FragColor = gl_Color*texture(tex,gl_TexCoord[0].xyz);}";


extern int load_crn_to_texture(unsigned char *data, size_t length);
extern int load_crn_to_texture_array(int slot, unsigned char *data, size_t length);
extern int load_bitmap_to_texture_array(int slot, unsigned char *data, int w, int h, int wrap, int premul);

GLuint debug_tex, dumb_prog;
GLuint voxel_tex[2];
GLuint sprite_tex;
int debug_render;


typedef struct
{
   float scale;
   char *filename;
} texture_info;

texture_info textures[] =
{
   1,"machinery/conveyor_90_00",
   1.0/4,"ground/Bowling_grass_pxr128",
   1,"ground/Dirt_and_gravel_pxr128",
   1,"ground/Fine_gravel_pxr128",
   1.0/2,"ground/Ivy_pxr128",
   1,"ground/Lawn_grass_pxr128",
   1,"ground/Pebbles_in_mortar_pxr128",
   1,"ground/Peetmoss_pxr128",

   1,"ground/Red_gravel_pxr128",
   1,"ground/Street_asphalt_pxr128",
   1,"floor/Wool_carpet_pxr128",
   1,"brick/Pink-brown_painted_pxr128",
   1,"brick/Building_block_pxr128",
   1,"brick/Standard_red_pxr128",
   1,"siding/Diagonal_cedar_pxr128",
   1,"siding/Vertical_redwood_pxr128",

   1,"machinery/conveyor_90_01",
   1,"stone/Buffed_marble_pxr128",
   1,"stone/Black_marble_pxr128",
   1,"stone/Blue_marble_pxr128",
   1,"stone/Gray_granite_pxr128",
   1,"metal/Round_mesh_pxr128",
   1,"machinery/conveyor",
   1,"machinery/ore_maker",

   1,"machinery/ore_eater",
   1.0/8,"ground/Beach_sand_pxr128",
   1,"stone/Gray_marble_pxr128",
   0,0,
   0,0,
   0,0,
   0,0,
   0,0,

   1,"machinery/conveyor_90_02", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_90_03", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_00", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_01", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_02", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_03", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,
};

static float camera_bounds[2][3];

void game_init(void)
{
   init_chunk_caches();
   init_mesh_building();
   init_mesh_build_threads();
   s_init_physics_cache();
   logistics_init();
}

static uint8 blinn_8x8(uint8 x, uint8 y)
{
   uint32 t = x*y + 128;
   return (uint8) ((t + (t >>8)) >> 8);
}

typedef struct
{
   vec pos;
   float size;
   int id;
   vec color;
} sprite;

#define MAX_SPRITES 40000
sprite sprites[MAX_SPRITES];
int num_sprites;

#pragma warning(disable:4244)

#ifdef _MSC_VER
#pragma warning(disable:4305)
#endif
float it_color[20][3] =
{
   { 0,0,0, },
   { 0.1,0.1,0.1 }, // IT_coal
   { 0.6,0.5,0.5 }, // IT_iron_ore
   { 0.6,0.6,0.3 }, // IT_copper_ore
   { 0,0,0 },
   { 0,0,0 },
   { 0,0,0 },
   { 0,0,0 },
   { 1.0,1.0,1.0 }, // IT_iron_bar
   { 0.7,0.7,0.8 }, // IT_iron_gear
   { 0,0,0 },       // IT_steel_plate
   { 1.0,0.0,1.0 }, // IT_conveyor_belt
};

void add_sprite(float x, float y, float z, int id)
{
   sprite *s = &sprites[num_sprites++];
   s->pos.x = x;
   s->pos.y = y;
   s->pos.z = z;
   s->size = 0.25;
   assert(id >= 1 && id < sizeof(it_color)/12);
   s->id = 0;
   s->color.x = it_color[id][0];
   s->color.y = it_color[id][1];
   s->color.z = it_color[id][2];
}

void premultiply_alpha(uint8 *pixels, int w, int h)
{
   int i;
   for (i=0; i < w*h; ++i) {
      pixels[i*4+0] = blinn_8x8(pixels[i*4+0], pixels[i*4+3]);
      pixels[i*4+1] = blinn_8x8(pixels[i*4+1], pixels[i*4+3]);
      pixels[i*4+2] = blinn_8x8(pixels[i*4+2], pixels[i*4+3]);
   }
}

GLuint load_sprite(char *filename)
{
   int w,h;
   GLuint tex;
   uint8 *data;

   data = stbi_load(filename, &w, &h, 0, 4);
   assert(data != NULL);
   premultiply_alpha(data, w, h);

   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
   free(data);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   glGenerateMipmapEXT(GL_TEXTURE_2D);
   return tex;
}



void render_init(void)
{
   // @TODO: support non-DXT path
   char **files = stb_readdir_recursive("data", "*.crn");
   int i;

   camera_bounds[0][0] = - 0.75f;
   camera_bounds[0][1] = - 0.75f;
   camera_bounds[0][2] = - 4.25f;
   camera_bounds[1][0] = + 0.75f;
   camera_bounds[1][1] = + 0.75f;
   camera_bounds[1][2] = + 0.25f;

   glGenTextures(2, voxel_tex);

   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, voxel_tex[0]);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   for (i=0; i < 11; ++i) {
      glTexImage3DEXT(GL_TEXTURE_2D_ARRAY_EXT, i,
                         GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                         1024>>i,1024>>i,128,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   }

   for (i=0; i < sizeof(textures)/sizeof(textures[0]); ++i) {
      if (textures[i].scale != 0) {
         size_t len;
         char *filename = stb_sprintf("data/pixar/crn/%s.crn", textures[i].filename);
         uint8 *data = stb_file(filename, &len);
         if (data == NULL)
            data = stb_file(stb_sprintf("data/%s.crn", textures[i].filename), &len);
         if (data == NULL) {
            int w,h;
            uint8 *pixels = stbi_load(stb_sprintf("data/%s.jpg", textures[i].filename), &w, &h, 0, 4);
            if (!pixels)
               pixels = stbi_load(stb_sprintf("data/%s.png", textures[i].filename), &w, &h, 0, 4);
            
            if (pixels) {
               load_bitmap_to_texture_array(i, pixels, w, h, 1, 0);
               free(pixels);
            } else
               assert(0);
         } else {
            load_crn_to_texture_array(i, data, len);
            free(data);
         }
      }
   }

   // temporary hack:
   voxel_tex[1] = voxel_tex[0];


   init_voxel_render(voxel_tex);
   init_object_render();

   {
      char const *frag[] = { dumb_fragment_shader, NULL };
      char error[1024];
      GLuint fragment;
      fragment = stbgl_compile_shader(STBGL_FRAGMENT_SHADER, frag, -1, error, sizeof(error));
      if (!fragment) {
         ods("oops");
         exit(0);
      }
      dumb_prog = stbgl_link_program(0, fragment, NULL, -1, error, sizeof(error));
      
   }

   #if 0
   {
      size_t len;
      unsigned char *data = stb_file(files[0], &len);
      glGenTextures(1, &debug_tex);
      glBindTexture(GL_TEXTURE_2D, debug_tex);
      load_crn_to_texture(data, len);
      free(data);
   }
   #endif

   glGenTextures(1, &sprite_tex);
   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, sprite_tex);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   for (i=0; i < 11; ++i) {
      glTexImage3DEXT(GL_TEXTURE_2D_ARRAY_EXT, i,
                         GL_RGBA,
                         256>>i,256>>i,256,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   }

   {
      char *filename = stb_sprintf("data/sprites/ore.png", textures[i].filename);
      int w,h;
      uint8 *pixels = stbi_load(filename, &w, &h, 0, 4);
      premultiply_alpha(pixels, w, h);
      if (pixels) {
         load_bitmap_to_texture_array(0, pixels, w, h, 0, 1);
         free(pixels);
      } else
         assert(0);
   }

   init_ui_render();

   #if 0
   for (i=0; i < 500; ++i) {
      sprite *s = &sprites[num_sprites++];
      s->pos.x = stb_frand() * 100 - 50;
      s->pos.y = stb_frand() * 100 - 50;
      s->pos.z = stb_frand() * 50 + 64;
      s->size = stb_rand() & 1 ? 0.5 : 0.125;
   }                
   #endif

   build_picker();
}


static void print_string(float x, float y, char *text, float r, float g, float b)
{
   static char buffer[99999];
   int num_quads;
   
   num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

   glColor3f(r,g,b);
   glEnableClientState(GL_VERTEX_ARRAY);
   glVertexPointer(2, GL_FLOAT, 16, buffer);
   glDrawArrays(GL_QUADS, 0, num_quads*4);
   glDisableClientState(GL_VERTEX_ARRAY);
}

static float text_color[3];
static float pos_x = 10;
static float pos_y = 10;

static void print(char *text, ...)
{
   char buffer[999];
   va_list va;
   va_start(va, text);
   vsprintf(buffer, text, va);
   va_end(va);
   print_string(pos_x, pos_y, buffer, text_color[0], text_color[1], text_color[2]);
   pos_y += 10;
}

//object player = { { 0,0,150 } };


float camang[3], camloc[3] = { 0,-10,80 };

// camera worldspace velocity
float light_pos[3];
float light_vel[3];

float pending_view_x;
float pending_view_z;

player_controls client_player_input;

float pending_dt;

int program_mode = MODE_single_player;

void process_tick(float dt)
{
   dt += pending_dt;
   pending_dt = 0;
   while (dt > 1.0f/60) {
      switch (program_mode) {
         case MODE_server:
            server_net_tick_pre_physics();
            process_tick_raw(1.0f/60);
            server_net_tick_post_physics();
            break;
         case MODE_client:
            client_view_physics(player_id, &client_player_input, dt);
            client_net_tick();
            break;
         case MODE_single_player:
            client_view_physics(player_id, &client_player_input, dt);
            p_input[player_id] = client_player_input;
            process_tick_raw(1.0f/60);
            break;
      }

      dt -= 1.0f/60;
   }
   pending_dt += dt;
}

void update_view(float dx, float dy)
{
   // hard-coded mouse sensitivity, not resolution independent?
   pending_view_z -= dx*300;
   pending_view_x -= dy*700;
}

float render_time, tick_time;

int screen_x, screen_y;
int is_synchronous_debug;
int sort_order_for_selected_belt;

size_t mesh_cache_requested_in_use;
int chunk_locations, chunks_considered, chunks_in_frustum;
int quads_considered, quads_rendered;
int chunk_storage_rendered, chunk_storage_considered, chunk_storage_total;
int view_dist_for_display;
int num_threads_active, num_meshes_started, num_meshes_uploaded;
float chunk_server_activity;

float chunk_server_status[32];
int chunk_server_pos;

extern vec3i physics_cache_feedback[64][64];
extern int num_gen_chunk_alloc;


stb_sdict *memstats_table;
typedef struct
{
   char *info;
   size_t total;
   size_t largest;
   size_t count;
} memstats;

void dump_callback(size_t size, char *info)
{
   memstats *ms = stb_sdict_get(memstats_table, info);
   if (ms == NULL) {
      ms = malloc(sizeof(*ms));
      ms->count = 1;
      ms->total = size;
      ms->largest = size;
      ms->info = info;
      stb_sdict_add(memstats_table, info, ms);
   } else {
      ++ms->count;
      ms->total += size;
      ms->largest = stb_max(ms->largest, size);
   }
}

void print_column(char *str, int id)
{
   static float xtab[4] = { 30,90,150,180 };
   float x = xtab[id];
   if (id >= 0 && id <= 2) {
      x -= stb_easy_font_width(str);
   }
   print_string(x, pos_y, str, text_color[0], text_color[1], text_color[2]);
}

void dump_memory(void)
{
   int i;
   char *info;
   memstats *arr=NULL;
   memstats *ms;
   memstats_table = stb_sdict_new(1);
   obbg_malloc_dump(dump_callback);

   stb_sdict_for(memstats_table, i, info, ms) {
      stb_arr_push(arr, *ms);
   }

   qsort(arr, stb_arr_len(arr), sizeof(*arr), stb_intcmp(offsetof(memstats,total)));

   for (i=stb_arr_len(arr)-1; i >= 0; --i) {
      ms = &arr[i];

      print_column(stb_sprintf("%9d", ms->count), 0);
      print_column(stb_sprintf("%9d", ms->total), 1);
      print_column(stb_sprintf("%9d", ms->largest), 2);
      print_column(ms->info, 3);
      print("");
   }
   stb_sdict_delete(memstats_table);
}

struct
{
   float count[32];
   double time[32];
} thread_timing[MAX_MESH_WORKERS];

void update_thread_times(void)
{
   int i,j;
   for (i=0; i < MAX_MESH_WORKERS; ++i) {
      int count[32];
      double time[32];
      query_thread_info(i, count, time);
      for (j=0; j<32; ++j) {
         thread_timing[i].count[j] = count[j];
         thread_timing[i].time[j] = time[j];
      }
   }
}

Bool show_memory;
void draw_stats(void)
{
   static double last_update_time;
   static Bool initialize_update = True;

   int i;

   static Uint64 last_frame_time;
   Uint64 cur_time = SDL_GetPerformanceCounter();
   float chunk_server=0;
   float frame_time = (cur_time - last_frame_time) / (float) SDL_GetPerformanceFrequency();
   last_frame_time = cur_time;

   if (initialize_update) {
      last_update_time = cur_time / (double) SDL_GetPerformanceFrequency();
      initialize_update = False;
   }

   if (cur_time / (double) SDL_GetPerformanceFrequency() >= last_update_time + 1.0f) {
      last_update_time = cur_time / (double) SDL_GetPerformanceFrequency();
      update_thread_times();
   }

   chunk_server_status[chunk_server_pos] = chunk_server_activity;
   chunk_server_pos = (chunk_server_pos+1) %32;

   for (i=0; i < 32; ++i)
      chunk_server += chunk_server_status[i] / 32.0f;

   stb_easy_font_spacing(-0.75);
   pos_y = 10;
   text_color[0] = text_color[1] = text_color[2] = 1.0f;
   print("Frame time: %6.2fms, CPU frame render time: %5.2fms , CPU tick time: %5.2fms", frame_time*1000, render_time*1000, tick_time*1000);
   print("Tris: %4.1fM drawn of %4.1fM in range", 2*quads_rendered/1000000.0f, 2*quads_considered/1000000.0f);
   print("Mesh data: requested in-cache %dMB, total in cache %dMB", mesh_cache_requested_in_use >> 20, c_mesh_cache_in_use >> 20);
   if (debug_render) {
      print("Gen chunks: %4d", num_gen_chunk_alloc);
      for (i=0; i < MAX_MESH_WORKERS; ++i) {
         static char *task[32] = { "mesh", "gencache", "heightf", "fill", "ore", "trees", "edits", "light", "physics", "scan", "idle" };
         char buffer[1024];
         int pos=0,j;
         float total=0;
         pos += sprintf(buffer+pos, "Thread %d - ", i);
         for (j=0; j < 32; ++j)
            total += thread_timing[i].time[j];
         pos += sprintf(buffer+pos, "%4dms - ", (int) (1000*total));
         for (j=0; j < 11; ++j) {
            if (0==strcmp(task[j], "ore")) continue;
            if (0==strcmp(task[j], "edits")) continue;
            if (0==strcmp(task[j], "physics")) continue;
            if (0==strcmp(task[j], "scan")) continue;
            pos += sprintf(buffer+pos, "%s:%3g/%3dms ", task[j], thread_timing[i].count[j], (int) (1000*thread_timing[i].time[j]));
         }
         print(buffer);
      }
      print("Vbuf storage: %dMB in frustum of %dMB in range of %dMB in cache", chunk_storage_rendered>>20, chunk_storage_considered>>20, chunk_storage_total>>20);
      print("Num mesh builds started this frame: %d; num uploaded this frame: %d\n", num_meshes_started, num_meshes_uploaded);
      print("QChunks: %3d in frustum of %3d valid of %3d in range", chunks_in_frustum, chunks_considered, chunk_locations);
      print("Mesh worker threads active: %d", num_threads_active);
      print("View distance: %d blocks", view_dist_for_display);
      print("x=%5.2f, y=%5.2f, z=%5.2f", obj[player_id].position.x, obj[player_id].position.y, obj[player_id].position.z);
      print("Belt sort order: %d", sort_order_for_selected_belt);
      print("%s", glGetString(GL_RENDERER));
      #if 0
      for (i=0; i < 4; ++i)
         print("[ %4d,%4d  %4d,%4d  %4d,%4d  %4d,%4d ]",
                                      physics_cache_feedback[i][0].x, physics_cache_feedback[i][0].y, 
                                      physics_cache_feedback[i][1].x, physics_cache_feedback[i][1].y, 
                                      physics_cache_feedback[i][2].x, physics_cache_feedback[i][2].y, 
                                      physics_cache_feedback[i][3].x, physics_cache_feedback[i][3].y);
      #endif
   }

   if (is_synchronous_debug) {
      text_color[0] = 1.0;
      text_color[1] = 0.5;
      text_color[2] = 0.5;
      print("SLOWNESS: Synchronous debug output is enabled!");
   }

   if (show_memory) {
      dump_memory();
   }
}

void stbgl_drawRectTCArray(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1, float i)
{
   glBegin(GL_POLYGON);
      glTexCoord3f(s0,t0,i); glVertex2f(x0,y0);
      glTexCoord3f(s1,t0,i); glVertex2f(x1,y0);
      glTexCoord3f(s1,t1,i); glVertex2f(x1,y1);
      glTexCoord3f(s0,t1,i); glVertex2f(x0,y1);
   glEnd();
}

Bool third_person;
float player_zoom = 1.0f;

vec vec_add(vec *b, vec *c)
{
   vec a;
   a.x = b->x + c->x;
   a.y = b->y + c->y;
   a.z = b->z + c->z;
   return a;
}

vec vec_sub(vec *b, vec *c)
{
   vec a;
   a.x = b->x - c->x;
   a.y = b->y - c->y;
   a.z = b->z - c->z;
   return a;
}

vec vec_add_scale(vec *b, vec *c, float d)
{
   vec a;
   a.x = b->x + d*c->x;
   a.y = b->y + d*c->y;
   a.z = b->z + d*c->z;
   return a;
}

vec vec_sub_scale(vec *b, vec *c, float d)
{
   vec a;
   a.x = b->x - d*c->x;
   a.y = b->y - d*c->y;
   a.z = b->z - d*c->z;
   return a;
}

int alpha_test_sprites=1;

void render_sprites(void)
{
   int i;
   vec s_off, t_off;
   stbglUseProgram(dumb_prog);
   
   if (alpha_test_sprites) {
      glEnable(GL_ALPHA_TEST);
      glDisable(GL_BLEND);
   } else {
      glEnable(GL_BLEND);
      glDepthMask(GL_FALSE);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   }
   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, sprite_tex);
   stbglUniform1i(stbgl_find_uniform(dumb_prog, "tex"), 0);

   objspace_to_worldspace(&s_off.x, player_id, 0.5,0,0);
   objspace_to_worldspace(&t_off.x, player_id, 0,0,0.5);

   glBegin(GL_QUADS);
   for (i=0; i < num_sprites; ++i) {
      sprite *s = &sprites[i];
      vec p0,p1,p2,p3;

      p0 = vec_add_scale(&s->pos, &s_off, s->size);
      p1 = vec_sub_scale(&s->pos, &s_off, s->size);
      p2 = vec_add_scale(&p1, &t_off, s->size);
      p3 = vec_add_scale(&p0, &t_off, s->size);
      p0 = vec_sub_scale(&p0, &t_off, s->size);
      p1 = vec_sub_scale(&p1, &t_off, s->size);
      glColor3fv(&s->color.x);
      glTexCoord3f(0,1,s->id); glVertex3fv(&p0.x);
      glTexCoord3f(1,1,s->id); glVertex3fv(&p1.x);
      glTexCoord3f(1,0,s->id); glVertex3fv(&p2.x);
      glTexCoord3f(0,0,s->id); glVertex3fv(&p3.x);
   }
   glEnd();
   glDepthMask(GL_TRUE);
   glDisable(GL_ALPHA_TEST);


   stbglUseProgram(0);
}

void render_objects(void)
{
   int i;
   vec sz;
   vec pos;
   glColor3f(1,1,1);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_BLEND);
   stbgl_drawBox(light_pos[0], light_pos[1], light_pos[2], 3,3,3, 1);

   num_sprites = 0;

   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid && (i != player_id || third_person)) {
         sz.x = camera_bounds[1][0] - camera_bounds[0][0];
         sz.y = camera_bounds[1][1] - camera_bounds[0][1];
         sz.z = camera_bounds[1][2] - camera_bounds[0][2];
         pos.x = obj[i].position.x + (camera_bounds[1][0] + camera_bounds[0][0])/2;
         pos.y = obj[i].position.y + (camera_bounds[1][1] + camera_bounds[0][1])/2;
         pos.z = obj[i].position.z + (camera_bounds[1][2] + camera_bounds[0][2])/2;
         stbgl_drawBox(pos.x,pos.y,pos.z, sz.x,sz.y,sz.z, 1);
      }
   }

   logistics_render();

   draw_pickers_flush(1.0f);

   render_sprites();
}

int face_dir[6][3] = {
   { 1,0,0 },
   { 0,1,0 },
   { -1,0,0 },
   { 0,-1,0 },
   { 0,0,1 },
   { 0,0,-1 },
};

void draw_main(void)
{
   Uint64 start_time, end_time; // render time
   glEnable(GL_CULL_FACE);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_LIGHTING);
   glEnable(GL_DEPTH_TEST);
   #ifdef REVERSE_DEPTH
   glDepthFunc(GL_GREATER);
   glClearDepth(0);
   #else
   glDepthFunc(GL_LESS);
   glClearDepth(1);
   #endif
   glDepthMask(GL_TRUE);
   glDisable(GL_SCISSOR_TEST);
   glClearColor(0.6f,0.7f,0.9f,0.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glColor3f(1,1,1);
   glFrontFace(GL_CW);
   glEnable(GL_TEXTURE_2D);
   glDisable(GL_BLEND);


   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   #ifdef REVERSE_DEPTH
   stbgl_Perspective(player_zoom, 90, 70, 3000, 1.0/16);
   #else
   stbgl_Perspective(player_zoom, 90, 70, 1.0/16, 3000);
   #endif

   // now compute where the camera should be
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   stbgl_initCamera_zup_facing_y();

   if (third_person) {
      objspace_to_worldspace(camloc, player_id, 0,-5,0);
      camloc[0] += obj[player_id].position.x;
      camloc[1] += obj[player_id].position.y;
      camloc[2] += obj[player_id].position.z;
   } else {
      camloc[0] = obj[player_id].position.x;
      camloc[1] = obj[player_id].position.y;
      camloc[2] = obj[player_id].position.z;
   }

   camang[0] = obj[player_id].ang.x;
   camang[1] = obj[player_id].ang.y;
   camang[2] = obj[player_id].ang.z;
#if 1
   glRotatef(-camang[0],1,0,0);
   glRotatef(-camang[2],0,0,1);
   glTranslatef(-camloc[0], -camloc[1], -camloc[2]);
#endif

   start_time = SDL_GetPerformanceCounter();
   render_voxel_world(camloc);

   player_zoom = 1;

   render_objects();

   end_time = SDL_GetPerformanceCounter();

   glDisable(GL_LIGHTING);

   do_ui_rendering_3d();

   if (debug_render)
      logistics_debug_render();

   render_time = (end_time - start_time) / (float) SDL_GetPerformanceFrequency();

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0,screen_x,screen_y,0);

   do_ui_rendering_2d();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   if (debug_tex) {
      stbglUseProgram(dumb_prog);
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, debug_tex);
      stbglUniform1i(stbgl_find_uniform(dumb_prog, "tex"), 0);
      glColor3f(1,1,1);
      stbgl_drawRectTCArray(0,0,512,512,0,0,1,1, 0.0);
      stbglUseProgram(0);
   }
   glDisable(GL_TEXTURE_2D);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0,screen_x/2,screen_y/2,0);
   draw_stats();

}



#pragma warning(disable:4244; disable:4305; disable:4018)

#define SCALE   2

void error(char *s)
{
   SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", s, NULL);
   exit(0);
}

SDL_mutex *logm;

void ods(char *fmt, ...)
{
   char buffer[1000];
   va_list va;

   if (logm == NULL)
      logm = SDL_CreateMutex();
   va_start(va, fmt);
   vsprintf(buffer, fmt, va);
   va_end(va);

   #ifdef _WIN32
   OutputDebugString(buffer);
   #else
   SDL_LockMutex(logm);
   SDL_Log("%s", buffer);
   SDL_UnlockMutex(logm);
   #endif
}

#define TICKS_PER_SECOND  60

static SDL_Window *window;

extern void draw_main(void);
extern void process_tick(float dt);
extern void editor_init(void);

void draw(void)
{
   draw_main();
   SDL_GL_SwapWindow(window);
}


static int initialized=0;
static float last_dt;

int screen_x,screen_y;

float carried_dt = 0;
#define TICKRATE 60

float tex2_alpha = 1.0;

int raw_level_time;

float global_timer;
int global_hack;
int quit;

int loopmode(float dt, int real, int in_client)
{
   Uint64 start_time, end_time;
   if (!initialized) return 0;

   if (!real)
      return 0;

   // don't allow more than 6 frames to update at a time
   if (dt > 0.075) dt = 0.075;

   global_timer += dt;

   carried_dt += dt;
   while (carried_dt > 1.0/TICKRATE) {
      #if 0
      if (global_hack) {
         tex2_alpha += global_hack / 60.0f;
         if (tex2_alpha < 0) tex2_alpha = 0;
         if (tex2_alpha > 1) tex2_alpha = 1;
      }
      #endif
      //update_input();
      // if the player is dead, stop the sim
      carried_dt -= 1.0/TICKRATE;
   }

   start_time = SDL_GetPerformanceCounter();
   process_tick(dt);
   end_time = SDL_GetPerformanceCounter();
   tick_time = (end_time - start_time) / (float) SDL_GetPerformanceFrequency();

   draw();

   return 0;
}

extern void process_key_down(int k, int s, SDL_Keymod mod);
void process_event(SDL_Event *e)
{
   switch (e->type) {
      case SDL_MOUSEMOTION:
         process_mouse_move(e->motion.xrel, e->motion.yrel);
         break;
      case SDL_MOUSEBUTTONDOWN:
         mouse_down(e->button.button == SDL_BUTTON_LEFT ? -1 : e->button.button == SDL_BUTTON_RIGHT ? 1 : 0);
         break;
      case SDL_MOUSEBUTTONUP:
         mouse_up();
         break;

      case SDL_QUIT:
         quit = 1;
         break;

      case SDL_WINDOWEVENT:
         switch (e->window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
               screen_x = e->window.data1;
               screen_y = e->window.data2;
               loopmode(0,1,0);
               break;
         }
         break;

      case SDL_KEYDOWN: {
         int k = e->key.keysym.sym;
         int s = e->key.keysym.scancode;
         process_key_down(k, s, SDL_GetModState());
         break;
      }
      case SDL_KEYUP: {
         int k = e->key.keysym.sym;
         int s = e->key.keysym.scancode;
         process_key_up(k,s);
         break;
      }
   }
}

static SDL_GLContext *context;

static float getTimestep(float minimum_time)
{
   float elapsedTime;
   double thisTime;
   static double lastTime = -1;
   
   if (lastTime == -1)
      lastTime = SDL_GetTicks() / 1000.0 - minimum_time;

   for(;;) {
      thisTime = SDL_GetTicks() / 1000.0;
      elapsedTime = (float) (thisTime - lastTime);
      if (elapsedTime >= minimum_time) {
         lastTime = thisTime;         
         return elapsedTime;
      }
      // @TODO: compute correct delay
      SDL_Delay(1);
   }
}

void APIENTRY gl_debug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *param)
{
   if (!stb_prefix((char *) message, "Buffer detailed info:")) {
      id=id;
   }
   ods("%s\n", message);
}

int is_synchronous_debug;
void enable_synchronous(void)
{
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
   is_synchronous_debug = 1;
}

static cur_mouse_relative = True;
void mouse_relative(Bool relative)
{
   if (relative != cur_mouse_relative) {
      SDL_SetRelativeMouseMode(relative ? SDL_TRUE : SDL_FALSE);
      cur_mouse_relative = relative;
   }
}

//extern void prepare_threads(void);

extern float compute_height_field(int x, int y);

Bool networking;

#ifndef SDL_main
#define SDL_main main
#endif

#define SERVER_PORT 4127

void *memory_mutex, *prof_mutex;

//void stbwingraph_main(void)
int SDL_main(int argc, char **argv)
{
   int server_port = SERVER_PORT;
   SDL_Init(SDL_INIT_VIDEO);
   #ifdef _NDEBUG
   SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
   #else
   SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
   #endif

   memory_mutex = SDL_CreateMutex();
   prof_mutex = SDL_CreateMutex();
   if (memory_mutex == NULL || prof_mutex == NULL) error("Couldn't create mutex");


   //client_player_input.flying = True;

   if (argc > 1 && !strcmp(argv[1], "--server")) {
      program_mode = MODE_server;
   }
   if (argc > 1 && !strcmp(argv[1], "--client"))
      program_mode = MODE_client;

   if (argc > 2 && !strcmp(argv[1], "--port")) {
      server_port = atoi(argv[2]);
   }

   //prepare_threads();

   SDL_GL_SetAttribute(SDL_GL_RED_SIZE  , 8);
   SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
   SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE , 8);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

   #ifdef GL_DEBUG
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
   #endif

   SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); // @TODO doesn't seem to be necessary
   SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

   #if 0
   screen_x = 1920;
   screen_y = 1080;
   #else
   screen_x = 1280;
   screen_y = 720;
   #endif

   if (program_mode == MODE_server) {
      screen_x = 320;
      screen_y = 200;
   }

   if (1 || program_mode != MODE_server) {
      window = SDL_CreateWindow("obbg", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                      screen_x, screen_y,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
                                );
      if (!window) error("Couldn't create window");

      context = SDL_GL_CreateContext(window);
      if (!context) error("Couldn't create context");

      SDL_GL_MakeCurrent(window, context); // is this true by default?

      #if 1
      SDL_SetRelativeMouseMode(SDL_TRUE);
      #if defined(_MSC_VER) && _MSC_VER < 1300
      // work around broken behavior in VC6 debugging
      if (IsDebuggerPresent())
         SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
      #endif
      #endif

      stbgl_initExtensions();

      #ifdef GL_DEBUG
      if (glDebugMessageCallbackARB) {
         glDebugMessageCallbackARB(gl_debug, NULL);

         enable_synchronous();
      }
      #endif

      SDL_GL_SetSwapInterval(1);
   }

   if (program_mode != MODE_single_player)
      networking = net_init(program_mode == MODE_server, server_port);

   //SDL_GL_SetSwapInterval(0);   // disable vsync
   game_init();
   render_init();

   //mesh_init();
   world_init();
   load_edits();

   initialized = 1;

   while (!quit) {
      SDL_Event e;
      while (SDL_PollEvent(&e))
         process_event(&e);

      loopmode(getTimestep(0.0166f/8), 1, 1);
   }

   if (networking)
      SDLNet_Quit();

   #ifdef _DEBUG
   //stop_manager();
   #endif

   return 0;
}
